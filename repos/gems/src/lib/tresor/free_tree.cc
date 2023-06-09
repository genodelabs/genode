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
#include <tresor/sha256_4k_hash.h>

using namespace Tresor;


/***************
 ** Utilities **
 ***************/

static Virtual_block_address
vbd_node_lowest_vba(Tree_degree_log_2     vbd_degree_log_2,
                    Tree_level_index      vbd_level,
                    Virtual_block_address vbd_leaf_vba)
{
	return vbd_leaf_vba &
		(0xffff'ffff'ffff'ffff <<
			((uint32_t)vbd_degree_log_2 * (uint32_t)vbd_level));
}


static Number_of_blocks vbd_node_nr_of_vbas(Tree_degree_log_2 vbd_degree_log_2,
                                            Tree_level_index  vbd_level)
{
	return (Number_of_blocks)1 << (vbd_level * vbd_degree_log_2);
}


static Virtual_block_address
vbd_node_highest_vba(Tree_degree_log_2     vbd_degree_log_2,
                     Tree_level_index      vbd_level,
                     Virtual_block_address vbd_leaf_vba)
{
	return
		vbd_node_lowest_vba(vbd_degree_log_2, vbd_level, vbd_leaf_vba) +
		(vbd_node_nr_of_vbas(vbd_degree_log_2, vbd_level) - 1);
}


/***********************
 ** Free_tree_request **
 ***********************/

char const *Free_tree_request::type_to_string(Type type)
{
	switch (type) {
	case INVALID: return "invalid";
	case ALLOC_FOR_NON_RKG: return "alloc_for_non_rkg";
	case ALLOC_FOR_RKG_CURR_GEN_BLKS: return "alloc_for_rkg_curr_gen_blks";
	case ALLOC_FOR_RKG_OLD_GEN_BLKS: return "alloc_for_rkg_old_gen_blks";
	}
	return "?";
}


Free_tree_request::Free_tree_request(uint64_t         src_module_id,
                                     uint64_t         src_request_id,
                                     size_t           req_type,
                                     addr_t           ft_root_pba_ptr,
                                     addr_t           ft_root_gen_ptr,
                                     addr_t           ft_root_hash_ptr,
                                     uint64_t         ft_max_level,
                                     uint64_t         ft_degree,
                                     uint64_t         ft_leaves,
                                     addr_t           mt_root_pba_ptr,
                                     addr_t           mt_root_gen_ptr,
                                     addr_t           mt_root_hash_ptr,
                                     uint64_t         mt_max_level,
                                     uint64_t         mt_degree,
                                     uint64_t         mt_leaves,
                                     Snapshots const *snapshots_ptr,
                                     Generation       last_secured_generation,
                                     uint64_t         current_gen,
                                     uint64_t         free_gen,
                                     uint64_t         requested_blocks,
                                     addr_t           new_blocks_ptr,
                                     addr_t           old_blocks_ptr,
                                     uint64_t         max_level,
                                     uint64_t         vba,
                                     uint64_t         vbd_degree,
                                     uint64_t         vbd_highest_vba,
                                     bool             rekeying,
                                     uint32_t         previous_key_id,
                                     uint32_t         current_key_id,
                                     uint64_t         rekeying_vba)
:
	Module_request           { src_module_id, src_request_id, FREE_TREE },
	_type                    { (Type)req_type },
	_ft_root_pba_ptr         { (addr_t)ft_root_pba_ptr },
	_ft_root_gen_ptr         { (addr_t)ft_root_gen_ptr },
	_ft_root_hash_ptr        { (addr_t)ft_root_hash_ptr },
	_ft_max_level            { ft_max_level },
	_ft_degree               { ft_degree },
	_ft_leaves               { ft_leaves },
	_mt_root_pba_ptr         { (addr_t)mt_root_pba_ptr },
	_mt_root_gen_ptr         { (addr_t)mt_root_gen_ptr },
	_mt_root_hash_ptr        { (addr_t)mt_root_hash_ptr },
	_mt_max_level            { mt_max_level },
	_mt_degree               { mt_degree },
	_mt_leaves               { mt_leaves },
	_current_gen             { current_gen },
	_free_gen                { free_gen },
	_requested_blocks        { requested_blocks },
	_new_blocks_ptr          { (addr_t)new_blocks_ptr },
	_old_blocks_ptr          { (addr_t)old_blocks_ptr },
	_max_level               { max_level },
	_vba                     { vba },
	_vbd_degree              { vbd_degree },
	_vbd_highest_vba         { vbd_highest_vba },
	_rekeying                { rekeying },
	_previous_key_id         { previous_key_id },
	_current_key_id          { current_key_id },
	_rekeying_vba            { rekeying_vba },
	_snapshots_ptr           { (addr_t)snapshots_ptr },
	_last_secured_generation { last_secured_generation }
{ }


/***************
 ** Free_tree **
 ***************/

void Free_tree::execute(bool &progress)
{
	for (Channel &channel : _channels) {
		_execute(
			channel, *(Snapshots *)channel._request._snapshots_ptr,
			channel._request._last_secured_generation, progress);
	}
}


Free_tree::Local_cache_request Free_tree::_new_cache_request(Physical_block_address  pba,
                                                             Local_cache_request::Op op,
                                                             Tree_level_index        lvl)
{
	return Local_cache_request {
		Local_cache_request::PENDING, op, false,
		pba, lvl };
}


void Free_tree::_check_type_2_stack(Type_2_info_stack &stack,
                                    Type_1_info_stack &stack_next,
                                    Node_queue        &leaves,
                                    Number_of_blocks  &found)
{
	if (!stack.empty()) {
		while (!stack.empty()) {
			Type_2_info const info { stack.peek_top() };
			if (!leaves.full()) {
				leaves.enqueue(info);
			}
			found++;
			stack.pop();
		}
	}
	if (!stack_next.empty()) {
		Type_1_info n { stack_next.peek_top() };

		/*
		 * Only when the node is in read state we actually have to acknowledge
		 * checking the leaf nodes.
		 */
		if (n.state == Type_1_info::READ) {
			n.state = Type_1_info::COMPLETE;
			stack_next.update_top(n);
		}
	}
}


void Free_tree::_populate_lower_n_stack(Type_1_info_stack &stack,
                                        Type_1_node_block &entries,
                                        Block      const  &block_data,
                                        Generation         current_gen)
{
	stack.reset();
	entries.decode_from_blk(block_data);

	for (Tree_node_index idx = 0; idx < NR_OF_T1_NODES_PER_BLK; idx++) {

		if (entries.nodes[idx].pba != 0) {

			stack.push({
				Type_1_info::INVALID, entries.nodes[idx], idx,
				_node_volatile(entries.nodes[idx], current_gen) });
		}
	}
}


bool
Free_tree::_check_type_2_leaf_usable(Snapshots       const &snapshots,
                                     Generation             last_secured_gen,
                                     Type_2_node     const &node,
                                     bool                   rekeying,
                                     Key_id                 previous_key_id,
                                     Virtual_block_address  rekeying_vba)
{
	if (node.pba == 0 ||
	    node.pba == INVALID_PBA ||
	    node.free_gen > last_secured_gen)
		return false;

	if (!node.reserved)
		return true;

	if (rekeying &&
	    node.last_key_id == previous_key_id &&
	    node.last_vba < rekeying_vba)
		return true;

	for (Snapshot const &snap : snapshots.items) {
		if (snap.valid &&
		    node.free_gen > snap.gen &&
		    node.alloc_gen < snap.gen + 1)
			return false;
	}
	return true;
}


void Free_tree::_populate_level_0_stack(Type_2_info_stack     &stack,
                                        Type_2_node_block     &entries,
                                        Block           const &block_data,
                                        Snapshots       const &active_snaps,
                                        Generation             secured_gen,
                                        bool                   rekeying,
                                        Key_id                 previous_key_id,
                                        Virtual_block_address  rekeying_vba)
{
	stack.reset();
	entries.decode_from_blk(block_data);

	for (Tree_node_index idx = 0; idx < NR_OF_T1_NODES_PER_BLK; idx++) {
		if (_check_type_2_leaf_usable(active_snaps, secured_gen,
		                              entries.nodes[idx], rekeying,
		                              previous_key_id, rekeying_vba)) {

			stack.push({ Type_2_info::INVALID, entries.nodes[idx], idx });
		}
	}
}


void Free_tree::_execute_scan(Channel         &chan,
                              Snapshots const &active_snaps,
                              Generation       last_secured_gen,
                              bool            &progress)
{
	Request &req { chan._request };
	bool end_of_tree  = false;
	bool enough_found = false;

	// handle level 0
	{
		Number_of_blocks     found_blocks = 0;
		_check_type_2_stack(
			chan._level_0_stack, chan._level_n_stacks[FIRST_LVL_N_STACKS_IDX],
			chan._type_2_leafs, found_blocks);

		chan._found_blocks += found_blocks;
	}

	// handle level 1 - n
	for (Tree_level_index lvl = FIRST_LVL_N_STACKS_IDX; lvl <= MAX_LVL_N_STACKS_IDX; lvl++) {

		if (!chan._level_n_stacks[lvl].empty()) {

			Type_1_info t1_info = chan._level_n_stacks[lvl].peek_top();
			switch (t1_info.state) {
			case Type_1_info::INVALID:

				if (chan._cache_request.state != Local_cache_request::INVALID) {
					class Exception_1 { };
					throw Exception_1 { };
				}
				chan._cache_request = _new_cache_request(
					t1_info.node.pba, Local_cache_request::READ, lvl);

				progress = true;
				break;

			case Type_1_info::AVAILABLE:

				chan._cache_request.state = Local_cache_request::INVALID;
				if (lvl >= 2) {
					_populate_lower_n_stack(
						chan._level_n_stacks[lvl - 1],
						chan._level_n_node, chan._cache_block_data,
						req._current_gen);
				} else {
					_populate_level_0_stack(
						chan._level_0_stack,
						chan._level_0_node, chan._cache_block_data,
						active_snaps, last_secured_gen,
						req._rekeying, req._previous_key_id,
						req._rekeying_vba);
				}
				t1_info.state = Type_1_info::READ;
				chan._level_n_stacks[lvl].update_top(t1_info);
				progress = true;
				break;

			case Type_1_info::READ:

				t1_info.state = Type_1_info::COMPLETE;
				chan._level_n_stacks[lvl].update_top(t1_info);
				progress = true;
				break;

			case Type_1_info::WRITE:

				class Exception_1 { };
				throw Exception_1 { };

			case Type_1_info::COMPLETE:

				if (lvl == req._ft_max_level)
					end_of_tree = true;

				if (chan._found_blocks >= chan._needed_blocks)
					enough_found = true;

				chan._level_n_stacks[lvl].pop();
				progress = true;
				break;
			}
			break;
		}
	}

	if (chan._state != Channel::SCAN)
		return;

	if (enough_found) {
		chan._state = Channel::SCAN_COMPLETE;

		for (Type_1_info_stack &stack : chan._level_n_stacks)
			stack = { };

		for (Type_1_node_block &blk : chan._level_n_nodes)
			blk = { };

		chan._level_n_stacks[req._ft_max_level].push(
			Type_1_info {
				Type_1_info::INVALID, chan._root_node(), 0,
				_node_volatile(chan._root_node(), req._current_gen) });
	}

	if (end_of_tree && !enough_found)
		chan._state = Channel::NOT_ENOUGH_FREE_BLOCKS;
}


void
Free_tree::_exchange_type_2_leaves(Generation              free_gen,
                                   Tree_level_index        max_level,
                                   Type_1_node_walk const &old_blocks,
                                   Tree_walk_pbas         &new_blocks,
                                   Virtual_block_address   vba,
                                   Tree_degree_log_2       vbd_degree_log_2,
                                   Request::Type           req_type,
                                   Type_2_info_stack      &stack,
                                   Type_2_node_block      &entries,
                                   Number_of_blocks       &exchanged,
                                   bool                   &handled,
                                   Virtual_block_address   vbd_highest_vba,
                                   bool                    rekeying,
                                   Key_id                  previous_key_id,
                                   Key_id                  current_key_id,
                                   Virtual_block_address   rekeying_vba)
{
	Number_of_blocks local_exchanged { 0 };
	handled = false;

	for (Tree_level_index i = 0; i <= max_level; i++) {

		if (new_blocks.pbas[i] == 0) {

			if (!stack.empty()) {

				Type_2_info const info { stack.peek_top() };
				Type_2_node &t2_node { entries.nodes[info.index] };
				if (t2_node.pba != info.node.pba) {
					class Exception_1 { };
					throw Exception_1 { };
				}
				switch (req_type) {
				case Request::ALLOC_FOR_NON_RKG:

					new_blocks.pbas[i] = t2_node.pba;
					t2_node.pba       = old_blocks.nodes[i].pba;
					t2_node.alloc_gen = old_blocks.nodes[i].gen;
					t2_node.free_gen  = free_gen;
					t2_node.last_vba  =
						vbd_node_lowest_vba(vbd_degree_log_2, i, vba);

					if (rekeying) {

						if (vba < rekeying_vba)
							t2_node.last_key_id = current_key_id;
						else
							t2_node.last_key_id = previous_key_id;

					} else {

						t2_node.last_key_id = current_key_id;
					}
					t2_node.reserved = true;
					break;

				case Request::ALLOC_FOR_RKG_CURR_GEN_BLKS:

					new_blocks.pbas[i] = t2_node.pba;

					t2_node.pba       = old_blocks.nodes[i].pba;
					t2_node.alloc_gen = old_blocks.nodes[i].gen;
					t2_node.free_gen  = free_gen;
					t2_node.last_vba  =
						vbd_node_lowest_vba (vbd_degree_log_2, i, vba);

					t2_node.last_key_id = previous_key_id;
					t2_node.reserved = false;
					break;

				case Request::ALLOC_FOR_RKG_OLD_GEN_BLKS:
				{
					new_blocks.pbas[i] = t2_node.pba;

					t2_node.alloc_gen = old_blocks.nodes[i].gen;
					t2_node.free_gen  = free_gen;

					Virtual_block_address const node_highest_vba {
						vbd_node_highest_vba(vbd_degree_log_2, i, vba) };

					if (rekeying_vba < node_highest_vba &&
					    rekeying_vba < vbd_highest_vba)
					{
						t2_node.last_key_id = previous_key_id;
						t2_node.last_vba    = rekeying_vba + 1;

					} else if (rekeying_vba == node_highest_vba ||
					           rekeying_vba == vbd_highest_vba) {

						t2_node.last_key_id = current_key_id;
						t2_node.last_vba    =
							vbd_node_lowest_vba (vbd_degree_log_2, i, vba);

					} else {

						class Exception_1 { };
						throw Exception_1 { };
					}
					t2_node.reserved = true;
					break;
				}
				default:

					class Exception_2 { };
					throw Exception_2 { };
				}

				local_exchanged = local_exchanged + 1;
				stack.pop();
				handled = true;

			} else {

				break;
			}
		}
	}
	exchanged = local_exchanged;
}


Free_tree::Local_meta_tree_request
Free_tree::_new_meta_tree_request(Physical_block_address pba)
{
	return {
		Local_meta_tree_request::PENDING, Local_meta_tree_request::READ, pba };
}


void Free_tree::_update_upper_n_stack(Type_1_info const &t,
                                      Generation         gen,
                                      Block       const &block_data,
                                      Type_1_node_block &entries)
{
	entries.nodes[t.index].pba = t.node.pba;
	entries.nodes[t.index].gen = gen;
	calc_sha256_4k_hash(block_data, entries.nodes[t.index].hash);
}


void Free_tree::_execute_update(Channel         &chan,
                                Snapshots const &active_snaps,
                                Generation       last_secured_gen,
                                bool            &progress)
{
	Request &req { chan._request };
	bool exchange_finished { false };
	bool update_finished { false };
	Number_of_blocks exchanged;

	/* handle level 0 */
	{
		bool handled;

		_exchange_type_2_leaves(
			req._free_gen, (Tree_level_index)req._max_level,
			*(Type_1_node_walk *)req._old_blocks_ptr,
			*(Tree_walk_pbas *)req._new_blocks_ptr,
			req._vba, (Tree_degree_log_2)chan._vbd_degree_log_2, req._type, chan._level_0_stack,
			chan._level_0_node, exchanged, handled, req._vbd_highest_vba,
			req._rekeying, req._previous_key_id, req._current_key_id,
			req._rekeying_vba);

		if (handled) {
			if (exchanged > 0) {
				chan._exchanged_blocks += exchanged;
			} else {
				Type_1_info n { chan._level_n_stacks[FIRST_LVL_N_STACKS_IDX].peek_top() };
				n.state = Type_1_info::COMPLETE;
				chan._level_n_stacks[FIRST_LVL_N_STACKS_IDX].update_top(n);
			}
		}
	}
	if (chan._exchanged_blocks == chan._needed_blocks) {
		exchange_finished = true;
	}
	/* handle level 1..N */
	for (Tree_level_index l { FIRST_LVL_N_STACKS_IDX }; l <= MAX_LVL_N_STACKS_IDX; l++) {

		Type_1_info_stack &stack { chan._level_n_stacks[l] };

		if (!stack.empty()) {

			Type_1_info n { stack.peek_top() };
			switch (n.state) {
			case Type_1_info::INVALID:

				if (chan._cache_request.state != Local_cache_request::INVALID) {
					class Exception_1 { };
					throw Exception_1 { };
				}
				chan._cache_request =
					_new_cache_request(
						n.node.pba, Local_cache_request::READ, l);

				progress = true;
				break;

			case Type_1_info::AVAILABLE:

				chan._cache_request.state = Local_cache_request::INVALID;
				if (l >= 2) {

					_populate_lower_n_stack(
						chan._level_n_stacks[l - 1],
						chan._level_n_nodes[l - 1], chan._cache_block_data,
						req._current_gen);

					if (!chan._level_n_stacks[l - 1].empty())
						n.state = Type_1_info::WRITE;
					else
						n.state = Type_1_info::COMPLETE;

				} else {

					_populate_level_0_stack(
						chan._level_0_stack, chan._level_0_node,
						chan._cache_block_data, active_snaps, last_secured_gen,
						req._rekeying, req._previous_key_id,
						req._rekeying_vba);

					if (!chan._level_0_stack.empty())
						n.state = Type_1_info::WRITE;
					else
						n.state = Type_1_info::COMPLETE;
				}
				stack.update_top(n);
				progress = true;
				break;

			case Type_1_info::READ:

				class Exception_2 { };
				throw Exception_2 { };

			case Type_1_info::WRITE:

				if (!n.volatil) {

					Local_meta_tree_request &mtr { chan._meta_tree_request };
					if (mtr.state == Local_meta_tree_request::INVALID) {

						mtr = _new_meta_tree_request(n.node.pba);
						progress = true;
						break;

					} else if (mtr.state == Local_meta_tree_request::COMPLETE) {

						mtr.state = Local_meta_tree_request::INVALID;
						n.volatil = true;
						n.node.pba = mtr.pba;
						stack.update_top(n);

					} else {

						class Exception_3 { };
						throw Exception_3 { };
					}
				}
				if (l >= 2) {

					chan._level_n_nodes[l - 1].encode_to_blk(chan._cache_block_data);

					if (l < req._ft_max_level) {
						_update_upper_n_stack(
							n, req._current_gen, chan._cache_block_data,
							chan._level_n_nodes[l]);
					} else {
						calc_sha256_4k_hash(chan._cache_block_data,
						                    *(Hash *)req._ft_root_hash_ptr);

						*(Generation *)req._ft_root_gen_ptr = req._current_gen;
						*(Physical_block_address *)req._ft_root_pba_ptr =
							n.node.pba;
					}
				} else {
					chan._level_0_node.encode_to_blk(chan._cache_block_data);

					_update_upper_n_stack(
						n, req._current_gen, chan._cache_block_data,
						chan._level_n_nodes[l]);
				}
				chan._cache_request = _new_cache_request(
					n.node.pba, Local_cache_request::WRITE, l);

				progress = true;
				break;

			case Type_1_info::COMPLETE:

				chan._cache_request.state = Local_cache_request::INVALID;
				stack.pop();

				if (exchange_finished)
					while (!stack.empty())
						stack.pop();

				if (l == req._ft_max_level)
					update_finished = true;

				progress = true;
				break;
			}
			break;
		}
	}
	if (chan._state != Channel::UPDATE)
		return;

	if (exchange_finished && update_finished)
		chan._state = Channel::UPDATE_COMPLETE;
}


void Free_tree::_mark_req_failed(Channel    &chan,
                                 bool       &progress,
                                 char const *str)
{
	error(chan._request.type_name(), " request failed, reason: \"", str, "\"");
	chan._request._success = false;
	chan._state = Channel::COMPLETE;
	progress = true;
}


void Free_tree::_mark_req_successful(Channel &channel,
                                     bool    &progress)
{
	channel._request._success = true;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Free_tree::_execute(Channel         &chan,
                         Snapshots const &active_snaps,
                         Generation       last_secured_gen,
                         bool            &progress)
{
	if (chan._meta_tree_request.state == Local_meta_tree_request::PENDING ||
	    chan._meta_tree_request.state == Local_meta_tree_request::IN_PROGRESS)
		return;

	if (chan._cache_request.state == Local_cache_request::PENDING ||
	    chan._cache_request.state == Local_cache_request::IN_PROGRESS)
		return;

	switch (chan._state) {
	case Channel::INVALID:
		break;
	case Channel::SCAN:
		_execute_scan(chan, active_snaps, last_secured_gen, progress);
		break;
	case Channel::SCAN_COMPLETE:
		chan._state = Channel::UPDATE;
		progress = true;
		break;
	case Channel::UPDATE:
		_execute_update(chan, active_snaps, last_secured_gen, progress);
		break;
	case Channel::UPDATE_COMPLETE:
		_mark_req_successful(chan, progress);
		break;
	case Channel::COMPLETE:
		break;
	case Channel::NOT_ENOUGH_FREE_BLOCKS:
		_mark_req_failed(chan, progress, "not enough free blocks");
		break;
	case Channel::TREE_HASH_MISMATCH:
		_mark_req_failed(chan, progress, "node hash mismatch");
		break;
	}
}


bool Free_tree::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::INVALID)
			return true;
	}
	return false;
}


void Free_tree::_reset_block_state(Channel &chan)
{
	Request &req { chan._request };
	chan._needed_blocks = req._requested_blocks;
	chan._found_blocks = 0;
	for (Type_1_info_stack &stack : chan._level_n_stacks)
		stack = { };

	for (Type_1_node_block &blk : chan._level_n_nodes)
		blk = { };

	chan._level_0_stack = { };
	chan._level_n_node = { };
	chan._level_0_node = { };
}


bool Free_tree::_node_volatile(Type_1_node const &node,
                               uint64_t           gen)
{
   return node.gen == 0 || node.gen == gen;
}


void Free_tree::submit_request(Module_request &mod_req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		Channel &chan { _channels[id] };
		if (chan._state == Channel::INVALID) {

			mod_req.dst_request_id(id);

			chan._request = *static_cast<Request *>(&mod_req);
			chan._exchanged_blocks = 0;
			_reset_block_state(chan);

			Request &req { chan._request };
			Type_1_node root_node { };
			root_node.pba = *(uint64_t *)req._ft_root_pba_ptr;
			root_node.gen = *(uint64_t *)req._ft_root_gen_ptr;
			memcpy(&root_node.hash, (void *)req._ft_root_hash_ptr,
			       HASH_SIZE);

			chan._level_n_stacks[req._ft_max_level].push(
				{ Type_1_info::INVALID, root_node, 0,
				  _node_volatile(root_node, req._current_gen) });

			chan._state = Channel::SCAN;
			chan._vbd_degree_log_2 = log2<uint64_t>(req._vbd_degree);
			return;
		}
	}
	class Exception_1 { };
	throw Exception_1 { };
}

bool Free_tree::_peek_generated_request(uint8_t *buf_ptr,
                                        size_t   buf_size)
{
	for (uint32_t id { 0 }; id < NR_OF_CHANNELS; id++) {

		Channel &channel { _channels[id] };
		Local_cache_request const &local_crq { channel._cache_request };
		if (local_crq.state == Local_cache_request::PENDING) {

			Block_io_request::Type blk_io_req_type {
				local_crq.op == Local_cache_request::READ ?
				                   Block_io_request::READ :
				                Local_cache_request::WRITE ?
				                   Block_io_request::WRITE :
				                   Block_io_request::INVALID };

			if (blk_io_req_type == Block_io_request::INVALID) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			construct_in_buf<Block_io_request>(
				buf_ptr, buf_size, FREE_TREE, id, blk_io_req_type, 0, 0, 0,
				local_crq.pba, 0, 1, &channel._cache_block_data, nullptr);

			return true;
		}

		Local_meta_tree_request const &local_mtr { channel._meta_tree_request };
		if (local_mtr.state == Local_meta_tree_request::PENDING) {

			Meta_tree_request::Type mt_req_type {
				local_mtr.op == Local_meta_tree_request::READ ?
				                      Meta_tree_request::UPDATE :
				                      Meta_tree_request::INVALID };

			if (mt_req_type == Meta_tree_request::INVALID) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			Meta_tree_request::create(
				buf_ptr, buf_size, FREE_TREE, id, mt_req_type,
				(void*)channel._request._mt_root_pba_ptr,
				(void*)channel._request._mt_root_gen_ptr,
				(void*)channel._request._mt_root_hash_ptr,
				channel._request._mt_max_level,
				channel._request._mt_degree,
				channel._request._mt_leaves,
				channel._request._current_gen,
				local_mtr.pba);

			return true;
		}
	}
	return false;
}

void Free_tree::_drop_generated_request(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	switch (mod_req.dst_module_id()) {
	case BLOCK_IO:
	{
		Local_cache_request &local_req { _channels[id]._cache_request };
		if (local_req.state != Local_cache_request::PENDING) {
			class Exception_2 { };
			throw Exception_2 { };
		}
		local_req.state = Local_cache_request::IN_PROGRESS;
		break;
	}
	case META_TREE:
	{
		Local_meta_tree_request &local_req { _channels[id]._meta_tree_request };
		if (local_req.state != Local_meta_tree_request::PENDING) {
			class Exception_3 { };
			throw Exception_3 { };
		}
		local_req.state = Local_meta_tree_request::IN_PROGRESS;
		break;
	}
	default:

		class Exception_4 { };
		throw Exception_4 { };
	}
}

void Free_tree::generated_request_complete(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	switch (mod_req.dst_module_id()) {
	case BLOCK_IO:
	{
		Local_cache_request &local_req { _channels[id]._cache_request };
		if (local_req.state != Local_cache_request::IN_PROGRESS) {
			class Exception_2 { };
			throw Exception_2 { };
		}
		Block_io_request &blk_io_req { *static_cast<Block_io_request *>(&mod_req) };
		Channel &channel { _channels[id] };
		if (!blk_io_req.success()) {
			class Exception_3 { };
			throw Exception_3 { };
		}
		Type_1_info n { channel._level_n_stacks[local_req.level].peek_top() };
		local_req.state = Local_cache_request::COMPLETE;

		switch (local_req.op) {
		case Local_cache_request::SYNC:

			class Exception_4 { };
			throw Exception_4 { };

		case Local_cache_request::READ:

			if (check_sha256_4k_hash(channel._cache_block_data, n.node.hash)) {

				n.state = Type_1_info::AVAILABLE;
				channel._level_n_stacks[local_req.level].update_top(n);

			} else {

				channel._state = Channel::TREE_HASH_MISMATCH;
			}
			break;

		case Local_cache_request::WRITE:

			n.state = Type_1_info::COMPLETE;
			channel._level_n_stacks[local_req.level].update_top(n);
			break;
		}
		break;
	}
	case META_TREE:
	{
		Local_meta_tree_request &local_req { _channels[id]._meta_tree_request };
		if (local_req.state != Local_meta_tree_request::IN_PROGRESS) {
			class Exception_5 { };
			throw Exception_5 { };
		}
		Meta_tree_request &mt_req { *static_cast<Meta_tree_request *>(&mod_req) };
		if (!mt_req.success()) {
			class Exception_6 { };
			throw Exception_6 { };
		}
		local_req.pba = mt_req.new_pba();
		local_req.state = Local_meta_tree_request::COMPLETE;
		break;
	}
	default:

		class Exception_7 { };
		throw Exception_7 { };
	}
}


bool Free_tree::_peek_completed_request(uint8_t *buf_ptr,
                                        size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::COMPLETE) {
			if (sizeof(channel._request) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			memcpy(buf_ptr, &channel._request, sizeof(channel._request));
			return true;
		}
	}
	return false;
}


void Free_tree::_drop_completed_request(Module_request &req)
{
	Module_request_id id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	if (_channels[id]._state != Channel::COMPLETE) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	_channels[id]._state = Channel::INVALID;
}
