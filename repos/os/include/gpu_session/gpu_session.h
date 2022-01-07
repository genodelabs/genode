/*
 * \brief  Gpu session interface.
 * \author Josef Soentgen
 * \date   2017-04-28
 */

/*
 * Copyright (C) 2017-2021 Genode Labs GmbH
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
	using Buffer_capability = Genode::Capability<Buffer>;

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
}

/*
 * Execution buffer sequence number
 */
struct Gpu::Sequence_number
{
	Genode::uint64_t value;
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
	 * Execute commands from given buffer
	 *
	 * \param id    buffer id
	 * \param size  size of the batch buffer in bytes
	 *
	 * \return execution buffer sequence number for complete checks
	 *
	 * \throw Invalid_state is thrown if the provided buffer is not valid, e.g not mapped
	 */
	virtual Gpu::Sequence_number exec_buffer(Buffer_id id, Genode::size_t size) = 0;

	/**
	 * Check if execution buffer has been completed
	 *
	 * \param seqno  sequence number of the execution buffer
	 *
	 * \return true if execution buffer has been finished, otherwise
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
	 * Export buffer dataspace from GPU session
	 *
	 * \param id  buffer id of associated buffer
	 */
	virtual Buffer_capability export_buffer(Buffer_id id) = 0;

	/**
	 * Import buffer dataspace to GPU session
	 *
	 * \param cap  capability of buffer as retrieved bu 'export_buffer'
	 * \param id   buffer id to be associated to this buffer in the session
	 *
	 * \throw Conflicting_id
	 * \throw Out_of_caps
	 * \throw Out_of_ram
	 * \throw Invalid_state  (cap is no longer valid)
	 */
	virtual void import_buffer(Buffer_capability cap, Buffer_id id) = 0;

	/**
	 * Map buffer
	 *
	 * \param id        buffer id
	 * \param aperture  if true create CPU accessible mapping through
	 *                  GGTT window, otherwise create PPGTT mapping
	 * \param attrs     specify how the buffer is mapped
	 *
	 * \throw Mapping_buffer_failed
	 * \throw Out_of_caps
	 * \throw Out_of_ram
	 */
	virtual Genode::Dataspace_capability map_buffer(Buffer_id id,
	                                                bool aperture,
	                                                Mapping_attributes attrs) = 0;

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
	 * \throw Out_of_caps
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

	GENODE_RPC(Rpc_info_dataspace, Genode::Dataspace_capability, info_dataspace);
	GENODE_RPC_THROW(Rpc_exec_buffer, Gpu::Sequence_number, exec_buffer,
	                 GENODE_TYPE_LIST(Invalid_state),
	                 Gpu::Buffer_id, Genode::size_t);
	GENODE_RPC(Rpc_complete, bool, complete,
	           Gpu::Sequence_number);
	GENODE_RPC(Rpc_completion_sigh, void, completion_sigh,
	           Genode::Signal_context_capability);
	GENODE_RPC_THROW(Rpc_alloc_buffer, Genode::Dataspace_capability, alloc_buffer,
	                 GENODE_TYPE_LIST(Out_of_caps, Out_of_ram),
	                 Gpu::Buffer_id, Genode::size_t);
	GENODE_RPC(Rpc_free_buffer, void, free_buffer, Gpu::Buffer_id);
	GENODE_RPC(Rpc_export_buffer, Gpu::Buffer_capability, export_buffer, Gpu::Buffer_id);
	GENODE_RPC_THROW(Rpc_import_buffer, void, import_buffer,
	                 GENODE_TYPE_LIST(Out_of_caps, Out_of_ram, Conflicting_id, Invalid_state),
	                 Gpu::Buffer_capability, Gpu::Buffer_id);
	GENODE_RPC_THROW(Rpc_map_buffer, Genode::Dataspace_capability, map_buffer,
	                 GENODE_TYPE_LIST(Mapping_buffer_failed, Out_of_caps, Out_of_ram),
	                 Gpu::Buffer_id, bool, Gpu::Mapping_attributes);
	GENODE_RPC(Rpc_unmap_buffer, void, unmap_buffer,
	           Gpu::Buffer_id);
	GENODE_RPC_THROW(Rpc_map_buffer_ppgtt, bool, map_buffer_ppgtt,
	                 GENODE_TYPE_LIST(Mapping_buffer_failed, Out_of_caps, Out_of_ram),
	                 Gpu::Buffer_id, Gpu::addr_t);
	GENODE_RPC(Rpc_unmap_buffer_ppgtt, void, unmap_buffer_ppgtt,
	           Gpu::Buffer_id, Gpu::addr_t);
	GENODE_RPC(Rpc_set_tiling, bool, set_tiling,
	           Gpu::Buffer_id, unsigned);

	GENODE_RPC_INTERFACE(Rpc_info_dataspace, Rpc_exec_buffer,
	                     Rpc_complete, Rpc_completion_sigh, Rpc_alloc_buffer,
	                     Rpc_free_buffer, Rpc_export_buffer, Rpc_import_buffer,
	                     Rpc_map_buffer, Rpc_unmap_buffer,
	                     Rpc_map_buffer_ppgtt, Rpc_unmap_buffer_ppgtt,
	                     Rpc_set_tiling);
};

#endif /* _INCLUDE__GPU_SESSION__GPU_SESSION_H_ */
