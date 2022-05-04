#ifndef _LX_EMUL__SHADOW__LINUX__MODULEPARAM_H_
#define _LX_EMUL__SHADOW__LINUX__MODULEPARAM_H_

#include_next<linux/moduleparam.h>

#undef module_param

#define module_param(name, type, perm) \
	typeof(name) *module_param_##name(void) { return &name; }


#endif /* _LX_EMUL__SHADOW__LINUX__MODULEPARAM_H_ */

