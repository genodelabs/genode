/*
 * \brief  Module for doing VBD COW allocations on the free tree
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
#include <tresor/free_tree.h>
#include <tresor/meta_tree.h>
#include <tresor/block_io.h>
#include <tresor/hash.h>

using namespace Tresor;

char const *Free_tree_request::type_to_string(Type type)
{
	switch (type) {
	case ALLOC_FOR_NON_RKG: return "alloc_for_non_rkg";
	case ALLOC_FOR_RKG_CURR_GEN_BLKS: return "alloc_for_rkg_curr_gen_blks";
	case ALLOC_FOR_RKG_OLD_GEN_BLKS: return "alloc_for_rkg_old_gen_blks";
	case EXTENSION_STEP: return "extension_step";
	}
	ASSERT_NEVER_REACHED;
}


Free_tree_request::Free_tree_request(Module_id src_module_id, Module_channel_id src_chan_id, Type type,
                                     Tree_root &ft, Tree_root &mt, Snapshots const &snapshots, Generation last_secured_gen,
                                     Generation curr_gen, Generation free_gen, Number_of_blocks num_required_pbas,
                                     Tree_walk_pbas &new_blocks, Type_1_node_walk const &old_blocks,
                                     Tree_level_index max_lvl, Virtual_block_address vba, Tree_degree vbd_degree,
                                     Virtual_block_address vbd_max_vba, bool rekeying, Key_id prev_key_id,
                                     Key_id curr_key_id, Virtual_block_address rekeying_vba, Physical_block_address &pba,
                                     Number_of_blocks &num_pbas, bool &success)

:
	Module_request { src_module_id, src_chan_id, FREE_TREE }, _type { type }, _ft { ft }, _mt { mt },
	_curr_gen { curr_gen }, _free_gen { free_gen }, _num_required_pbas { num_required_pbas }, _new_blocks { new_blocks },
	_old_blocks { old_blocks }, _max_lvl { max_lvl }, _vba { vba }, _vbd_degree { vbd_degree }, _vbd_max_vba { vbd_max_vba },
	_rekeying { rekeying }, _prev_key_id { prev_key_id }, _curr_key_id { curr_key_id }, _rekeying_vba { rekeying_vba },
	_success { success }, _snapshots { snapshots }, _last_secured_gen { last_secured_gen }, _pba { pba }, _num_pbas { num_pbas }
{ }


void Free_tree::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}


bool Free_tree_channel::_can_alloc_pba_of(Type_2_node &node)
{
	Request &req { *_req_ptr };
	if (node.pba == 0 || node.pba == INVALID_PBA || node.free_gen > req._last_secured_gen)
		return false;

	if (!node.reserved)
		return true;

	if (req._rekeying && node.last_key_id == req._prev_key_id && node.last_vba < req._rekeying_vba)
		return true;

	for (Snapshot const &snap : req._snapshots.items)
		if (snap.valid && node.free_gen > snap.gen && node.alloc_gen < snap.gen + 1)
			return false;

	return true;
}


void Free_tree_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("free tree: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Free_tree_channel::_alloc_pba_of(Type_2_node &t2_node)
{
	Request &req { *_req_ptr };
	Tree_level_index vbd_lvl { 0 };
	for (; vbd_lvl <= req._max_lvl && req._new_blocks.pbas[vbd_lvl]; vbd_lvl++);

	Virtual_block_address node_min_vba { vbd_node_min_vba(_vbd_degree_log_2, vbd_lvl, req._vba) };
	req._new_blocks.pbas[vbd_lvl] = t2_node.pba;
	t2_node.alloc_gen = req._old_blocks.nodes[vbd_lvl].gen;
	t2_node.free_gen = req._free_gen;
	Virtual_block_address rkg_vba { req._rekeying_vba };
	switch (req._type) {
	case Request::ALLOC_FOR_NON_RKG:

		t2_node.reserved = true;
		t2_node.pba = req._old_blocks.nodes[vbd_lvl].pba;
		t2_node.last_vba = node_min_vba;
		if (req._rekeying) {
			if (req._vba < rkg_vba)
				t2_node.last_key_id = req._curr_key_id;
			else
				t2_node.last_key_id = req._prev_key_id;
		} else
			t2_node.last_key_id = req._curr_key_id;
		break;

	case Request::ALLOC_FOR_RKG_CURR_GEN_BLKS:

		t2_node.reserved = false;
		t2_node.pba = req._old_blocks.nodes[vbd_lvl].pba;
		t2_node.last_vba = node_min_vba;
		t2_node.last_key_id = req._prev_key_id;
		break;

	case Request::ALLOC_FOR_RKG_OLD_GEN_BLKS:
	{
		t2_node.reserved = true;
		Virtual_block_address node_max_vba { vbd_node_max_vba(_vbd_degree_log_2, vbd_lvl, req._vba) };
		if (rkg_vba < node_max_vba && rkg_vba < req._vbd_max_vba) {
			t2_node.last_key_id = req._prev_key_id;
			t2_node.last_vba = rkg_vba + 1;
		} else if (rkg_vba == node_max_vba || rkg_vba == req._vbd_max_vba) {
			t2_node.last_key_id = req._curr_key_id;
			t2_node.last_vba = node_min_vba;
		} else
			ASSERT_NEVER_REACHED;
		break;
	}
	default: ASSERT_NEVER_REACHED;
	}
}


void Free_tree_channel::_mark_req_failed(bool &progress, char const *str)
{
	error(Request::type_to_string(_req_ptr->_type), " request failed, reason: \"", str, "\"");
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Free_tree_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Free_tree_channel::_start_tree_traversal(bool &progress)
{
	Request &req { *_req_ptr };
	_num_pbas = 0;
	_lvl = req._ft.max_lvl;
	_node_idx[_lvl] = 0;
	_t1_blks[_lvl].nodes[_node_idx[_lvl]] = req._ft.t1_node();
	_generate_req<Block_io::Read>(SEEK_DOWN, progress, req._ft.pba, _blk);
}


void Free_tree_channel::_traverse_curr_node(bool &progress)
{
	if (_lvl) {
		Type_1_node &t1_node { _t1_blks[_lvl].nodes[_node_idx[_lvl]] };
		if (t1_node.pba)
			_generate_req<Block_io::Read>(SEEK_DOWN, progress, t1_node.pba, _blk);
		else {
			_state = SEEK_LEFT_OR_UP;
			progress = true;
		}
	} else {
		Type_2_node &t2_node { _t2_blk.nodes[_node_idx[_lvl]] };
		if (_num_pbas < _req_ptr->_num_required_pbas && _can_alloc_pba_of(t2_node)) {
			if (_apply_allocation)
				_alloc_pba_of(t2_node);
			_num_pbas++;
		}
		_state = SEEK_LEFT_OR_UP;
		progress = true;
	}
}


void Free_tree_channel::_alloc_pbas(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		_vbd_degree_log_2 = log2<Tree_degree_log_2>(req._vbd_degree);
		_apply_allocation = false;
		_start_tree_traversal(progress);
		break;

	case SEEK_DOWN:
	{
		if (!check_hash(_blk, _t1_blks[_lvl].nodes[_node_idx[_lvl]].hash)) {
			_mark_req_failed(progress, "hash mismatch");
			break;
		}
		_lvl--;
		_node_idx[_lvl] = req._ft.degree - 1;
		if (_lvl)
			_t1_blks[_lvl].decode_from_blk(_blk);
		else
			_t2_blk.decode_from_blk(_blk);
		_traverse_curr_node(progress);
		break;
	}
	case SEEK_LEFT_OR_UP:

		if (_lvl < req._ft.max_lvl) {
			if (_node_idx[_lvl] && _num_pbas < _req_ptr->_num_required_pbas) {
				_node_idx[_lvl]--;
				_traverse_curr_node(progress);
			} else {
				_lvl++;
				Type_1_node &t1_node { _t1_blks[_lvl].nodes[_node_idx[_lvl]] };
				if (_apply_allocation)
					if (t1_node.is_volatile(req._curr_gen)) {
						_state = WRITE_BLK;
						progress = true;
					} else
						_generate_req<Meta_tree::Alloc_pba>(WRITE_BLK, progress, req._mt, req._curr_gen, t1_node.pba);
				else {
					_state = SEEK_LEFT_OR_UP;
					progress = true;
				}
			}
		} else {
			if (_apply_allocation) {
				req._ft.t1_node(_t1_blks[_lvl].nodes[_node_idx[_lvl]]);
				_mark_req_successful(progress);
			} else {
				if (_num_pbas < req._num_required_pbas)
					_mark_req_failed(progress, "not enough free pbas");
				else {
					_apply_allocation = true;
					_start_tree_traversal(progress);
				}
			}
		}
		break;

	case WRITE_BLK:
	{
		if (_lvl > 1)
			_t1_blks[_lvl - 1].encode_to_blk(_blk);
		else
			_t2_blk.encode_to_blk(_blk);
		Type_1_node &t1_node { _t1_blks[_lvl].nodes[_node_idx[_lvl]] };
		t1_node.gen = req._curr_gen;
		calc_hash(_blk, t1_node.hash);
		_generate_req<Block_io::Write>(SEEK_LEFT_OR_UP, progress, t1_node.pba, _blk);
		break;
	}
	default: break;
	}
}


void Free_tree_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	switch(_req_ptr->_type) {
	case Request::ALLOC_FOR_NON_RKG:
	case Request::ALLOC_FOR_RKG_CURR_GEN_BLKS:
	case Request::ALLOC_FOR_RKG_OLD_GEN_BLKS: _alloc_pbas(progress); break;
	case Request::EXTENSION_STEP: _extension_step(progress); break;
	}
}


void Free_tree_channel::_generate_write_blk_req(bool &progress)
{
	if (_lvl > 1)
		_t1_blks[_lvl].encode_to_blk(_blk);
	else
		_t2_blk.encode_to_blk(_blk);

	_generate_req<Block_io::Write>(WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[_lvl], _blk);
	if (VERBOSE_FT_EXTENSION)
		log("  lvl ", _lvl, " write to pba ", _new_pbas.pbas[_lvl]);
}


void Free_tree_channel::_add_new_root_lvl()
{
	Request &req { *_req_ptr };
	ASSERT(req._ft.max_lvl < TREE_MAX_LEVEL);
	req._ft.max_lvl++;
	_t1_blks[req._ft.max_lvl] = { };
	_t1_blks[req._ft.max_lvl].nodes[0] = req._ft.t1_node();
	_new_pbas.pbas[req._ft.max_lvl] = alloc_pba_from_range(req._pba, req._num_pbas);
	req._ft.t1_node({ _new_pbas.pbas[req._ft.max_lvl], req._curr_gen });
	if (VERBOSE_FT_EXTENSION)
		log("  set root: ", req._ft, "\n  set lvl ", req._ft.max_lvl, " node 0: ",
		    _t1_blks[req._ft.max_lvl].nodes[0]);
}


void Free_tree_channel::_add_new_branch_at(Tree_level_index dst_lvl, Tree_node_index dst_node_idx)
{
	Request &req { *_req_ptr };
	_num_leaves = 0;
	_lvl = dst_lvl;
	if (dst_lvl > 1) {
		for (Tree_level_index lvl = 1; lvl < dst_lvl; lvl++) {
			if (lvl > 1)
				_t1_blks[lvl] = Type_1_node_block { };
			else
				_t2_blk = Type_2_node_block { };

			if (VERBOSE_FT_EXTENSION)
				log("  reset lvl ", lvl);
		}
	}
	for (; _lvl && req._num_pbas; _lvl--) {
		Tree_node_index node_idx = (_lvl == dst_lvl) ? dst_node_idx : 0;
		if (_lvl > 1) {
			_new_pbas.pbas[_lvl - 1] = alloc_pba_from_range(req._pba, req._num_pbas);
			_t1_blks[_lvl].nodes[node_idx] = { _new_pbas.pbas[_lvl - 1], req._curr_gen };
			if (VERBOSE_FT_EXTENSION)
				log("  set _lvl d ", _lvl, " node ", node_idx, ": ", _t1_blks[_lvl].nodes[node_idx]);

		} else {
			for (; node_idx < req._ft.degree && req._num_pbas; node_idx++) {
				_t2_blk.nodes[node_idx] = { alloc_pba_from_range(req._pba, req._num_pbas) };
				_num_leaves++;
				if (VERBOSE_FT_EXTENSION)
					log("  set _lvl e ", _lvl, " node ", node_idx, ": ", _t2_blk.nodes[node_idx]);
			}
		}
	}
	if (!_lvl)
		_lvl = 1;
}


void Free_tree_channel::_extension_step(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		_num_leaves = 0;
		_vba = req._ft.num_leaves;
		_old_pbas = { };
		_old_generations = { };
		_new_pbas = { };
		_lvl = req._ft.max_lvl;
		_old_pbas.pbas[_lvl] = req._ft.pba;
		_old_generations.items[_lvl] = req._ft.gen;
		if (_vba <= tree_max_max_vba(req._ft.degree, req._ft.max_lvl)) {

			_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, req._ft.pba, _blk);
			if (VERBOSE_FT_EXTENSION)
				log("  root (", req._ft, "): load to lvl ", _lvl);
		} else {
			_add_new_root_lvl();
			_add_new_branch_at(req._ft.max_lvl, 1);
			_generate_write_blk_req(progress);
			if (VERBOSE_FT_EXTENSION)
				log("  pbas allocated: curr gen ", req._curr_gen);
		}
		break;

	case READ_BLK_SUCCEEDED:

		if (_lvl > 1) {

			_t1_blks[_lvl].decode_from_blk(_blk);
			if (_lvl < req._ft.max_lvl) {
				Tree_node_index node_idx = t1_node_idx_for_vba(_vba, _lvl + 1, req._ft.degree);
				if (!check_hash(_blk, _t1_blks[_lvl + 1].nodes[node_idx].hash))
					_mark_req_failed(progress, "hash mismatch");
			} else
				if (!check_hash(_blk, req._ft.hash))
					_mark_req_failed(progress, "hash mismatch");

			Tree_node_index node_idx = t1_node_idx_for_vba(_vba, _lvl, req._ft.degree);
			Type_1_node &t1_node = _t1_blks[_lvl].nodes[node_idx];
			if (t1_node.valid()) {

				_lvl--;
				_old_pbas.pbas [_lvl] = t1_node.pba;
				_old_generations.items[_lvl] = t1_node.gen;
				_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, t1_node.pba, _blk);
				if (VERBOSE_FT_EXTENSION)
					log("  lvl ", _lvl + 1, " node ", node_idx, " (", t1_node, "): load to lvl ", _lvl);
			} else {
				_alloc_lvl = _lvl;
				_add_new_branch_at(_lvl, node_idx);
				if (_old_generations.items[_alloc_lvl] == req._curr_gen) {

					_alloc_pba = _old_pbas.pbas[_alloc_lvl];
					_state = ALLOC_PBA_SUCCEEDED;
					progress = true;
				} else {
					_alloc_pba = _old_pbas.pbas[_alloc_lvl];
					_generate_req<Meta_tree_request>(
						ALLOC_PBA_SUCCEEDED, progress, Meta_tree_request::ALLOC_PBA, req._mt, req._curr_gen, _alloc_pba);
				}
			}
		} else {
			_t2_blk.decode_from_blk(_blk);
			Tree_node_index t1_node_idx = t1_node_idx_for_vba(_vba, _lvl + 1, req._ft.degree);
			if (!check_hash(_blk, _t1_blks[_lvl + 1].nodes[t1_node_idx].hash))
				_mark_req_failed(progress, "hash mismatch");

			Tree_node_index t2_node_idx = t2_node_idx_for_vba(_vba, req._ft.degree);
			if (_t2_blk.nodes[t2_node_idx].valid())
				_mark_req_failed(progress, "t2 node valid");

			_add_new_branch_at(_lvl, t2_node_idx);
			_alloc_lvl = _lvl;
			if (VERBOSE_FT_EXTENSION)
				log("  alloc lvl ", _alloc_lvl);

			_alloc_pba = _old_pbas.pbas[_alloc_lvl];
			_generate_req<Meta_tree_request>(
				ALLOC_PBA_SUCCEEDED, progress, Meta_tree_request::ALLOC_PBA, req._mt, req._curr_gen, _alloc_pba);
		}
		break;

	case ALLOC_PBA_SUCCEEDED:

		_new_pbas.pbas[_alloc_lvl] = _alloc_pba;
		if (_alloc_lvl < req._ft.max_lvl) {

			_alloc_lvl++;
			if (_old_generations.items[_alloc_lvl] == req._curr_gen) {

				_alloc_pba = _old_pbas.pbas[_alloc_lvl];
				_state = ALLOC_PBA_SUCCEEDED;
				progress = true;
			} else {
				_alloc_pba = _old_pbas.pbas[_alloc_lvl];
				_generate_req<Meta_tree_request>(
					ALLOC_PBA_SUCCEEDED, progress, Meta_tree_request::ALLOC_PBA, req._mt, req._curr_gen, _alloc_pba);
			}
		} else {
			_generate_write_blk_req(progress);
			if (VERBOSE_FT_EXTENSION)
				log("  pbas allocated: curr gen ", req._curr_gen);
		}
		break;

	case WRITE_BLK_SUCCEEDED:

		if (_lvl < req._ft.max_lvl) {
			if (_lvl > 1) {
				Tree_node_index node_idx = t1_node_idx_for_vba(_vba, _lvl + 1, req._ft.degree);
				Type_1_node &t1_node { _t1_blks[_lvl + 1].nodes[node_idx] };
				t1_node = { _new_pbas.pbas[_lvl], req._curr_gen };
				calc_hash(_blk, t1_node.hash);
				if (VERBOSE_FT_EXTENSION)
					log("  set lvl ", _lvl + 1, " node ", node_idx, ": ", t1_node);

				_lvl++;
				_generate_write_blk_req(progress);
			} else {
				Tree_node_index node_idx = t1_node_idx_for_vba(_vba, _lvl + 1, req._ft.degree);
				Type_1_node &t1_node = _t1_blks[_lvl + 1].nodes[node_idx];
				t1_node = { _new_pbas.pbas[_lvl], req._curr_gen };
				calc_hash(_blk, t1_node.hash);
				if (VERBOSE_FT_EXTENSION)
					log("  set lvl ", _lvl + 1, " t1_node ", node_idx, ": ", t1_node);

				_lvl++;
				_generate_write_blk_req(progress);
			}
		} else {
			req._ft.t1_node({ _new_pbas.pbas[_lvl], req._curr_gen });
			calc_hash(_blk, req._ft.hash);
			req._ft.num_leaves += _num_leaves;
			_mark_req_successful(progress);
		}
		break;

	default: break;
	}
}


void Free_tree_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = REQ_SUBMITTED;
}


Free_tree::Free_tree()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}
