/*
 * \brief  Gpu session interface.
 * \author Josef Soentgen
 * \date   2017-04-28
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
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

	struct Buffer;
	using Buffer_id = Genode::Id_space<Buffer>::Id;

	struct Info;
	struct Session;
}


/*
 * Gpu information
 *
 * Used to query information in the DRM backend
 */
struct Gpu::Info
{
	using Chip_id    = Genode::uint16_t;
	using Features   = Genode::uint32_t;
	using size_t     = Genode::size_t;
	using Context_id = Genode::uint32_t;

	Chip_id    chip_id;
	Features   features;
	size_t     aperture_size;
	Context_id ctx_id;

	struct Execution_buffer_sequence {
		Genode::uint64_t id;
	} last_completed;

	struct Revision      { Genode::uint8_t value; } revision;
	struct Slice_mask    { unsigned value; }        slice_mask;
	struct Subslice_mask { unsigned value; }        subslice_mask;
	struct Eu_total      { unsigned value; }        eus;
	struct Subslices     { unsigned value; }        subslices;

	Info(Chip_id chip_id, Features features, size_t aperture_size,
	     Context_id ctx_id, Execution_buffer_sequence last,
	     Revision rev, Slice_mask s_mask, Subslice_mask ss_mask,
	     Eu_total eu, Subslices subslice)
	:
		chip_id(chip_id), features(features),
		aperture_size(aperture_size), ctx_id(ctx_id),
		last_completed(last),
		revision(rev),
		slice_mask(s_mask),
		subslice_mask(ss_mask),
		eus(eu),
		subslices(subslice)
	{ }
};


/*
 * Gpu session interface
 */
struct Gpu::Session : public Genode::Session
{
	struct Out_of_ram     : Genode::Exception { };
	struct Out_of_caps    : Genode::Exception { };
	struct Invalid_state  : Genode::Exception { };
	struct Conflicting_id : Genode::Exception { };
	struct Mapping_buffer_failed  : Genode::Exception { };

	enum { REQUIRED_QUOTA = 1024 * 1024, CAP_QUOTA = 8, };

	static const char *service_name() { return "Gpu"; }

	virtual ~Session() { }

	/***********************
	 ** Session interface **
	 ***********************/

	/**
	 * Query GPU information
	 */
	virtual Info info() const = 0;

	/**
	 * Execute commands from given buffer
	 *
	 * \param id    buffer id
	 * \param size  size of the batch buffer in bytes
	 *
	 * \return execution buffer sequence number for complete checks
	 *
	 * \throw Invalid_state is thrown if the provided buffer is not valid, e.g not mapped
	 */
	virtual Gpu::Info::Execution_buffer_sequence exec_buffer(Buffer_id id, Genode::size_t size) = 0;

	/**
	 * Register completion signal handler
	 *
	 * \param sigh  signal handler that is called when the execution
	 *              has completed
	 */
	virtual void completion_sigh(Genode::Signal_context_capability sigh) = 0;

	/**
	 * Allocate buffer dataspace
	 *
	 * \param id    buffer id to be associated with the buffer
	 * \param size  size of buffer in bytes
	 *
	 * \throw Out_of_ram
	 * \throw Out_of_caps
	 * \throw Conflicting_id
	 */
	virtual Genode::Dataspace_capability alloc_buffer(Buffer_id id, Genode::size_t size) = 0;

	/**
	 * Free buffer dataspace
	 *
	 * \param ds  dataspace capability for buffer
	 */
	virtual void free_buffer(Buffer_id id) = 0;

	/**
	 * Map buffer
	 *
	 * \param id        buffer id
	 * \param aperture  if true create CPU accessible mapping through
	 *                  GGTT window, otherwise create PPGTT mapping
	 *
	 * \throw Mapping_buffer_failed
	 */
	virtual Genode::Dataspace_capability map_buffer(Buffer_id id,
	                                                bool aperture) = 0;

	/**
	 * Unmap buffer
	 *
	 * \param id  buffer id
	 */
	virtual void unmap_buffer(Buffer_id id) = 0;

	/**
	 * Map buffer in PPGTT
	 *
	 * \param id  buffer id
	 * \param va  virtual address
	 *
	 * \throw Mapping_buffer_failed
	 * \throw Out_of_ram
	 */
	virtual bool map_buffer_ppgtt(Buffer_id id, Gpu::addr_t va) = 0;

	/**
	 * Unmap buffer
	 *
	 * \param id  buffer id
	 */
	virtual void unmap_buffer_ppgtt(Buffer_id id, Gpu::addr_t) = 0;

	/**
	 * Set tiling for buffer
	 *
	 * \param id    buffer id
	 * \param mode  tiling mode
	 */
	virtual bool set_tiling(Buffer_id id, unsigned mode) = 0;

	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_info, Info, info);
	GENODE_RPC_THROW(Rpc_exec_buffer, Gpu::Info::Execution_buffer_sequence, exec_buffer,
	                 GENODE_TYPE_LIST(Invalid_state),
	                 Gpu::Buffer_id, Genode::size_t);
	GENODE_RPC(Rpc_completion_sigh, void, completion_sigh,
	           Genode::Signal_context_capability);
	GENODE_RPC_THROW(Rpc_alloc_buffer, Genode::Dataspace_capability, alloc_buffer,
	                 GENODE_TYPE_LIST(Out_of_ram),
	                 Gpu::Buffer_id, Genode::size_t);
	GENODE_RPC(Rpc_free_buffer, void, free_buffer, Gpu::Buffer_id);
	GENODE_RPC_THROW(Rpc_map_buffer, Genode::Dataspace_capability, map_buffer,
	                 GENODE_TYPE_LIST(Mapping_buffer_failed, Out_of_ram),
	                 Gpu::Buffer_id, bool);
	GENODE_RPC(Rpc_unmap_buffer, void, unmap_buffer,
	           Gpu::Buffer_id);
	GENODE_RPC_THROW(Rpc_map_buffer_ppgtt, bool, map_buffer_ppgtt,
	                 GENODE_TYPE_LIST(Mapping_buffer_failed, Out_of_ram),
	                 Gpu::Buffer_id, Gpu::addr_t);
	GENODE_RPC(Rpc_unmap_buffer_ppgtt, void, unmap_buffer_ppgtt,
	           Gpu::Buffer_id, Gpu::addr_t);
	GENODE_RPC(Rpc_set_tiling, bool, set_tiling,
	           Gpu::Buffer_id, unsigned);

	GENODE_RPC_INTERFACE(Rpc_info, Rpc_exec_buffer,
	                     Rpc_completion_sigh, Rpc_alloc_buffer,
	                     Rpc_free_buffer, Rpc_map_buffer, Rpc_unmap_buffer,
	                     Rpc_map_buffer_ppgtt, Rpc_unmap_buffer_ppgtt,
	                     Rpc_set_tiling);
};

#endif /* _INCLUDE__GPU_SESSION__GPU_SESSION_H_ */
