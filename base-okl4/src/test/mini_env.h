/*
 * \brief  Minimalistic environment used for test steps
 * \author Norman Feske
 * \date   2009-03-25
 *
 * This file is not an interface but an implementation snippet. It should
 * be included only once per program because it contains the implementation
 * of the global 'env()' function.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MINI_ENV_H_
#define _MINI_ENV_H_

#include <base/allocator_avl.h>
#include <base/env.h>

/**
 * Minimalistic environment
 *
 * \param HEAP_SIZE  size of static heap in bytes
 *
 * This implementation of the Genode environment does only
 * provide a heap. It suffices to successfully initialize
 * the C++ exception handling.
 */
template <unsigned HEAP_SIZE>
class Minimal_env : public Genode::Env
{
	private:

		char                  _heap[HEAP_SIZE];
		Genode::Allocator_avl _alloc;

	public:

		/**
		 * Constructor
		 */
		Minimal_env() : _alloc(0) {
			_alloc.add_range((Genode::addr_t)_heap, HEAP_SIZE); }

		Genode::Allocator *heap() { return &_alloc; }


		/***********************************************
		 ** Dummy implementation of the Env interface **
		 ***********************************************/

		Genode::Parent      *parent()      { return 0; }
		Genode::Ram_session *ram_session() { return 0; }
		Genode::Cpu_session *cpu_session() { return 0; }
		Genode::Rm_session  *rm_session()  { return 0; }
		Genode::Pd_session  *pd_session()  { return 0; }

		Genode::Ram_session_capability ram_session_cap() {
			return Genode::Ram_session_capability(); }
		Genode::Cpu_session_capability cpu_session_cap() {
			return Genode::Cpu_session_capability(); }

		void reload_parent_cap(Genode::Capability<Genode::Parent>::Dst, long) { }
};


/**
 * Instance of the minimal environment
 */
namespace Genode {

	/**
	 * Instance of minimalistic Genode environment providing a
	 * static heap of 64KB
	 */
	Env *env()
	{
		static Minimal_env<64*1024> env;
		return &env;
	}
}

#endif /* _MINI_ENV_H_ */
