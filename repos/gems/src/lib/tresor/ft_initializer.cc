/*
 * \brief  Module for initializing the free tree
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2023-03-09
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
#include <tresor/ft_initializer.h>

using namespace Tresor;

Ft_initializer_request::Ft_initializer_request(Module_id src_mod, Module_channel_id src_chan,
                                               Tree_root &ft, Pba_allocator &pba_alloc, bool &success)
:
	Module_request { src_mod, src_chan, FT_INITIALIZER }, _ft { ft }, _pba_alloc { pba_alloc }, _success { success }
{ }


bool Ft_initializer_channel::_execute_t2_node(Tree_node_index node_idx, bool &progress)
{
	Node_state &node_state { _t2_node_states[node_idx] };
	Type_2_node &node { _t2_blk.nodes[node_idx] };
	switch (node_state) {
	case DONE: return false;
	case INIT_BLOCK:

		node_state = INIT_NODE;
		progress = true;
		break;

	case INIT_NODE:

		if (_num_remaining_leaves) {
			node = { };
			if (!_req_ptr->_pba_alloc.alloc(node.pba)) {
				_mark_req_failed(progress, "allocate pba");
				break;
			}
			node_state = DONE;
			_num_remaining_leaves--;
			progress = true;
			if (VERBOSE_FT_INIT)
				log("[ft_init] node: ", 1, " ", node_idx, " assign pba: ", node.pba, " leaves left: ", _num_remaining_leaves);
		} else {
			node = { };
			node_state = DONE;
			progress = true;
			if (VERBOSE_FT_INIT)
				log("[ft_init] node: ", 1, " ", node_idx, " assign pba 0, leaf unused");
		}
		break;

	case WRITE_BLK: ASSERT_NEVER_REACHED;
	}
	return true;
}


bool Ft_initializer_channel::_execute_t1_node(Tree_level_index lvl, Tree_node_index node_idx, bool &progress)
{
	Type_1_node &node { _t1_blks.items[lvl].nodes[node_idx] };
	Node_state &node_state { _t1_node_states[lvl][node_idx] };
	switch (node_state) {
	case DONE: return false;
	case INIT_BLOCK:

		if (_num_remaining_leaves) {
			_reset_level(lvl - 1, INIT_BLOCK);
			node_state = INIT_NODE;
			progress = true;
			if (VERBOSE_FT_INIT)
				log("[ft_init] node: ", lvl, " ", node_idx, " reset level: ", lvl - 1);
		} else {
			node = { };
			node_state = DONE;
			progress = true;
			if (VERBOSE_FT_INIT)
				log("[ft_init] node: ", lvl, " ", node_idx, " assign pba 0, unused");
		}
		break;

	case INIT_NODE:
	{
		node = { };
		if (!_req_ptr->_pba_alloc.alloc(node.pba)) {
			_mark_req_failed(progress, "allocate pba");
			break;
		}
		if (lvl == 2)
			_t2_blk.encode_to_blk(_blk);
		else
			_t1_blks.items[lvl - 1].encode_to_blk(_blk);
		calc_hash(_blk, node.hash);
		generate_req<Block_io::Write>(EXECUTE_NODES, progress, node.pba, _blk, _generated_req_success);
		_state = REQ_GENERATED;
		node_state = WRITE_BLK;
		progress = true;
		if (VERBOSE_FT_INIT)
			log("[ft_init] node: ", lvl, " ", node_idx, " assign pba: ", node.pba);
		break;
	}
	case WRITE_BLK:

		node_state = DONE;
		progress = true;
		if (VERBOSE_FT_INIT)
			log("[ft_init] node: ", lvl, " ", node_idx, " write pba: ", node.pba, " level: ", lvl - 1, " (node: ", node, ")");
		break;
	}
	return true;
}


void Ft_initializer_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("ft initializer request (", *_req_ptr, ") failed because generated request failed");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Ft_initializer_channel::_mark_req_failed(bool &progress,  char const *str)
{
	error("ft initializer request (", *_req_ptr, ") failed because: ", str);
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Ft_initializer_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_ft.t1_node(_t1_blks.items[_req_ptr->_ft.max_lvl + 1].nodes[0]);
	_req_ptr->_success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Ft_initializer_channel::_reset_level(Tree_level_index lvl, Node_state node_state)
{
	if (lvl == 1)
		for (Tree_node_index idx = 0; idx < NUM_NODES_PER_BLK; idx++) {
			_t2_blk.nodes[idx] = { };
			_t2_node_states[idx] = node_state;
		}
	else
		for (Tree_node_index idx = 0; idx < NUM_NODES_PER_BLK; idx++) {
			_t1_blks.items[lvl].nodes[idx] = { };
			_t1_node_states[lvl][idx] = node_state;
		}
}


void Ft_initializer_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		_num_remaining_leaves = req._ft.num_leaves;
		for (Tree_level_index lvl = 0; lvl < TREE_MAX_LEVEL; lvl++)
			_reset_level(lvl, DONE);

		_t1_node_states[req._ft.max_lvl + 1][0] = INIT_BLOCK;
		_state = EXECUTE_NODES;
		progress = true;
		return;

	case EXECUTE_NODES:

		for (Tree_node_index node_idx = 0; node_idx < req._ft.degree; node_idx++)
			if (_execute_t2_node(node_idx, progress))
				return;

		for (Tree_level_index lvl = 1; lvl <= req._ft.max_lvl + 1; lvl++)
			for (Tree_node_index node_idx = 0; node_idx < req._ft.degree; node_idx++)
				if (_execute_t1_node(lvl, node_idx, progress))
					return;

		if (_num_remaining_leaves)
			_mark_req_failed(progress, "leaves remaining");
		else
			_mark_req_successful(progress);
		return;

	default: return;
	}
}


void Ft_initializer_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = REQ_SUBMITTED;
}


Ft_initializer::Ft_initializer()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}


void Ft_initializer::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}
