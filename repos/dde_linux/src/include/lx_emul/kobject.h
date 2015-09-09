/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/******************
 ** linux/kref.h **
 ******************/

struct kref { atomic_t refcount; };

void kref_init(struct kref *kref);
void kref_get(struct kref *kref);
int  kref_put(struct kref *kref, void (*release) (struct kref *kref));


/*********************
 ** linux/kobject.h **
 *********************/

struct kobject { struct kobject *parent; };
struct kobj_uevent_env
{
	char buf[32];
	int buflen;
};

struct kobj_uevent_env;

int add_uevent_var(struct kobj_uevent_env *env, const char *format, ...);
char *kobject_name(const struct kobject *kobj);
char *kobject_get_path(struct kobject *kobj, gfp_t gfp_mask);
