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
#include <tresor/hash.h>

using namespace Tresor;

bool Ft_check::Check::_execute_node(Block_io &block_io, Tree_level_index lvl, Tree_node_index node_idx, bool &progress)
{
	bool &check_node { _check_node[lvl][node_idx] };

	if (check_node == false)
		return false;

	switch (_helper.state) {
	case IN_PROGRESS:

		if (lvl == 1) {
			Type_2_node const &node { _t2_blk.nodes[node_idx] };
			if (!_num_remaining_leaves) {
				if (node.valid()) {
					_helper.mark_failed(progress, { "lvl ", lvl, " node ", node_idx, " (", node,
					                                ") valid but no leaves remaining" });
					break;
				}
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, _attr.in_ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " unused");
				break;
			}
			_num_remaining_leaves--;
			check_node = false;
			progress = true;
			if (VERBOSE_CHECK)
				log(Level_indent { lvl, _attr.in_ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " done");
		} else {
			Type_1_node const &node { _t1_blks.items[lvl].nodes[node_idx] };
			if (!node.valid()) {
				if (_num_remaining_leaves) {
					_helper.mark_failed(progress, { "lvl ", lvl, " node ", node_idx, " invalid but ",
					                                _num_remaining_leaves, " leaves remaining" });
					break;
				}
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, _attr.in_ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " unused");
				break;
			}
			_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, node.pba, _blk);
			if (VERBOSE_CHECK)
				log(Level_indent { lvl, _attr.in_ft.max_lvl }, "    lvl ", lvl, " node ", node_idx,
				    " (", node, "): load to lvl ", lvl - 1);
		}
		break;

	case READ_BLK: progress |= _read_block.execute(block_io); break;
	case READ_BLK_SUCCEEDED:
	{
		Type_1_node const &node { _t1_blks.items[lvl].nodes[node_idx] };
		if (node.gen != INITIAL_GENERATION && !check_hash(_blk, node.hash)) {
			_helper.mark_failed(progress, { "lvl ", lvl, " node ", node_idx, " (", node, ") has bad hash" });
			break;
		}
		if (lvl == 2)
			_t2_blk.decode_from_blk(_blk);
		else
			_t1_blks.items[lvl - 1].decode_from_blk(_blk);
		for (bool &cn : _check_node[lvl - 1])
			cn = true;

		_helper.state = IN_PROGRESS;
		check_node = false;
		progress = true;
		if (VERBOSE_CHECK)
			log(Level_indent { lvl, _attr.in_ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " has good hash");
		break;
	}
	default: break;
	}
	return true;
}


bool Ft_check::Check::execute(Block_io &block_io)
{
	bool progress = false;
	if (_helper.state == INIT) {
		for (Tree_level_index lvl { 1 }; lvl <= _attr.in_ft.max_lvl + 1; lvl++)
			for (Tree_node_index node_idx { 0 }; node_idx < _attr.in_ft.degree; node_idx++)
				_check_node[lvl][node_idx] = false;

		_num_remaining_leaves = _attr.in_ft.num_leaves;
		_t1_blks.items[_attr.in_ft.max_lvl + 1].nodes[0] = _attr.in_ft.t1_node();
		_check_node[_attr.in_ft.max_lvl + 1][0] = true;
		_helper.state = IN_PROGRESS;
	}
	for (Tree_level_index lvl { 1 }; lvl <= _attr.in_ft.max_lvl + 1; lvl++)
		for (Tree_node_index node_idx { 0 }; node_idx < _attr.in_ft.degree; node_idx++)
			if (_execute_node(block_io, lvl, node_idx, progress))
				return progress;

	_helper.mark_succeeded(progress);
	return progress;
}
