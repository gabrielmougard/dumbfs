#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "dumbfs.h"

/* Mount a dumbfs partition */
struct dentry *dumbfs_mount(struct file_system_type *fs_type, int flags,
			    const char *dev_name, void *data) {
	struct dentry *dentry =
	    mount_bdev(fs_type, flags, dev_name, data, dumbfs_fill_super);
	if (IS_ERR(dentry))
		pr_err("'%s' mount failure\n", dev_name);
	else
		pr_info("'%s' mount success\n", dev_name);

	return dentry;
}

/* Unmount a dumbfs partition */
void dumbfs_kill_sb(struct super_block *sb) {
	kill_block_super(sb);

	pr_info("unmunted disk\n");
}

static struct file_system_type dumbfs_file_system_type = {
    .owner = THIS_MODULE,
    .name = "dumbfs",
    .mount = dumbfs_mount,
    .kill_sb = dumbfs_kill_sb,
    .fs_flags = FS_REQUIRES_DEV,
    .next = NULL,
};

static int __init dumbfs_init(void) {
	int ret = dumbfs_init_inode_cache();
	if (ret) {
		pr_err("inode cache creation failed\n");
		goto end;
	}

	ret = register_filesystem(&dumbfs_file_system_type);
	if (ret) {
		pr_err("register_filesystem() failed\n");
		goto end;
	}

	pr_info("module loaded\n");
end:
	return ret;
}

static void __exit dumbfs_exit(void) {
	int ret = unregister_filesystem(&dumbfs_file_system_type);
	if (ret) pr_err("unregister_filesystem() failed\n");

	dumbfs_destroy_inode_cache();

	pr_info("module unloaded\n");
}

module_init(dumbfs_init);
module_exit(dumbfs_exit);

MODULE_AUTHOR("GABRIEL MOUGARD");
MODULE_DESCRIPTION("A dumb file system");
