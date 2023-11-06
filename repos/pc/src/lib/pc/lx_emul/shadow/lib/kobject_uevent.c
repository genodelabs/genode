/*
 * \brief  Replaces lib/kobject_uevent.c
 * \author Josef Soentgen
 * \date   2022-05-03
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <linux/kobject.h>

int kobject_uevent(struct kobject * kobj,enum kobject_action action)
{
	return 0;
}


int kobject_uevent_env(struct kobject * kobj,enum kobject_action action,char * envp_ext[])
{
	return 0;
}
