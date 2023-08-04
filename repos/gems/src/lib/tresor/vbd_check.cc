/*
 * \brief  Module for checking all hashes of a VBD snapshot
 * \author Martin Stein
 * \date   2023-05-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/vbd_check.h>
#include <tresor/block_io.h>
#include <tresor/hash.h>

using namespace Tresor;

Vbd_check_request::Vbd_check_request(Module_id src_mod, Module_channel_id src_chan, Tree_root const &vbd, bool &success)
:
	Module_request { src_mod, src_chan, VBD_CHECK }, _vbd { vbd }, _success { success }
{ }


void Vbd_check_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("vbd check: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


bool Vbd_check_channel::_execute_node(Tree_level_index lvl, Tree_node_index node_idx, bool &progress)
{
	bool &check_node = _check_node[lvl][node_idx];
	if (!check_node)
		return false;

	Request &req { *_req_ptr };
	Type_1_node const &node = _t1_blks.items[lvl].nodes[node_idx];
	switch (_state) {
	case REQ_IN_PROGRESS:

		if (lvl == 1) {
			if (!_num_remaining_leaves) {
				if (node.valid()) {
					_mark_req_failed(progress, { "lvl ", lvl, " node ", node_idx, " (", node,
					                             ") valid but no leaves remaining" });
					break;
				}
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, ": expectedly invalid");
				break;
			}
			if (node.gen == INITIAL_GENERATION) {
				_num_remaining_leaves--;
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, ": uninitialized");
				break;
			}
		} else {
			if (!node.valid()) {
				if (_num_remaining_leaves) {
					_mark_req_failed(progress, { "lvl ", lvl, " node ", node_idx, " invalid but ",
					                             _num_remaining_leaves, " leaves remaining" });
					break;
				}
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, ": expectedly invalid");
				break;
			}
		}
		_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, node.pba, _blk);
		if (VERBOSE_CHECK)
			log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, " (", node,
			    "): load to lvl ", lvl - 1);
		break;

	case READ_BLK_SUCCEEDED:

		if (!(lvl > 1 && node.gen == INITIAL_GENERATION) && !check_hash(_blk, node.hash)) {
			_mark_req_failed(progress, { "lvl ", lvl, " node ", node_idx, " (", node, ") has bad hash" });
			break;
		}
		if (lvl == 1)
			_num_remaining_leaves--;
		else {
			_t1_blks.items[lvl - 1].decode_from_blk(_blk);
			for (bool &cn : _check_node[lvl - 1])
				cn = true;
		}
		check_node = false;
		_state = REQ_IN_PROGRESS;
		progress = true;
		if (VERBOSE_CHECK)
			log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, ": good hash");
		break;

	default: break;
	}
	return true;
}


void Vbd_check_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	Request &req { *_req_ptr };
	if (_state == REQ_SUBMITTED) {
		for (Tree_level_index lvl { 1 }; lvl <= req._vbd.max_lvl + 1; lvl++)
			for (Tree_node_index node_idx { 0 }; node_idx < req._vbd.degree; node_idx++)
				_check_node[lvl][node_idx] = false;

		_num_remaining_leaves = req._vbd.num_leaves;
		_t1_blks.items[req._vbd.max_lvl + 1].nodes[0] = req._vbd.t1_node();
		_check_node[req._vbd.max_lvl + 1][0] = true;
		_state = REQ_IN_PROGRESS;
	}
	for (Tree_level_index lvl { 1 }; lvl <= req._vbd.max_lvl + 1; lvl++)
		for (Tree_node_index node_idx { 0 }; node_idx < req._vbd.degree; node_idx++)
			if (_execute_node(lvl, node_idx, progress))
				return;

	_mark_req_successful(progress);
}


void Vbd_check_channel::_mark_req_failed(bool &progress, Error_string str)
{
	error("vbd check request (", *_req_ptr, ") failed: ", str);
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Vbd_check_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Vbd_check_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = REQ_SUBMITTED;
}


Vbd_check::Vbd_check()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}


void Vbd_check::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}
