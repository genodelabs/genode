/*
 * \brief  Client-side Gpu session interface
 * \author Josef Soentgen
 * \date   2017-04-28
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPU_SESSION__CLIENT_H_
#define _INCLUDE__GPU_SESSION__CLIENT_H_

/* Genode includes */
#include <base/rpc_client.h>
#include <gpu_session/capability.h>

namespace Gpu { class Session_client; }


class Gpu::Session_client : public Genode::Rpc_client<Session>
{
	public:

		/**
		 * Constructor
		 *
		 * \param session  session capability
		 */
		Session_client(Session_capability session)
		: Genode::Rpc_client<Session>(session) { }

		/***********************
		 ** Session interface **
		 ***********************/

		Info info() const override {
			return call<Rpc_info>(); }

		void exec_buffer(Genode::Dataspace_capability cap, Genode::size_t size) override {
			call<Rpc_exec_buffer>(cap, size); }

		void completion_sigh(Genode::Signal_context_capability sigh) override {
			call<Rpc_completion_sigh>(sigh); }

		Genode::Dataspace_capability alloc_buffer(Genode::size_t size) override {
			return call<Rpc_alloc_buffer>(size); }

		void free_buffer(Genode::Dataspace_capability ds) override {
			call<Rpc_free_buffer>(ds); }

		Genode::Dataspace_capability map_buffer(Genode::Dataspace_capability ds,
		                                        bool aperture) override {
			return call<Rpc_map_buffer>(ds, aperture); }

		void unmap_buffer(Genode::Dataspace_capability ds) override {
			call<Rpc_unmap_buffer>(ds); }

		bool map_buffer_ppgtt(Genode::Dataspace_capability ds,
		                      Gpu::addr_t va) override {
			return call<Rpc_map_buffer_ppgtt>(ds, va); }

		void unmap_buffer_ppgtt(Genode::Dataspace_capability ds, Gpu::addr_t va) override {
			call<Rpc_unmap_buffer_ppgtt>(ds, va); }

		bool set_tiling(Genode::Dataspace_capability ds, unsigned mode) override {
			return call<Rpc_set_tiling>(ds, mode); }
};

#endif /* _INCLUDE__GPU_SESSION__CLIENT_H_ */
