#ifndef _VMM_H_
#define _VMM_H_

#include <base/env.h>
#include <base/allocator.h>

Genode::Env       & genode_env();
Genode::Allocator & vmm_heap();

#endif /* _VMM_H_ */
