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
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/******************
 ** linux/kref.h **
 ******************/

struct kref { atomic_t refcount; };

void kref_init(struct kref *kref);
void kref_get(struct kref *kref);
int  kref_put(struct kref *kref, void (*release) (struct kref *kref));
int  kref_get_unless_zero(struct kref *kref);
int  kref_put_mutex(struct kref *kref,
                    void (*release)(struct kref *kref),
                    struct mutex *lock);


/*********************
 ** linux/kobject.h **
 *********************/

struct kobject
{
	struct kset      *kset;
	struct kobj_type *ktype;
	struct kobject   *parent;
	const char       *name;
};

struct kobj_uevent_env
{
	char buf[32];
	int buflen;
};

struct kobj_uevent_env;

int add_uevent_var(struct kobj_uevent_env *env, const char *format, ...);
void  kobject_put(struct kobject *);
const char *kobject_name(const struct kobject *kobj);
char *kobject_get_path(struct kobject *kobj, gfp_t gfp_mask);
struct kobject * kobject_create_and_add(const char *, struct kobject *);
