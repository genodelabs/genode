/*
 * \brief  Protection-domain facility
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

#include <base/allocator.h>
#include <platform_thread.h>
#include <address_space.h>

/*
 * Must be initialized by the startup code,
 * only valid in core
 */
extern Genode::addr_t __core_pd_sel;

namespace Genode {

	class Platform_thread;
	class Platform_pd : public Address_space
	{
		private:

			Native_capability _parent;
			int               _thread_cnt;
			addr_t            _pd_sel;
			const char *      _label;

		public:

			/**
			 * Constructors
			 */
			Platform_pd(Allocator * md_alloc, char const *,
			            signed pd_id = -1, bool create = true);

			/**
			 * Destructor
			 */
			~Platform_pd();

			/**
			 * Bind thread to protection domain
			 */
			bool bind_thread(Platform_thread *thread);

			/**
			 * Unbind thread from protection domain
			 *
			 * Free the thread's slot and update thread object.
			 */
			void unbind_thread(Platform_thread *thread);

			/**
			 * Assign parent interface to protection domain
			 */
			void assign_parent(Native_capability parent);

			/**
			 * Return portal capability selector for parent interface
			 */
			addr_t parent_pt_sel() { return _parent.local_name(); }

			/**
			 * Assign PD selector to PD
			 */
			void assign_pd(addr_t pd_sel) { _pd_sel = pd_sel; }

			/**
			 * Capability selector of this task.
			 *
			 * \return PD selector
			 */
			addr_t pd_sel() const { return _pd_sel; }

			/**
			 * Capability selector of core protection domain
			 *
			 * \return PD selector
			 */
			static addr_t pd_core_sel() { return __core_pd_sel; }


			/**
			 * Label of this protection domain
			 *
			 * \return name of this protection domain
			 */
			const char * name() const { return _label; }

			/*****************************
			 ** Address-space interface **
			 *****************************/

			void flush(addr_t, size_t);
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
