/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Josef Soentgen
 * \date   2022-04-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/slab.h>

int simple_pin_fs(struct file_system_type * type, struct vfsmount ** mount, int * count)
{
	if (!mount)
		return -EFAULT;

	if (!*mount)
		*mount = kzalloc(sizeof(struct vfsmount), GFP_KERNEL);

	if (!*mount)
		return -ENOMEM;

	if (count)
		++*count;

	return 0;
}


void simple_release_fs(struct vfsmount ** mount,int * count)
{
	kfree(*mount);
}


struct inode * alloc_anon_inode(struct super_block * s)
{
	return kzalloc(sizeof(struct inode), GFP_KERNEL);
}
