/*
 * \brief  Linux-specific PD session
 * \author Norman Feske
 * \date   2012-08-15
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PD_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__PD_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/rpc_server.h>
#include <linux_pd_session/linux_pd_session.h>

/* core includes */
#include <platform_pd.h>
#include <signal_broker.h>

namespace Genode {

	class Dataspace_component;
	class Pd_session_component;
	class Parent;
}


class Genode::Pd_session_component : public Rpc_object<Linux_pd_session, Pd_session_component>
{
	private:

		enum { LABEL_MAX_LEN     = 1024 };
		enum { ROOT_PATH_MAX_LEN =  512 };

		unsigned long      _pid;
		char               _label[LABEL_MAX_LEN];
		char               _root[ROOT_PATH_MAX_LEN];
		unsigned           _uid;
		unsigned           _gid;
		Capability<Parent> _parent;
		Rpc_entrypoint    &_ds_ep;
		Signal_broker      _signal_broker;

		void _start(Dataspace_component *ds);

	public:

		/**
		 * Constructor
		 *
		 * \param ds_ep        entrypoint where the dataspaces are managed
		 * \param receiver_ep  entrypoint holding signal-receiver component
		 *                     objects
		 * \param context_ep   global pool of all signal contexts
		 * \param md_alloc     meta-data allocator
		 * \param args         additional session arguments
		 */
		Pd_session_component(Rpc_entrypoint &ds_ep,
		                     Rpc_entrypoint &receiver_ep,
		                     Rpc_entrypoint &context_ep,
		                     Allocator &md_alloc,
		                     const char *args);

		~Pd_session_component();

		/**
		 * Register quota donation at allocator guard
		 */
		void upgrade_ram_quota(size_t ram_quota) { }


		/**************************
		 ** PD session interface **
		 **************************/

		int bind_thread(Thread_capability);
		int assign_parent(Capability<Parent>);
		bool assign_pci(addr_t, uint16_t) { return false; }

		Signal_source_capability alloc_signal_source() override {
			return _signal_broker.alloc_signal_source(); }

		void free_signal_source(Signal_source_capability cap) override {
			_signal_broker.free_signal_source(cap); }

		Signal_context_capability
		alloc_context(Signal_source_capability sig_rec_cap, unsigned long imprint) override
		{
			return _signal_broker.alloc_context(sig_rec_cap, imprint);
		}

		void free_context(Signal_context_capability cap) override {
			_signal_broker.free_context(cap); }

		void submit(Signal_context_capability cap, unsigned n) override {
			_signal_broker.submit(cap, n); }


		/******************************
		 ** Linux-specific extension **
		 ******************************/

		void start(Capability<Dataspace> binary);
};

#endif /* _CORE__INCLUDE__PD_SESSION_COMPONENT_H_ */
