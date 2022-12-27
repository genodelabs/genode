/*
 * \brief  Replaces fs/sysfs/file.c
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

#include <linux/sysfs.h>

int sysfs_create_bin_file(struct kobject * kobj,const struct bin_attribute * attr)
{
	lx_emul_trace(__func__);
	return 0;
}


void sysfs_remove_bin_file(struct kobject * kobj,const struct bin_attribute * attr)
{
	lx_emul_trace(__func__);
}


int sysfs_create_file_ns(struct kobject * kobj,const struct attribute * attr,const void * ns)
{
	lx_emul_trace(__func__);
	return 0;
}


void sysfs_remove_file_ns(struct kobject * kobj,const struct attribute * attr,const void * ns)
{
	lx_emul_trace(__func__);
}


int sysfs_create_files(struct kobject * kobj,const struct attribute * const * ptr)
{
	lx_emul_trace(__func__);
	return 0;
}


void sysfs_remove_files(struct kobject * kobj,const struct attribute * const * ptr)
{
	lx_emul_trace(__func__);
}


bool sysfs_remove_file_self(struct kobject * kobj,const struct attribute * attr)
{
	lx_emul_trace(__func__);
	return false;
}


int sysfs_emit(char * buf,const char * fmt,...)
{
	lx_emul_trace(__func__);
	return PAGE_SIZE;
}


int sysfs_emit_at(char * buf, int at, const char * fmt,...)
{
	lx_emul_trace(__func__);
	return at > PAGE_SIZE ? PAGE_SIZE : PAGE_SIZE - at;
}


void sysfs_notify(struct kobject * kobj,const char * dir,const char * attr)
{
	lx_emul_trace(__func__);
}
