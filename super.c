#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/statfs.h>

#include "dumbfs.h"

static struct kmem_cache *dumbfs_inode_cache;

int dumbfs_init_inode_cache(void) {
	dumbfs_inode_cache = kmem_cache_create(
	    "dumbfs_cache", sizeof(struct dumbfs_inode_info), 0, 0, NULL);
	if (!dumbfs_inode_cache) return -ENOMEM;
	return 0;
}

void dumbfs_destroy_inode_cache(void) {
	kmem_cache_destroy(dumbfs_inode_cache);
}

static struct inode *dumbfs_alloc_inode(struct super_block *sb) {
	struct dumbfs_inode_info *ci =
	    kmem_cache_alloc(dumbfs_inode_cache, GFP_KERNEL);
	if (!ci) return NULL;

	inode_init_once(&ci->vfs_inode);
	return &ci->vfs_inode;
}

static void dumbfs_destroy_inode(struct inode *inode) {
	struct dumbfs_inode_info *ci = DUMBFS_INODE(inode);
	kmem_cache_free(dumbfs_inode_cache, ci);
}

static int dumbfs_write_inode(struct inode *inode,
			      struct writeback_control *wbc) {
	struct dumbfs_inode *disk_inode;
	struct dumbfs_inode_info *ci = DUMBFS_INODE(inode);
	struct super_block *sb = inode->i_sb;
	struct dumbfs_sb_info *sbi = DUMBFS_SB(sb);
	struct buffer_head *bh;
	uint32_t ino = inode->i_ino;
	uint32_t inode_block = (ino / DUMBFS_INODES_PER_BLOCK) + 1;
	uint32_t inode_shift = ino % DUMBFS_INODES_PER_BLOCK;

	if (ino >= sbi->nr_inodes) return 0;

	bh = sb_bread(sb, inode_block);
	if (!bh) return -EIO;

	disk_inode = (struct dumbfs_inode *)bh->b_data;
	disk_inode += inode_shift;

	/* update the mode using what the generic inode has */
	disk_inode->i_mode = inode->i_mode;
	disk_inode->i_uid = i_uid_read(inode);
	disk_inode->i_gid = i_gid_read(inode);
	disk_inode->i_size = inode->i_size;
	disk_inode->i_ctime = inode->i_ctime.tv_sec;
	disk_inode->i_atime = inode->i_atime.tv_sec;
	disk_inode->i_mtime = inode->i_mtime.tv_sec;
	disk_inode->i_blocks = inode->i_blocks;
	disk_inode->i_nlink = inode->i_nlink;
	disk_inode->ei_block = ci->ei_block;
	strncpy(disk_inode->i_data, ci->i_data, sizeof(ci->i_data));

	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);

	return 0;
}

static void dumbfs_put_super(struct super_block *sb) {
	struct dumbfs_sb_info *sbi = DUMBFS_SB(sb);
	if (sbi) {
		kfree(sbi->ifree_bitmap);
		kfree(sbi->bfree_bitmap);
		kfree(sbi);
	}
}

static int dumbfs_sync_fs(struct super_block *sb, int wait) {
	struct dumbfs_sb_info *sbi = DUMBFS_SB(sb);
	struct dumbfs_sb_info *disk_sb;
	int i;

	/* Flush superblock */
	struct buffer_head *bh = sb_bread(sb, 0);
	if (!bh) return -EIO;

	disk_sb = (struct dumbfs_sb_info *)bh->b_data;

	disk_sb->nr_blocks = sbi->nr_blocks;
	disk_sb->nr_inodes = sbi->nr_inodes;
	disk_sb->nr_istore_blocks = sbi->nr_istore_blocks;
	disk_sb->nr_ifree_blocks = sbi->nr_ifree_blocks;
	disk_sb->nr_bfree_blocks = sbi->nr_bfree_blocks;
	disk_sb->nr_free_inodes = sbi->nr_free_inodes;
	disk_sb->nr_free_blocks = sbi->nr_free_blocks;

	mark_buffer_dirty(bh);
	if (wait) sync_dirty_buffer(bh);
	brelse(bh);

	/* Flush free inodes bitmask */
	for (i = 0; i < sbi->nr_ifree_blocks; i++) {
		int idx = sbi->nr_istore_blocks + i + 1;

		bh = sb_bread(sb, idx);
		if (!bh) return -EIO;

		memcpy(bh->b_data,
		       (void *)sbi->ifree_bitmap + i * DUMBFS_BLOCK_SIZE,
		       DUMBFS_BLOCK_SIZE);

		mark_buffer_dirty(bh);
		if (wait) sync_dirty_buffer(bh);
		brelse(bh);
	}

	/* Flush free blocks bitmask */
	for (i = 0; i < sbi->nr_bfree_blocks; i++) {
		int idx = sbi->nr_istore_blocks + sbi->nr_ifree_blocks + i + 1;

		bh = sb_bread(sb, idx);
		if (!bh) return -EIO;

		memcpy(bh->b_data,
		       (void *)sbi->bfree_bitmap + i * DUMBFS_BLOCK_SIZE,
		       DUMBFS_BLOCK_SIZE);

		mark_buffer_dirty(bh);
		if (wait) sync_dirty_buffer(bh);
		brelse(bh);
	}

	return 0;
}

static int dumbfs_statfs(struct dentry *dentry, struct kstatfs *stat) {
	struct super_block *sb = dentry->d_sb;
	struct dumbfs_sb_info *sbi = DUMBFS_SB(sb);

	stat->f_type = DUMBFS_MAGIC;
	stat->f_bsize = DUMBFS_BLOCK_SIZE;
	stat->f_blocks = sbi->nr_blocks;
	stat->f_bfree = sbi->nr_free_blocks;
	stat->f_bavail = sbi->nr_free_blocks;
	stat->f_files = sbi->nr_inodes - sbi->nr_free_inodes;
	stat->f_ffree = sbi->nr_free_inodes;
	stat->f_namelen = DUMBFS_FILENAME_LEN;

	return 0;
}

static struct super_operations dumbfs_super_ops = {
    .put_super = dumbfs_put_super,
    .alloc_inode = dumbfs_alloc_inode,
    .destroy_inode = dumbfs_destroy_inode,
    .write_inode = dumbfs_write_inode,
    .sync_fs = dumbfs_sync_fs,
    .statfs = dumbfs_statfs,
};

/* Fill the struct superblock from partition superblock */
int dumbfs_fill_super(struct super_block *sb, void *data, int silent) {
	struct buffer_head *bh = NULL;
	struct dumbfs_sb_info *csb = NULL;
	struct dumbfs_sb_info *sbi = NULL;
	struct inode *root_inode = NULL;
	int ret = 0, i;

	/* Init sb */
	sb->s_magic = DUMBFS_MAGIC;
	sb_set_blocksize(sb, DUMBFS_BLOCK_SIZE);
	sb->s_maxbytes = DUMBFS_MAX_FILESIZE;
	sb->s_op = &dumbfs_super_ops;

	/* Read sb from disk */
	bh = sb_bread(sb, DUMBFS_SB_BLOCK_NR);
	if (!bh) return -EIO;

	csb = (struct dumbfs_sb_info *)bh->b_data;

	/* Check magic number */
	if (csb->magic != sb->s_magic) {
		pr_err("Wrong magic number\n");
		ret = -EINVAL;
		goto release;
	}

	/* Alloc sb_info */
	sbi = kzalloc(sizeof(struct dumbfs_sb_info), GFP_KERNEL);
	if (!sbi) {
		ret = -ENOMEM;
		goto release;
	}

	sbi->nr_blocks = csb->nr_blocks;
	sbi->nr_inodes = csb->nr_inodes;
	sbi->nr_istore_blocks = csb->nr_istore_blocks;
	sbi->nr_ifree_blocks = csb->nr_ifree_blocks;
	sbi->nr_bfree_blocks = csb->nr_bfree_blocks;
	sbi->nr_free_inodes = csb->nr_free_inodes;
	sbi->nr_free_blocks = csb->nr_free_blocks;
	sb->s_fs_info = sbi;

	brelse(bh);

	/* Alloc and copy ifree_bitmap */
	sbi->ifree_bitmap =
	    kzalloc(sbi->nr_ifree_blocks * DUMBFS_BLOCK_SIZE, GFP_KERNEL);
	if (!sbi->ifree_bitmap) {
		ret = -ENOMEM;
		goto free_sbi;
	}

	for (i = 0; i < sbi->nr_ifree_blocks; i++) {
		int idx = sbi->nr_istore_blocks + i + 1;

		bh = sb_bread(sb, idx);
		if (!bh) {
			ret = -EIO;
			goto free_ifree;
		}

		memcpy((void *)sbi->ifree_bitmap + i * DUMBFS_BLOCK_SIZE,
		       bh->b_data, DUMBFS_BLOCK_SIZE);

		brelse(bh);
	}

	/* Alloc and copy bfree_bitmap */
	sbi->bfree_bitmap =
	    kzalloc(sbi->nr_bfree_blocks * DUMBFS_BLOCK_SIZE, GFP_KERNEL);
	if (!sbi->bfree_bitmap) {
		ret = -ENOMEM;
		goto free_ifree;
	}

	for (i = 0; i < sbi->nr_bfree_blocks; i++) {
		int idx = sbi->nr_istore_blocks + sbi->nr_ifree_blocks + i + 1;

		bh = sb_bread(sb, idx);
		if (!bh) {
			ret = -EIO;
			goto free_bfree;
		}

		memcpy((void *)sbi->bfree_bitmap + i * DUMBFS_BLOCK_SIZE,
		       bh->b_data, DUMBFS_BLOCK_SIZE);

		brelse(bh);
	}

	/* Create root inode */
	root_inode = dumbfs_iget(sb, 0);
	if (IS_ERR(root_inode)) {
		ret = PTR_ERR(root_inode);
		goto free_bfree;
	}
	inode_init_owner(root_inode, NULL, root_inode->i_mode);
	sb->s_root = d_make_root(root_inode);
	if (!sb->s_root) {
		ret = -ENOMEM;
		goto iput;
	}

	return 0;

iput:
	iput(root_inode);
free_bfree:
	kfree(sbi->bfree_bitmap);
free_ifree:
	kfree(sbi->ifree_bitmap);
free_sbi:
	kfree(sbi);
release:
	brelse(bh);

	return ret;
}
