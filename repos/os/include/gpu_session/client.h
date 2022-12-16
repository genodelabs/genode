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

		bool complete(Sequence_number seqno) override {
			return call<Rpc_complete>(seqno); }

		void completion_sigh(Genode::Signal_context_capability sigh) override {
			call<Rpc_completion_sigh>(sigh); }

		Gpu::Sequence_number execute(Vram_id id, Genode::off_t offset) override {
			return call<Rpc_execute>(id, offset); }

		Genode::Dataspace_capability alloc_vram(Vram_id id, Genode::size_t size) override {
			return call<Rpc_alloc_vram>(id, size); }

		void free_vram(Vram_id id) override {
			call<Rpc_free_vram>(id); }

		Vram_capability export_vram(Vram_id id) override {
			return call<Rpc_export_vram>(id); }

		void import_vram(Vram_capability cap, Vram_id id) override {
			call<Rpc_import_vram>(cap, id); }

		Genode::Dataspace_capability map_cpu(Vram_id id, Mapping_attributes attrs) override {
			return call<Rpc_map_cpu>(id, attrs); }

		void unmap_cpu(Vram_id id) override {
			call<Rpc_unmap_cpu>(id); }

		bool map_gpu(Vram_id id, Genode::size_t size,
		             Genode::off_t offset, Virtual_address va)  override {
			return call<Rpc_map_gpu>(id, size, offset, va); }

		void unmap_gpu(Vram_id id, Genode::off_t offset, Virtual_address va) override {
			call<Rpc_unmap_gpu>(id, offset, va); }

		bool set_tiling_gpu(Vram_id id, Genode::off_t offset, unsigned mode) override {
			return call<Rpc_set_tiling_gpu>(id, offset, mode); }
};

#endif /* _INCLUDE__GPU_SESSION__CLIENT_H_ */
