/*
 * \brief  Replaces fs/sysfs/dir.c
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

#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>


int sysfs_create_dir_ns(struct kobject * kobj,const void * ns)
{
	if (!kobj)
		return -EINVAL;

	kobj->sd = kzalloc(sizeof(*kobj->sd), GFP_KERNEL);
	return 0;
}


void sysfs_remove_dir(struct kobject * kobj)
{
	if (!kobj)
		return;

	kfree(kobj->sd);
}
