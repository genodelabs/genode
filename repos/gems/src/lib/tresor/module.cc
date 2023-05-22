/*
 * \brief  Framework for component internal modularization
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/types.h>
#include <tresor/module.h>

using namespace Tresor;


/********************
 ** Module_request **
 ********************/

Module_request::Module_request(Module_id         src_module_id,
                               Module_request_id src_request_id,
                               Module_id         dst_module_id)
:
	_src_module_id  { src_module_id },
	_src_request_id { src_request_id },
	_dst_module_id  { dst_module_id }
{ }


String<32> Module_request::src_request_id_str() const
{
	return
		_src_request_id == INVALID_MODULE_REQUEST_ID ?
			String<32> { "?" } : String<32> { _src_request_id };
}


String<32> Module_request::dst_request_id_str() const
{
	return
		_dst_request_id == INVALID_MODULE_REQUEST_ID ?
			String<32> { "?" } : String<32> { _dst_request_id };
}


/**********************
 ** Global functions **
 **********************/

char const *Tresor::module_name(Module_id id)
{
	switch (id) {
	case CRYPTO: return "crypto";
	case BLOCK_IO: return "block_io";
	case CACHE: return "cache";
	case META_TREE: return "meta_tree";
	case FREE_TREE: return "free_tree";
	case VIRTUAL_BLOCK_DEVICE: return "vbd";
	case SUPERBLOCK_CONTROL: return "sb_control";
	case CLIENT_DATA: return "client_data";
	case TRUST_ANCHOR: return "trust_anchor";
	case COMMAND_POOL: return "command_pool";
	case BLOCK_ALLOCATOR: return "block_allocator";
	case VBD_INITIALIZER: return "vbd_initializer";
	case FT_INITIALIZER: return "ft_initializer";
	case SB_INITIALIZER: return "sb_initializer";
	case SB_CHECK: return "sb_check";
	case VBD_CHECK: return "vbd_check";
	case FT_CHECK: return "ft_check";
	case FT_RESIZING: return "ft_resizing";
	case REQUEST_POOL: return "request_pool";
	default: break;
	}
	return "?";
}
