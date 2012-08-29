/*
 * \brief  Protection-domain facility
 * \author Martin Stein
 * \date   2010-09-13
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__INCLUDE__PLATFORM_PD_H_
#define _SRC__CORE__INCLUDE__PLATFORM_PD_H_

/* core includes */
#include <platform_thread.h>
#include <base/thread.h>
#include <kernel/config.h>

namespace Genode {

	class Platform_pd;

	typedef Id_allocator<Platform_pd, Native_process_id, Cpu::BYTE_WIDTH> Pid_allocator;

	Pid_allocator *pid_allocator();


	class Platform_thread;
	class Platform_pd
	{
	public:

		typedef unsigned             Context_id;
		typedef Thread_base::Context Context;

		private:

			addr_t context_area_base() {
				return Native_config::context_area_virtual_base();
			}
			addr_t context_area_size() {
				return Native_config::context_area_virtual_size();
			}
			addr_t context_area_top() {
				return context_area_base() + context_area_size();
			}
			addr_t context_size() {
				return Native_config::context_virtual_size();
			}
			addr_t context_base_mask() {
			 	return ~(Native_config::context_virtual_size() - 1);
			}
			addr_t context_offset_mask() {
				return ~context_base_mask();
			}
			addr_t max_context_id {
				return context_area_size()/context_size()-1;
			}

			Native_process_id _pid;

			Native_thread_id owner_tid_by_context_id[max_context_id()+1];

			void _free_context(Native_thread_id const & t)
			{
				for (Context_id cid = 0; cid <= max_context_id(); cid++) {
					if (owner_tid_by_context_id[cid] == t) {
						owner_tid_by_context_id[cid] = 0;
					}
				}
			}


		public:

			/**
			 * Constructors
			 */
			Platform_pd(signed pid = 0, bool create = true) : _pid(pid)
			{
				static bool const verbose = false;

				if ((unsigned)User::MAX_THREAD_ID>(unsigned)max_context_id()) {
					PERR("More threads allowed than context areas available");
					return;
				}
				if (!_pid)
					_pid=pid_allocator()->allocate(this);

				if (!_pid) {
					PERR("Allocating new Process ID failed");
					return;
				}
				if (verbose)
					PDBG("Create protection domain %i", (unsigned int)_pid);
			}

			/**
			 * Destructor
			 */
			~Platform_pd() { }

			enum Context_part{ NO_CONTEXT_PART = 0,
				MISC_AREA       = 1,
				UTCB_AREA       = 2,
				STACK_AREA      = 3 };

			bool cid_if_context_address(addr_t a, Context_id* cid)
			{
				if (a < context_area_base() || a >= context_area_top())
					return false;

				addr_t context_base = a & context_base_mask();
				*cid = (Context_id)((context_base-context_area_base()) / context_size());
				return true;
			}

			Context *context(Context_id i)
			{
				return (Context*)(context_area_base()+(i+1)*context_size()-sizeof(Context));
			}

			Context *context_by_tid(Native_thread_id tid)
			{
				for (unsigned cid = 0; cid <= max_context_id(); cid++)
					if (owner_tid_by_context_id[cid] == tid)
						return context(cid);

				return 0;
			}

			bool metadata_if_context_address(addr_t a, Native_thread_id *context_owner_tid,
			                                 Context_part *cp, unsigned *stack_offset)
			{
				Context_id cid;
				if (!cid_if_context_address(a, &cid))
					return false;

				if (cid > max_context_id()) {
					PERR("Context ID %i out of range", (unsigned int)cid);
					return false;
				}

				*context_owner_tid = owner_tid_by_context_id[cid];
				if (!*context_owner_tid) {
					if (_pid == Roottask::PROTECTION_ID)
						PERR("Context %p is not in use", (void*)a);

					return false;
				}

				addr_t offset    = a & CONTEXT_OFFSET_MASK;
				Context *context = (Context *)(context_size() - sizeof(Context));

				if ((void*)offset >= &context->utcb) {
					*cp = UTCB_AREA;

				} else if ((void*)offset < &context->stack) {
					*cp           = STACK_AREA;
					*stack_offset = (((unsigned)&(context->stack)) - (unsigned)offset);
				} else {
					*cp = MISC_AREA;
				}
				return true;
			}

			bool allocate_context(Native_thread_id tid, Context_id cid)
			{
				static bool const verbose = false;

				if (cid > max_context_id())
					return 0;

				if (owner_tid_by_context_id[cid]){
					PERR("Context is already in use");
					return false;
				}
				owner_tid_by_context_id[cid] = tid;
				if (verbose)
					PDBG("Thread %i owns Context %i (0x%p) of PD %i",
					     tid, cid, context(cid), _pid);
				return true;
			}

			Context *allocate_context(Native_thread_id tid)
			{
				static bool const verbose = false;

				/*
				 * First thread is assumed to be the main thread and gets last
				 * context-area by convention
				 */
				if (!owner_tid_by_context_id[max_context_id()]){
					owner_tid_by_context_id[max_context_id()] = tid;
					if (verbose)
						PDBG("Thread %i owns Context %i (0x%p) of Protection Domain %i",
						     tid, max_context_id(), context(max_context_id()), _pid);

					return context(max_context_id());
				}

				for (unsigned i = 0; i <= max_context_id() - 1; i++) {
					if (!owner_tid_by_context_id[i]) {
						owner_tid_by_context_id[i] = tid;
						if (verbose)
							PDBG("Thread %i owns Context %i (0x%p) of Protection Domain %i",
							     tid, max_context_id(), context(max_context_id()), _pid);
						return context(i);
					}
				}
				return 0;
			}

			/**
			 * Bind thread to protection domain
			 *
			 * \return  0  on success
			 */
			inline int bind_thread(Platform_thread* pt)
			{
				Context *context = allocate_context(pt->tid());
				if (!context) {
					PERR("Context allocation failed");
					return -1;
				}
				Native_utcb *utcb = &context->utcb;
				pt->_assign_physical_thread(_pid, utcb, this);
				return 0;
			}

			/**
			 * Unbind thread from protection domain
			 *
			 * Free the thread's slot and update thread object.
			 */
			inline void unbind_thread(Platform_thread *pt)
			{
				_free_context(pt->tid());
			}

			/**
			 * Free a context so it is allocatable again
			 * \param  c  PD wide unique context ID
			 */
			void free_context(Context_id const & c)
			{
				if (c > max_context_id()) { return; }
				owner_tid_by_context_id[c] = Kernel::INVALID_THREAD_ID;
			}

			/**
			 * Assign parent interface to protection domain
			 */
			inline int assign_parent(Native_capability parent) { return 0; }
	};
}

#endif /* _SRC__CORE__INCLUDE__PLATFORM_PD_H */



