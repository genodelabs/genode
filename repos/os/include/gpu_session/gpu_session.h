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

#include <session/session.h>

namespace Gpu {

	using addr_t = Genode::uint64_t;

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

	Info(Chip_id chip_id, Features features,
	     size_t aperture_size, Context_id ctx_id)
	:
		chip_id(chip_id), features(features),
		aperture_size(aperture_size), ctx_id(ctx_id)
	{ }
};


/*
 * Gpu session interface
 */
struct Gpu::Session : public Genode::Session
{
	struct Out_of_ram             : Genode::Exception { };
	struct Out_of_caps            : Genode::Exception { };

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
	 * \param cap   capability to buffer object containing the exec buffer
	 * \param size  size of the batch buffer in bytes
	 */
	virtual void exec_buffer(Genode::Dataspace_capability cap, Genode::size_t size) = 0;

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
	 * \param size  size of buffer in bytes
	 *
	 * \throw Out_of_ram
	 * \throw Out_of_caps
	 */
	virtual Genode::Dataspace_capability alloc_buffer(Genode::size_t size) = 0;

	/**
	 * Free buffer dataspace
	 *
	 * \param ds  dataspace capability for buffer
	 */
	virtual void free_buffer(Genode::Dataspace_capability ds) = 0;

	/**
	 * Map buffer
	 *
	 * \param ds        dataspace capability for buffer
	 * \param aperture  if true create CPU accessible mapping through
	 *                  GGTT window, otherwise create PPGTT mapping
	 */
	virtual Genode::Dataspace_capability map_buffer(Genode::Dataspace_capability ds,
	                                                bool aperture) = 0;

	/**
	 * Unmap buffer
	 *
	 * \param ds  dataspace capability for buffer
	 */
	virtual void unmap_buffer(Genode::Dataspace_capability ds) = 0;

	/**
	 * Map buffer in PPGTT
	 *
	 * \param ds  dataspace capability for buffer
	 * \param va  virtual address
	 */
	virtual bool map_buffer_ppgtt(Genode::Dataspace_capability ds,
	                              Gpu::addr_t va) = 0;

	/**
	 * Unmap buffer
	 *
	 * \param ds  dataspace capability for buffer
	 */
	virtual void unmap_buffer_ppgtt(Genode::Dataspace_capability ds, Gpu::addr_t) = 0;

	/**
	 * Set tiling for buffer
	 *
	 * \param ds    dataspace capability for buffer
	 * \param mode  tiling mode
	 */
	virtual bool set_tiling(Genode::Dataspace_capability ds, unsigned mode) = 0;

	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_info, Info, info);
	GENODE_RPC(Rpc_exec_buffer, void, exec_buffer, Genode::Dataspace_capability,
	           Genode::size_t);
	GENODE_RPC(Rpc_completion_sigh, void, completion_sigh,
	           Genode::Signal_context_capability);
	GENODE_RPC_THROW(Rpc_alloc_buffer, Genode::Dataspace_capability, alloc_buffer,
	                 GENODE_TYPE_LIST(Out_of_ram),
	                 Genode::size_t);
	GENODE_RPC(Rpc_free_buffer, void, free_buffer, Genode::Dataspace_capability);
	GENODE_RPC_THROW(Rpc_map_buffer, Genode::Dataspace_capability, map_buffer,
	                 GENODE_TYPE_LIST(Out_of_ram),
	                 Genode::Dataspace_capability, bool);
	GENODE_RPC(Rpc_unmap_buffer, void, unmap_buffer,
	           Genode::Dataspace_capability);
	GENODE_RPC_THROW(Rpc_map_buffer_ppgtt, bool, map_buffer_ppgtt,
	                 GENODE_TYPE_LIST(Out_of_ram),
	                 Genode::Dataspace_capability, Gpu::addr_t);
	GENODE_RPC(Rpc_unmap_buffer_ppgtt, void, unmap_buffer_ppgtt,
	           Genode::Dataspace_capability, Gpu::addr_t);
	GENODE_RPC(Rpc_set_tiling, bool, set_tiling,
	           Genode::Dataspace_capability, unsigned);

	GENODE_RPC_INTERFACE(Rpc_info, Rpc_exec_buffer,
	                     Rpc_completion_sigh, Rpc_alloc_buffer,
	                     Rpc_free_buffer, Rpc_map_buffer, Rpc_unmap_buffer,
	                     Rpc_map_buffer_ppgtt, Rpc_unmap_buffer_ppgtt,
	                     Rpc_set_tiling);
};

#endif /* _INCLUDE__GPU_SESSION__GPU_SESSION_H_ */
