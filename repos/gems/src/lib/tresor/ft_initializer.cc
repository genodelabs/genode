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
#include <tresor/hash.h>
#include <tresor/ft_initializer.h>

using namespace Tresor;

void Ft_initializer::Initialize::_reset_level(Tree_level_index lvl, Node_state node_state)
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


bool Ft_initializer::Initialize::execute(Block_io &block_io)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_num_remaining_leaves = _attr.in_tree_cfg.num_leaves;
		for (Tree_level_index lvl = 0; lvl < TREE_MAX_LEVEL; lvl++)
			_reset_level(lvl, DONE);

		_t1_node_states[_attr.in_tree_cfg.max_lvl + 1][0] = INIT_BLOCK;
		_helper.state = EXECUTE_NODES;
		progress = true;
		break;

	case EXECUTE_NODES:

		for (Tree_node_index node_idx = 0; node_idx < _attr.in_tree_cfg.degree; node_idx++)
			if (_execute_t2_node(node_idx, progress))
				return progress;

		for (Tree_level_index lvl = 1; lvl <= _attr.in_tree_cfg.max_lvl + 1; lvl++)
			for (Tree_node_index node_idx = 0; node_idx < _attr.in_tree_cfg.degree; node_idx++)
				if (_execute_t1_node(lvl, node_idx, progress))
					return progress;

		if (_num_remaining_leaves)
			_helper.mark_failed(progress, "leaves remaining");
		else {
			_attr.out_tree_root = _t1_blks.items[_attr.in_tree_cfg.max_lvl + 1].nodes[0];
			_helper.mark_succeeded(progress);
		}
		break;

	case WRITE_BLOCK: progress |= _write_block.execute(block_io); break;
	default: break;;
	}
	return progress;
}

bool Ft_initializer::Initialize::_execute_t2_node(Tree_node_index node_idx, bool &progress)
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
			if (!_attr.in_out_pba_alloc.alloc(node.pba)) {
				_helper.mark_failed(progress, "allocate pba");
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

	case WRITING_BLOCK: ASSERT_NEVER_REACHED;
	}
	return true;
}


bool Ft_initializer::Initialize::_execute_t1_node(Tree_level_index lvl, Tree_node_index node_idx, bool &progress)
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
		if (!_attr.in_out_pba_alloc.alloc(node.pba)) {
			_helper.mark_failed(progress, "allocate pba");
			break;
		}
		if (lvl == 2)
			_t2_blk.encode_to_blk(_blk);
		else
			_t1_blks.items[lvl - 1].encode_to_blk(_blk);
		calc_hash(_blk, node.hash);
		_write_block.generate(_helper, WRITE_BLOCK, EXECUTE_NODES, progress, node.pba, _blk);
		node_state = WRITING_BLOCK;
		if (VERBOSE_FT_INIT)
			log("[ft_init] node: ", lvl, " ", node_idx, " assign pba: ", node.pba);
		break;
	}
	case WRITING_BLOCK:

		node_state = DONE;
		progress = true;
		if (VERBOSE_FT_INIT)
			log("[ft_init] node: ", lvl, " ", node_idx, " write pba: ", node.pba, " level: ", lvl - 1, " (node: ", node, ")");
		break;
	}
	return true;
}
