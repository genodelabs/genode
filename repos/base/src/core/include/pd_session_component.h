/*
 * \brief  Core-specific instance of the PD session interface
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PD_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__PD_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator_guard.h>
#include <base/session_label.h>
#include <base/rpc_server.h>
#include <pd_session/pd_session.h>
#include <util/arg_string.h>

/* base-internal includes */
#include <base/internal/stack_area.h>

/* core includes */
#include <platform_pd.h>
#include <signal_broker.h>
#include <rpc_cap_factory.h>
#include <native_pd_component.h>
#include <region_map_component.h>
#include <platform_generic.h>

namespace Genode { class Pd_session_component; }


class Genode::Pd_session_component : public Rpc_object<Pd_session>
{
	private:

		/**
		 * Read and store the PD label
		 */
		struct Label {

			enum { MAX_LEN = 64 };
			char string[MAX_LEN];

			Label(char const *args)
			{
				Arg_string::find_arg(args, "label").string(string,
				                                           sizeof(string), "");
			}
		} const _label;

		Allocator_guard     _md_alloc;   /* guarded meta-data allocator */
		Platform_pd         _pd;
		Capability<Parent>  _parent;
		Rpc_entrypoint     &_thread_ep;
		Pager_entrypoint   &_pager_ep;
		Signal_broker       _signal_broker;
		Rpc_cap_factory     _rpc_cap_factory;
		Native_pd_component _native_pd;

		Region_map_component _address_space;
		Region_map_component _stack_area;
		Region_map_component _linker_area;

		size_t _ram_quota(char const * args) {
			return Arg_string::find_arg(args, "ram_quota").long_value(0); }

		friend class Native_pd_component;

	public:

		/**
		 * Constructor
		 *
		 * \param receiver_ep  entrypoint holding signal-receiver component
		 *                     objects
		 * \param context_ep   global pool of all signal contexts
		 * \param md_alloc     backing-store allocator for
		 *                     signal-context component objects
		 *
		 * To maintain proper synchronization, 'receiver_ep' must be
		 * the same entrypoint as used for the signal-session component.
		 * The 'signal_context_ep' is only used for associative array
		 * to map signal-context capabilities to 'Signal_context_component'
		 * objects and as capability allocator for such objects.
		 */
		Pd_session_component(Rpc_entrypoint   &thread_ep,
		                     Rpc_entrypoint   &receiver_ep,
		                     Rpc_entrypoint   &context_ep,
		                     Allocator        &md_alloc,
		                     Pager_entrypoint &pager_ep,
		                     char const       *args)
		:
			_label(args),
			_md_alloc(&md_alloc, _ram_quota(args)),
			_pd(&_md_alloc, _label.string),
			_thread_ep(thread_ep), _pager_ep(pager_ep),
			_signal_broker(_md_alloc, receiver_ep, context_ep),
			_rpc_cap_factory(_md_alloc),
			_native_pd(*this, args),
			_address_space(thread_ep, _md_alloc, pager_ep,
			               platform()->vm_start(), platform()->vm_size()),
			_stack_area(thread_ep, _md_alloc, pager_ep, 0, stack_area_virtual_size()),
			_linker_area(thread_ep, _md_alloc, pager_ep, 0, LINKER_AREA_SIZE)
		{ }

		/**
		 * Register quota donation at allocator guard
		 */
		void upgrade_ram_quota(size_t ram_quota);

		/**
		 * Associate thread with PD
		 *
		 * \return true on success
		 *
		 * This function may fail for platform-specific reasons such as a
		 * limit on the number of threads per protection domain or a limited
		 * thread ID namespace.
		 */
		bool bind_thread(Platform_thread &thread)
		{
			return _pd.bind_thread(&thread);
		}

		Region_map_component &address_space_region_map()
		{
			return _address_space;
		}

		Session_label label() { return Session_label(_label.string); }


		/**************************
		 ** PD session interface **
		 **************************/

		void assign_parent(Capability<Parent>) override;
		bool assign_pci(addr_t, uint16_t) override;

		Signal_source_capability alloc_signal_source() override
		{
			try {
				return _signal_broker.alloc_signal_source(); }
			catch (Genode::Allocator::Out_of_memory) {
				throw Pd_session::Out_of_metadata(); }
		}

		void free_signal_source(Signal_source_capability sig_rec_cap) override {
			_signal_broker.free_signal_source(sig_rec_cap); }

		Signal_context_capability
		alloc_context(Signal_source_capability sig_rec_cap, unsigned long imprint) override
		{
			try {
				return _signal_broker.alloc_context(sig_rec_cap, imprint); }
			catch (Genode::Allocator::Out_of_memory) {
				throw Pd_session::Out_of_metadata(); }
			catch (Signal_broker::Invalid_signal_source) {
				throw Pd_session::Invalid_signal_source(); }
		}

		void free_context(Signal_context_capability cap) override {
			_signal_broker.free_context(cap); }

		void submit(Signal_context_capability cap, unsigned n) override {
			_signal_broker.submit(cap, n); }

		Native_capability alloc_rpc_cap(Native_capability ep) override
		{
			try {
				return _rpc_cap_factory.alloc(ep); }
			catch (Genode::Allocator::Out_of_memory) {
				throw Pd_session::Out_of_metadata(); }
		}

		void free_rpc_cap(Native_capability cap) override {
			_rpc_cap_factory.free(cap); }

		Capability<Region_map> address_space() {
			return _address_space.cap(); }

		Capability<Region_map> stack_area() {
			return _stack_area.cap(); }

		Capability<Region_map> linker_area() {
			return _linker_area.cap(); }

		Capability<Native_pd> native_pd() { return _native_pd.cap(); }
};

#endif /* _CORE__INCLUDE__PD_SESSION_COMPONENT_H_ */
