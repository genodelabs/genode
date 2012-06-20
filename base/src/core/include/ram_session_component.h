/*
 * \brief  Core-specific instance of the RAM session interface
 * \author Norman Feske
 * \date   2006-06-19
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__RAM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__RAM_SESSION_COMPONENT_H_

/* Genode includes */
#include <util/list.h>
#include <base/tslab.h>
#include <base/rpc_server.h>
#include <base/allocator_guard.h>

/* core includes */
#include <dataspace_component.h>

namespace Genode {

	class Ram_session_component;
	typedef List<Ram_session_component> Ram_ref_account_members;

	class Ram_session_component : public Rpc_object<Ram_session>,
	                              public Ram_ref_account_members::Element,
	                              public Dataspace_owner
	{
		private:

			enum { SBS = 1024 };   /* slab block size */

			typedef Tslab<Dataspace_component, SBS> Ds_slab;

			Rpc_entrypoint         *_ds_ep;
			Rpc_entrypoint         *_ram_session_ep;
			Range_allocator        *_ram_alloc;
			size_t                  _quota_limit;
			size_t                  _payload;      /* quota used for payload      */
			Allocator_guard         _md_alloc;     /* guarded meta-data allocator */
			Ds_slab                 _ds_slab;      /* meta-data allocator         */
			Ram_session_component  *_ref_account;  /* reference ram session       */

			enum { MAX_LABEL_LEN = 64 };
			char _label[MAX_LABEL_LEN];

			/**
			 * List of RAM sessions that use us as their reference account
			 */
			Ram_ref_account_members _ref_members;
			Lock                    _ref_members_lock;  /* protect '_ref_members' */

			/**
			 * Register RAM session to use us as reference account
			 */
			void _register_ref_account_member(Ram_session_component *new_member);

			/**
			 * Dissolve reference-account relationship of a member account
			 */
			void _remove_ref_account_member(Ram_session_component *member);
			void _unsynchronized_remove_ref_account_member(Ram_session_component *member);

			/**
			 * Return portion of RAM quota that is currently in use
			 */
			size_t used_quota() {
				return _ds_slab.consumed() + _payload + sizeof(*this); }

			/**
			 * Free dataspace
			 */
			void _free_ds(Dataspace_component *ds);

			/**
			 * Transfer quota to another RAM session
			 */
			int _transfer_quota(Ram_session_component *dst, size_t amount);


			/********************************************
			 ** Platform-implemented support functions **
			 ********************************************/

			/**
			 * Export RAM dataspace as shared memory block
			 */
			void _export_ram_ds(Dataspace_component *ds);

			/**
			 * Revert export of RAM dataspace
			 */
			void _revoke_ram_ds(Dataspace_component *ds);

			/**
			 * Zero-out content of dataspace
			 */
			void _clear_ds(Dataspace_component *ds);

		public:

			/**
			 * Constructor
			 *
			 * \param ds_ep           server entry point to manage the
			 *                        dataspaces created by the Ram session
			 * \param ram_session_ep  entry point that manages Ram sessions,
			 *                        used for looking up another ram session
			 *                        in transfer_quota()
			 * \param ram_alloc       memory pool to manage
			 * \param md_alloc        meta-data allocator
			 * \param md_ram_quota    limit of meta-data backing store
			 * \param quota_limit     initial quota limit
			 *
			 * The 'quota_limit' parameter is only used for the very
			 * first ram session in the system. All other ram session
			 * load their quota via 'transfer_quota'.
			 */
			Ram_session_component(Rpc_entrypoint  *ds_ep,
			                      Rpc_entrypoint  *ram_session_ep,
			                      Range_allocator *ram_alloc,
			                      Allocator       *md_alloc,
			                      const char      *args,
			                      size_t           quota_limit = 0);

			/**
			 * Destructor
			 */
			~Ram_session_component();

			/**
			 * Accessors
			 */
			Ram_session_component *ref_account() { return _ref_account; }

			/**
			 * Register quota donation at allocator guard
			 */
			void upgrade_ram_quota(size_t ram_quota) { _md_alloc.upgrade(ram_quota); }


			/***************************
			 ** RAM Session interface **
			 ***************************/

			Ram_dataspace_capability alloc(size_t, bool);
			void free(Ram_dataspace_capability);
			int ref_account(Ram_session_capability);
			int transfer_quota(Ram_session_capability, size_t);
			size_t quota() { return _quota_limit; }
			size_t used()  { return _payload; }
	};
}

#endif /* _CORE__INCLUDE__RAM_SESSION_COMPONENT_H_ */
