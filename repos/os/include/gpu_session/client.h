/*
 * \brief  Client-side Gpu session interface
 * \author Josef Soentgen
 * \date   2017-04-28
 */

/*
 * Copyright (C) 2017-2021 Genode Labs GmbH
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

		Genode::Dataspace_capability info_dataspace() const override {
			return call<Rpc_info_dataspace>(); }

		Gpu::Sequence_number exec_buffer(Buffer_id id,
		                                 Genode::size_t size) override {
			return call<Rpc_exec_buffer>(id, size); }

		void completion_sigh(Genode::Signal_context_capability sigh) override {
			call<Rpc_completion_sigh>(sigh); }

		Genode::Dataspace_capability alloc_buffer(Buffer_id id, Genode::size_t size) override {
			return call<Rpc_alloc_buffer>(id, size); }

		void free_buffer(Gpu::Buffer_id id) override {
			call<Rpc_free_buffer>(id); }

		Genode::Dataspace_capability map_buffer(Buffer_id id,
		                                        bool aperture) override {
			return call<Rpc_map_buffer>(id, aperture); }

		void unmap_buffer(Buffer_id id) override {
			call<Rpc_unmap_buffer>(id); }

		bool map_buffer_ppgtt(Buffer_id id, Gpu::addr_t va) override {
			return call<Rpc_map_buffer_ppgtt>(id, va); }

		void unmap_buffer_ppgtt(Buffer_id id, Gpu::addr_t va) override {
			call<Rpc_unmap_buffer_ppgtt>(id, va); }

		bool set_tiling(Buffer_id id, unsigned mode) override {
			return call<Rpc_set_tiling>(id, mode); }
};

#endif /* _INCLUDE__GPU_SESSION__CLIENT_H_ */
