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
 ** linux/init.h **
 ******************/

#define __init
#define __exit
#define __devinitconst

#define _SETUP_CONCAT(a, b) __##a##b
#define SETUP_CONCAT(a, b) _SETUP_CONCAT(a, b)
#define __setup(str, fn) static void SETUP_CONCAT(fn, SETUP_SUFFIX)(void *addrs) { fn(addrs); }

#define  core_initcall(fn)   void core_##fn(void) { fn(); }
#define subsys_initcall(fn) void subsys_##fn(void) { fn(); }
#define pure_initcall(fd) void pure_##fn(void) { printk("PURE_INITCALL"); fn(); }


/********************
 ** linux/module.h **
 ********************/

#define EXPORT_SYMBOL(x)
#define EXPORT_SYMBOL_GPL(x)
#define MODULE_LICENSE(x)
#define MODULE_NAME_LEN (64 - sizeof(long))
#define MODULE_ALIAS(name)
#define MODULE_AUTHOR(name)
#define MODULE_DESCRIPTION(desc)
#define MODULE_VERSION(version)
#define THIS_MODULE 0
#define MODULE_FIRMWARE(_firmware)
#define MODULE_DEVICE_TABLE(type, name)


struct module;
#define module_init(fn) int module_##fn(void) { return fn(); }
#define module_exit(fn) void module_exit_##fn(void) { fn(); }
void module_put_and_exit(int);

void module_put(struct module *);
void __module_get(struct module *module);
int try_module_get(struct module *);


/*************************
 ** linux/moduleparam.h **
 *************************/

#define module_param(name, type, perm)
#define module_param_named(name, value, type, perm)

#define module_param_unsafe(name, type, perm)
#define module_param_named_unsafe(name, value, type, perm)
#define MODULE_PARM_DESC(_parm, desc)
#define kparam_block_sysfs_write(name)
#define kparam_unblock_sysfs_write(name)
