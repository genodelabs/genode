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


Module_request::Module_request(Module_id src_module_id, Module_channel_id src_chan_id, Module_id dst_module_id)
:
	_src_module_id { src_module_id }, _src_chan_id { src_chan_id }, _dst_module_id { dst_module_id }
{ }


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
	case VBD_INITIALIZER: return "vbd_initializer";
	case FT_INITIALIZER: return "ft_initializer";
	case SB_INITIALIZER: return "sb_initializer";
	case SB_CHECK: return "sb_check";
	case VBD_CHECK: return "vbd_check";
	case FT_CHECK: return "ft_check";
	case SPLITTER: return "splitter";
	case REQUEST_POOL: return "request_pool";
	default: break;
	}
	ASSERT_NEVER_REACHED;
}


void Module_channel::generated_req_completed()
{
	ASSERT(_gen_req_state == IN_PROGRESS);
	_gen_req_state = NONE;
	_generated_req_completed(_gen_req_complete_state);
}


bool Module_channel::try_submit_request(Module_request &req)
{
	if (_req_ptr)
		return false;

	req.dst_chan_id(_id);
	_req_ptr = &req;
	_request_submitted(req);
	return true;
}


bool Module::try_submit_request(Module_request &req)
{
	bool success { false };
	for_each_channel([&] (Module_channel &chan) {
		if (success)
			return;

		success = chan.try_submit_request(req);
	});
	return success;
}


void Module_composition::execute_modules()
{
	bool progress { true };
	while (progress) {

		progress = false;
		for (Module_id id { 0 }; id <= MAX_MODULE_ID; id++) {
			if (!_module_ptrs[id])
				continue;

			Module &mod { *_module_ptrs[id] };
			mod.execute(progress);
			mod.for_each_generated_request([&] (Module_request &req) {
				ASSERT(req.dst_module_id() <= MAX_MODULE_ID);
				ASSERT(_module_ptrs[req.dst_module_id()]);
				Module &dst_module { *_module_ptrs[req.dst_module_id()] };
				if (dst_module.try_submit_request(req)) {
					if (VERBOSE_MODULE_COMMUNICATION)
						log(module_name(id), " ", req.src_chan_id(), " --", req, "--> ",
						    module_name(req.dst_module_id()), " ", req.dst_chan_id());

					progress = true;
					return true;
				}
				if (VERBOSE_MODULE_COMMUNICATION)
					log(module_name(id), " ", req.src_chan_id(), " --", req, "-| ", module_name(req.dst_module_id()));

				return false;
			});
			mod.for_each_completed_request([&] (Module_request &req) {
				ASSERT(req.src_module_id() <= MAX_MODULE_ID);
				if (VERBOSE_MODULE_COMMUNICATION)
					log(module_name(req.src_module_id()), " ", req.src_chan_id(), " <--", req,
					    "-- ", module_name(id), " ", req.dst_chan_id());

				Module &src_module { *_module_ptrs[req.src_module_id()] };
				src_module.with_channel(req.src_chan_id(), [&] (Module_channel &chan) {
					chan.generated_req_completed(); });
				progress = true;
			});
		}
	};
}


void Module_composition::add_module(Module_id module_id, Module &mod)
{
	ASSERT(module_id <= MAX_MODULE_ID);
	ASSERT(!_module_ptrs[module_id]);
	_module_ptrs[module_id] = &mod;
}


void Module_composition::remove_module(Module_id module_id)
{
	ASSERT(module_id <= MAX_MODULE_ID);
	ASSERT(_module_ptrs[module_id]);
	_module_ptrs[module_id] = nullptr;
}
