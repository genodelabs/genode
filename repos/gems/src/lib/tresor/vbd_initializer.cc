/*
 * \brief  Module for initializing the VBD
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2023-03-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/block_io.h>
#include <tresor/hash.h>
#include <tresor/vbd_initializer.h>

using namespace Tresor;

Vbd_initializer_request::Vbd_initializer_request(Module_id src_mod, Module_channel_id src_chan, Tree_root &vbd,
                                                 Pba_allocator &pba_alloc, bool &success)
:
	Module_request { src_mod, src_chan, VBD_INITIALIZER }, _vbd { vbd }, _pba_alloc { pba_alloc }, _success { success }
{ }


bool Vbd_initializer_channel::_execute_node(Tree_level_index lvl, Tree_node_index node_idx, bool &progress)
{
	Type_1_node &node = _t1_blks.items[lvl].nodes[node_idx];
	Node_state &node_state = _node_states[lvl][node_idx];
	switch (node_state) {
	case DONE: return false;
	case INIT_BLOCK:

		if (lvl == 1) {
			node_state = INIT_NODE;
			progress = true;
		} else
			if (_num_remaining_leaves) {
				_reset_level(lvl - 1, INIT_BLOCK);
				node_state = INIT_NODE;
				progress = true;
				if (VERBOSE_VBD_INIT)
					log("[vbd_init] node: ", lvl, " ", node_idx, " reset level: ", lvl - 1);
			} else {
				node = { };
				node_state = DONE;
				progress = true;
				if (VERBOSE_VBD_INIT)
					log("[vbd_init] node: ", lvl, " ", node_idx, " assign pba 0, inner node unused");
			}
		break;

	case INIT_NODE:

		if (lvl == 1)
			if (_num_remaining_leaves) {
				node = { };
				if (!_req_ptr->_pba_alloc.alloc(node.pba)) {
					_mark_req_failed(progress, "allocate pba");
					break;
				}
				node_state = DONE;
				_num_remaining_leaves--;
				progress = true;
				if (VERBOSE_VBD_INIT)
					log("[vbd_init] node: ", lvl, " ", node_idx, " assign pba: ", node.pba, " leaves left: ", _num_remaining_leaves);
			} else {
				node = { };
				node_state = DONE;
				progress = true;
				if (VERBOSE_VBD_INIT)
					log("[vbd_init] node: ", lvl, " ", node_idx, " assign pba 0, leaf unused");
			}
		else {
			node = { };
			if (!_req_ptr->_pba_alloc.alloc(node.pba)) {
				_mark_req_failed(progress, "allocate pba");
				break;
			}
			_t1_blks.items[lvl - 1].encode_to_blk(_blk);
			calc_hash(_blk, node.hash);
			node_state = WRITE_BLOCK;
			generate_req<Block_io::Write>(EXECUTE_NODES, progress, node.pba, _blk, _generated_req_success);
			_state = REQ_GENERATED;
			if (VERBOSE_VBD_INIT)
				log("[vbd_init] node: ", lvl, " ", node_idx, " assign pba: ", node.pba);
		}
		break;

	case WRITE_BLOCK:

		ASSERT(lvl > 1);
		node_state = DONE;
		progress = true;
		if (VERBOSE_VBD_INIT)
			log("[vbd_init] node: ", lvl, " ", node_idx, " write pba: ", node.pba, " level: ", lvl - 1, " (node: ", node, ")");
		break;
	}
	return true;
}


void Vbd_initializer_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("vbd initializer: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Vbd_initializer_channel::_mark_req_failed(bool &progress, char const *str)
{
	error("vbd_initializer request (", *_req_ptr, ") failed because: ", str);
	_req_ptr->_success = false;
	_state = COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Vbd_initializer_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_vbd.t1_node(_t1_blks.items[_req_ptr->_vbd.max_lvl + 1].nodes[0]);
	_req_ptr->_success = true;
	_state = COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Vbd_initializer_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = SUBMITTED;
}


void Vbd_initializer_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:

		_num_remaining_leaves = req._vbd.num_leaves;
		for (Tree_level_index lvl = 0; lvl < TREE_MAX_LEVEL; lvl++)
			_reset_level(lvl, Vbd_initializer_channel::DONE);

		_node_states[req._vbd.max_lvl + 1][0] = Vbd_initializer_channel::INIT_BLOCK;
		_state = EXECUTE_NODES;
		progress = true;
		return;

	case EXECUTE_NODES:

		for (Tree_level_index lvl = 0; lvl <= req._vbd.max_lvl + 1; lvl++)
			for (Tree_node_index node_idx = 0; node_idx < req._vbd.degree; node_idx++)
				if (_execute_node(lvl, node_idx, progress))
					return;

		if (_num_remaining_leaves)
			_mark_req_failed(progress, "leaves remaining");
		else
			_mark_req_successful(progress);
		return;

	default: return;
	}
}


void Vbd_initializer_channel::_reset_level(Tree_level_index lvl, Node_state state)
{
	for (unsigned int idx = 0; idx < NUM_NODES_PER_BLK; idx++) {
		_t1_blks.items[lvl].nodes[idx] = { };
		_node_states[lvl][idx] = state;
	}
}


Vbd_initializer::Vbd_initializer()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}


void Vbd_initializer::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}
