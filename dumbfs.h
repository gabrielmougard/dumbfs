#ifndef DUMBFS_H
#define DUMBFS_H

#define DUMBFS_MAGIC 0xDEADCELL

#define DUMBFS_SB_BLOCK_NR 0

#define DUMBFS_BLOCK_SIZE (1 << 12) /* 4 KiB */
#define DUMBFS_MAX_EXTENTS DUMBFS_BLOCK_SIZE / sizeof(struct dumbfs_extent)
#define DUMBFS_MAX_BLOCKS_PER_EXTENT 8 /* It can be ~(uint32) 0 */
#define DUMBFS_MAX_FILESIZE                                        \
	(uint64_t) DUMBFS_MAX_BLOCKS_PER_EXTENT *DUMBFS_BLOCK_SIZE \
	    *DUMBFS_MAX_EXTENTS
#define DUMBFS_FILENAME_LEN 28
#define DUMBFS_MAX_SUBFILES 128

/*
 * dumbfs partition layout
 * +---------------+
 * |  superblock   |  1 block
 * +---------------+
 * |  inode store  |  sb->nr_istore_blocks blocks
 * +---------------+
 * | ifree bitmap  |  sb->nr_ifree_blocks blocks
 * +---------------+
 * | bfree bitmap  |  sb->nr_bfree_blocks blocks
 * +---------------+
 * |    data       |
 * |      blocks   |  rest of the blocks
 * +---------------+
 */

struct dumbfs_inode {
	uint32_t i_mode;   /* File mode */
	uint32_t i_uid;	   /* Owner id */
	uint32_t i_gid;	   /* Group id */
	uint32_t i_size;   /* Size in bytes */
	uint32_t i_ctime;  /* Inode change time */
	uint32_t i_atime;  /* Access time */
	uint32_t i_mtime;  /* Modification time */
	uint32_t i_blocks; /* Block count */
	uint32_t i_nlink;  /* Hard links count */
	union {
		uint32_t
		    ei_block; /* Block with list of extents for this file */
		uint32_t
		    dir_block; /* Block with list of files for this directory */
	};
	char i_data[32]; /* store symlink content */
};

#define DUMBFS_INODES_PER_BLOCK \
	(DUMBFS_BLOCK_SIZE / sizeof(struct dumbfs_inode))

struct dumbfs_sb_info {
	uint32_t magic; /* Magic number */

	uint32_t nr_blocks; /* Total number of blocks (incl sb & inodes) */
	uint32_t nr_inodes; /* Total number of inodes */

	uint32_t nr_istore_blocks; /* Number of inode store blocks */
	uint32_t nr_ifree_blocks;  /* Number of inode free bitmap blocks */
	uint32_t nr_bfree_blocks;  /* Number of block free bitmap blocks */

	uint32_t nr_free_inodes; /* Number of free inodes */
	uint32_t nr_free_blocks; /* Number of free blocks */

#ifdef __KERNEL__
	unsigned long *ifree_bitmap; /* In-memory free inodes bitmap */
	unsigned long *bfree_bitmap; /* In-memory free blocks bitmap */
#endif
};

#ifdef __KERNEL__

struct dumbfs_inode_info {
	union {
		uint32_t
		    ei_block; /* Block with list of extents for this file */
		uint32_t
		    dir_block; /* Block with list of files for this directory */
	};
	char i_data[32];
	struct inode vfs_inode;
};

struct dumbfs_extent {
	uint32_t ee_block; /* first logical block extent covers */
	uint32_t ee_len;   /* number of blocks covered by extent */
	uint32_t ee_start; /* first physical block extent covers */
};

struct dumbfs_file_ei_block {
	struct dumbfs_extent extents[DUMBFS_MAX_EXTENTS];
};

struct dumbfs_dir_block {
	struct dumbfs_file {
		uint32_t inode;
		char filename[DUMBFS_FILENAME_LEN];
    } files[DUMBFS_MAX_SUBFILES;plefs_inode_info, vfs_inode))

/* superblock functions */
int dumbfs_fill_super(struct super_block *sb, void *data, int silent);

/* inode functions */
int dumbfs_init_inode_cache(void);
void dumbfs_destroy_inode_cache(void);
struct inode *dumbfs_iget(struct super_block *sb, unsigned long ino);

/* file functions */
extern const struct file_operations dumbfs_file_ops;
extern const struct file_operations dumbfs_dir_ops;
extern const struct address_space_operations dumbfs_aops;

/* extent functions */
extern uint32_t dumbfs_ext_search(struct dumbfs_file_ei_block *index,
                                    uint32_t iblock);

/* Getters for superbock and inode */
#define DUMBFS_SB(sb) (sb->s_fs_info)
#define DUMBFS_INODE(inode) \
    (container_of(inode, struct sim]

#endif /* __KERNEL__ */
#endif /* DUMBFS_H */
