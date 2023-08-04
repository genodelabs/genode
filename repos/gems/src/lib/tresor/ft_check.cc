/*
 * \brief  Module for checking all hashes of a free tree or meta tree
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
#include <tresor/ft_check.h>
#include <tresor/block_io.h>
#include <tresor/hash.h>

using namespace Tresor;

Ft_check_request::Ft_check_request(Module_id src_mod, Module_channel_id src_chan, Tree_root const &ft, bool &success)
:
	Module_request { src_mod, src_chan, FT_CHECK }, _ft { ft }, _success { success }
{ }


bool Ft_check_channel::_execute_node(Tree_level_index lvl, Tree_node_index node_idx, bool &progress)
{
	bool &check_node { _check_node[lvl][node_idx] };

	if (check_node == false)
		return false;

	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_IN_PROGRESS:

		if (lvl == 1) {
			Type_2_node const &node { _t2_blk.nodes[node_idx] };
			if (!_num_remaining_leaves) {
				if (node.valid()) {
					_mark_req_failed(progress, { "lvl ", lvl, " node ", node_idx, " (", node,
					                             ") valid but no leaves remaining" });
					break;
				}
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " unused");
				break;
			}
			_num_remaining_leaves--;
			check_node = false;
			progress = true;
			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " done");
		} else {
			Type_1_node const &node { _t1_blks.items[lvl].nodes[node_idx] };
			if (!node.valid()) {
				if (_num_remaining_leaves) {
					_mark_req_failed(progress, { "lvl ", lvl, " node ", node_idx, " invalid but ",
					                             _num_remaining_leaves, " leaves remaining" });
					break;
				}
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " unused");
				break;
			}
			_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, node.pba, _blk);
			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx,
				    " (", node, "): load to lvl ", lvl - 1);
		}
		break;

	case READ_BLK_SUCCEEDED:
	{
		Type_1_node const &node { _t1_blks.items[lvl].nodes[node_idx] };
		if (node.gen != INITIAL_GENERATION && !check_hash(_blk, node.hash)) {
			_mark_req_failed(progress, { "lvl ", lvl, " node ", node_idx, " (", node, ") has bad hash" });
			break;
		}
		if (lvl == 2)
			_t2_blk.decode_from_blk(_blk);
		else
			_t1_blks.items[lvl - 1].decode_from_blk(_blk);
		for (bool &cn : _check_node[lvl - 1])
			cn = true;

		_state = REQ_IN_PROGRESS;
		check_node = false;
		progress = true;
		if (VERBOSE_CHECK)
			log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " has good hash");
		break;
	}
	default: break;
	}
	return true;
}


void Ft_check_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	if (_state == REQ_SUBMITTED) {
		for (Tree_level_index lvl { 1 }; lvl <= _req_ptr->_ft.max_lvl + 1; lvl++)
			for (Tree_node_index node_idx { 0 }; node_idx < _req_ptr->_ft.degree; node_idx++)
				_check_node[lvl][node_idx] = false;

		_num_remaining_leaves = _req_ptr->_ft.num_leaves;
		_t1_blks.items[_req_ptr->_ft.max_lvl + 1].nodes[0] = _req_ptr->_ft.t1_node();
		_check_node[_req_ptr->_ft.max_lvl + 1][0] = true;
		_state = REQ_IN_PROGRESS;
	}
	for (Tree_level_index lvl { 1 }; lvl <= _req_ptr->_ft.max_lvl + 1; lvl++)
		for (Tree_node_index node_idx { 0 }; node_idx < _req_ptr->_ft.degree; node_idx++)
			if (_execute_node(lvl, node_idx, progress))
				return;

	_mark_req_successful(progress);
}


void Ft_check_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("ft check: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Ft_check_channel::_mark_req_failed(bool &progress, Error_string str)
{
	error("ft check request (", *_req_ptr, ") failed: ", str);
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Ft_check_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Ft_check_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = REQ_SUBMITTED;
}


Ft_check::Ft_check()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}


void Ft_check::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}
