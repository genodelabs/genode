/*
 * \brief  Gpu session interface.
 * \author Josef Soentgen
 * \author Sebastian Sumpf
 * \date   2017-04-28
 */

/*
 * Copyright (C) 2017-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPU_SESSION__GPU_SESSION_H_
#define _INCLUDE__GPU_SESSION__GPU_SESSION_H_

#include <base/id_space.h>
#include <session/session.h>

namespace Gpu {

	using addr_t = Genode::uint64_t;

	struct Vram;
	using Vram_id_space   = Genode::Id_space<Vram>;
	using Vram_id         = Vram_id_space::Id;
	using Vram_capability = Genode::Capability<Vram>;

	/*
	 * Attributes for mapping a buffer
	 */
	struct Mapping_attributes
	{
		bool readable;
		bool writeable;

		static Mapping_attributes ro()
		{
			return { .readable = true, .writeable = false };
		}

		static Mapping_attributes rw()
		{
			return { .readable = true, .writeable = true };
		}

		static Mapping_attributes wo()
		{
			return { .readable = false, .writeable = true };
		}
	};

	struct Sequence_number;
	struct Session;
	struct Virtual_address;
}

/*
 * Execution buffer sequence number
 */
struct Gpu::Sequence_number
{
	Genode::uint64_t value;
};


struct Gpu::Virtual_address
{
	Genode::uint64_t value;
};



/*
 * Gpu session interface
 */
struct Gpu::Session : public Genode::Session
{
	struct Out_of_ram           : Genode::Exception { };
	struct Out_of_caps          : Genode::Exception { };
	struct Invalid_state        : Genode::Exception { };
	struct Conflicting_id       : Genode::Exception { };
	struct Mapping_vram_failed  : Genode::Exception { };

	enum { REQUIRED_QUOTA = 1024 * 1024, CAP_QUOTA = 32, };

	static const char *service_name() { return "Gpu"; }

	virtual ~Session() { }

	/***********************
	 ** Session interface **
	 ***********************/

	/**
	 * Get GPU information dataspace
	 */
	virtual Genode::Dataspace_capability info_dataspace() const = 0;

	/**
	 * Execute commands in vram
	 *
	 * \param id      vram id
	 * \param offset  offset in vram to start execution
	 *
	 * \return execution sequence number for complete checks
	 *
	 * \throw Invalid_state is thrown if the provided vram is not valid, e.g not mapped
	 */
	virtual Gpu::Sequence_number execute(Vram_id id, Genode::off_t offset) = 0;

	/**
	 * Check if execution has been completed
	 *
	 * \param seqno  sequence number of the execution
	 *
	 * \return true if execution has been finished, otherwise
	 *         false is returned
	 */
	virtual bool complete(Sequence_number seqno) = 0;

	/**
	 * Register completion signal handler
	 *
	 * \param sigh  signal handler that is called when the execution
	 *              has completed
	 */
	virtual void completion_sigh(Genode::Signal_context_capability sigh) = 0;

	/**
	 * Allocate video ram
	 *
	 * \param id    id to be associated with the vram
	 * \param size  size of memory in bytes
	 *
	 * \throw Out_of_ram
	 * \throw Out_of_caps
	 * \throw Conflicting_id
	 */
	virtual Genode::Dataspace_capability alloc_vram(Vram_id id, Genode::size_t size) = 0;

	/**
	 * Free vram
	 *
	 * \param id  id of vram
	 */
	virtual void free_vram(Vram_id id) = 0;

	/**
	 * Export vram dataspace from GPU session
	 *
	 * \param id  id of associated vram
	 *
	 * \return cability of exported vram
	 */
	virtual Vram_capability export_vram(Vram_id id) = 0;

	/**
	 * Import vram to GPU session
	 *
	 * \param cap  capability of vram as retrieved by 'exportvram'
	 * \param id   vram id to be associated to this vram in the session
	 *
	 * \throw Conflicting_id
	 * \throw Out_of_caps
	 * \throw Out_of_ram
	 * \throw Invalid_state  (cap is no longer valid)
	 */
	virtual void import_vram(Vram_capability cap, Vram_id id) = 0;

	/**
	 * Map vram at CPU
	 *
	 * \param id        id of vram
	 * \param attrs     specify how the buffer is mapped
	 *
	 * \throw Mapping_vram_failed
	 * \throw Out_of_caps
	 * \throw Out_of_ram
	 */
	virtual Genode::Dataspace_capability map_cpu(Vram_id id, Mapping_attributes attrs) = 0;

	/**
	 * Unmap vram
	 *
	 * \param id  id of vram
	 */
	virtual void unmap_cpu(Vram_id id) = 0;

	/**
	 * Map vram at GPU
	 *
	 * \param id    vram id
	 * \param size  size of vram to be mapped
	 * \pram offset offset in vram
	 * \param va    GPU virtual address
	 *
	 * \return true on success, false otherwise

	 * \throw Mapping_vram_failed
	 * \throw Out_of_caps
	 * \throw Out_of_ram
	 */
	virtual bool map_gpu(Vram_id id, Genode::size_t size,
	                     Genode::off_t offset,
	                     Virtual_address va) = 0;

	/**
	 * Unmap vram on GPU
	 *
	 * \param id     vram id
	 * \param offset offset in vram
	 * \param va     GPU virtual address
	 */
	virtual void unmap_gpu(Vram_id id, Genode::off_t offset,
	                       Virtual_address va) = 0;

	/**
	 * Set tiling for vram on GPU
	 *
	 * \param id     vram id
	 * \param offset offset in vram
	 * \param mode   tiling mode
	 */
	virtual bool set_tiling_gpu(Vram_id id, Genode::off_t offset, unsigned mode) = 0;

	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_info_dataspace, Genode::Dataspace_capability, info_dataspace);
	GENODE_RPC(Rpc_complete, bool, complete,
	           Gpu::Sequence_number);
	GENODE_RPC(Rpc_completion_sigh, void, completion_sigh,
	           Genode::Signal_context_capability);
	GENODE_RPC_THROW(Rpc_execute, Gpu::Sequence_number, execute,
	                 GENODE_TYPE_LIST(Invalid_state),
	                 Gpu::Vram_id, Genode::off_t);
	GENODE_RPC_THROW(Rpc_alloc_vram, Genode::Dataspace_capability, alloc_vram,
	                 GENODE_TYPE_LIST(Out_of_caps, Out_of_ram),
	                 Gpu::Vram_id, Genode::size_t);
	GENODE_RPC(Rpc_free_vram, void, free_vram, Gpu::Vram_id);
	GENODE_RPC(Rpc_export_vram, Gpu::Vram_capability, export_vram, Gpu::Vram_id);
	GENODE_RPC_THROW(Rpc_import_vram, void, import_vram,
	                 GENODE_TYPE_LIST(Out_of_caps, Out_of_ram, Conflicting_id, Invalid_state),
	                 Gpu::Vram_capability, Gpu::Vram_id);
	GENODE_RPC_THROW(Rpc_map_cpu, Genode::Dataspace_capability, map_cpu,
	                 GENODE_TYPE_LIST(Mapping_vram_failed, Out_of_caps, Out_of_ram),
	                 Gpu::Vram_id, Gpu::Mapping_attributes);
	GENODE_RPC(Rpc_unmap_cpu, void, unmap_cpu,
	           Gpu::Vram_id);
	GENODE_RPC_THROW(Rpc_map_gpu, bool, map_gpu,
	                 GENODE_TYPE_LIST(Mapping_vram_failed, Out_of_caps, Out_of_ram),
	                 Gpu::Vram_id, Genode::size_t, Genode::off_t, Gpu::Virtual_address);
	GENODE_RPC(Rpc_unmap_gpu, void, unmap_gpu,
	           Gpu::Vram_id, Genode::off_t, Gpu::Virtual_address);
	GENODE_RPC(Rpc_set_tiling_gpu, bool, set_tiling_gpu, Gpu::Vram_id, Genode::off_t, unsigned);

	GENODE_RPC_INTERFACE(Rpc_info_dataspace, Rpc_complete, Rpc_completion_sigh, Rpc_execute,
	                     Rpc_alloc_vram, Rpc_free_vram, Rpc_export_vram, Rpc_import_vram,
	                     Rpc_map_cpu, Rpc_unmap_cpu, Rpc_map_gpu, Rpc_unmap_gpu, Rpc_set_tiling_gpu);
};

#endif /* _INCLUDE__GPU_SESSION__GPU_SESSION_H_ */
