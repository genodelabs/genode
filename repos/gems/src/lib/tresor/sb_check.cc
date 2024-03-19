/*
 * \brief  Module for checking all hashes of a superblock and its hash trees
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
#include <tresor/sb_check.h>
#include <tresor/hash.h>

using namespace Tresor;

void Sb_check::Check::_check_snap(bool &progress)
{
	Snapshot &snap { _sb.snapshots.items[_snap_idx] };
	if (!snap.valid) {
		_helper.state = CHECK_VBD_SUCCEEDED;
		progress = true;
		return;
	}
	_tree_root.construct(snap.pba, snap.gen, snap.hash, snap.max_level, _sb.degree, snap.nr_of_leaves);
	_check_vbd.generate(_helper, CHECK_VBD, CHECK_VBD_SUCCEEDED, progress, *_tree_root);
	if (VERBOSE_CHECK)
		log("  check snap #", _snap_idx, " (", snap, ")");
}


bool Sb_check::Check::execute(Vbd_check &vbd_check, Ft_check &ft_check, Block_io &block_io, Trust_anchor &trust_anchor)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_read_sb_hash.generate(_helper, READ_SB_HASH, READ_SB_HASH_SUCCEEDED, progress, _hash);
		break;

	case READ_SB_HASH: progress |= _read_sb_hash.execute(trust_anchor); break;
	case READ_SB_HASH_SUCCEEDED:

		_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, _sb_idx, _blk);
		break;

	case READ_BLK: progress |= _read_block.execute(block_io); break;
	case READ_BLK_SUCCEEDED:

		if (check_hash(_blk, _hash)) {
			_sb.decode_from_blk(_blk);
			if (VERBOSE_CHECK)
				log("check superblock (pba ", _sb_idx, " hash ", _hash, ")");

			if (!_sb.valid()) {
				_helper.mark_failed(progress, "superblock marked invalid");;
				break;
			}
			_snap_idx = 0;
			_check_snap(progress);
		} else
			if (_sb_idx < MAX_SUPERBLOCK_INDEX) {
				_sb_idx++;
				_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, _sb_idx, _blk);
			} else
				_helper.mark_failed(progress, "superblock not found");
		break;

	case CHECK_VBD: progress |= _check_vbd.execute(vbd_check, block_io); break;
	case CHECK_VBD_SUCCEEDED:

		if (_snap_idx < MAX_SNAP_IDX) {
			_snap_idx++;
			_check_snap(progress);
		} else {
			_tree_root.construct(_sb.free_number, _sb.free_gen, _sb.free_hash, _sb.free_max_level, _sb.free_degree, _sb.free_leaves);
			_check_ft.generate(_helper, CHECK_FT, CHECK_FT_SUCCEEDED, progress, *_tree_root);
			if (VERBOSE_CHECK)
				log("  check free tree (", _tree_root->t1_node(), ")");
		}
		break;

	case CHECK_FT: progress |= _check_ft.execute(ft_check, block_io); break;
	case CHECK_FT_SUCCEEDED:

		_tree_root.construct(_sb.meta_number, _sb.meta_gen, _sb.meta_hash, _sb.meta_max_level, _sb.meta_degree, _sb.meta_leaves);
		_check_ft.generate(_helper, CHECK_MT, CHECK_MT_SUCCEEDED, progress, *_tree_root);
		if (VERBOSE_CHECK)
			log("  check meta tree (", _tree_root->t1_node(), ")");
		break;

	case CHECK_MT: progress |= _check_ft.execute(ft_check, block_io); break;
	case CHECK_MT_SUCCEEDED: _helper.mark_succeeded(progress); break;
	default: break;
	}
	return progress;
}
