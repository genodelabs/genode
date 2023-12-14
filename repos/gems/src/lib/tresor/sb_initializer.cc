/*
 * \brief  Module for initializing the superblocks of a new Tresor
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2023-03-14
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/hash.h>
#include <tresor/sb_initializer.h>

using namespace Tresor;

bool Sb_initializer::Initialize::execute(Block_io &block_io, Trust_anchor &trust_anchor, Vbd_initializer &vbd_initializer, Ft_initializer &ft_initializer)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:
	{
		_init_vbd.generate(_helper, INIT_VBD, INIT_VBD_SUCCEEDED, progress, _attr.in_vbd_cfg, _vbd_root, _attr.in_out_pba_alloc);
		break;
	}
	case INIT_VBD: progress |= _init_vbd.execute(vbd_initializer, block_io); break;
	case INIT_VBD_SUCCEEDED:

		_init_ft.generate(_helper, INIT_FT, INIT_FT_SUCCEEDED, progress, _attr.in_ft_cfg, _ft_root, _attr.in_out_pba_alloc);
		break;

	case INIT_FT: progress |= _init_ft.execute(ft_initializer, block_io); break;
	case INIT_FT_SUCCEEDED:

		_init_ft.generate(_helper, INIT_FT, INIT_MT_SUCCEEDED, progress, _attr.in_mt_cfg, _mt_root, _attr.in_out_pba_alloc);
		break;

	case INIT_MT_SUCCEEDED:

		_generate_key.generate(_helper, GENERATE_KEY, GENERATE_KEY_SUCCEEDED, progress, _sb.current_key.value);
		break;

	case GENERATE_KEY: progress |= _generate_key.execute(trust_anchor); break;
	case GENERATE_KEY_SUCCEEDED:

		_encrypt_key.generate(_helper, ENCRYPT_KEY, ENCRYPT_KEY_SUCCEEDED, progress, _sb.current_key.value, _sb.current_key.value);
		break;

	case ENCRYPT_KEY: progress |= _encrypt_key.execute(trust_anchor); break;
	case ENCRYPT_KEY_SUCCEEDED:
	{
		Snapshot &snap = _sb.snapshots.items[0];
		snap.pba = _vbd_root.pba;
		snap.gen = _vbd_root.gen;
		snap.hash = _vbd_root.hash;
		snap.nr_of_leaves = _attr.in_vbd_cfg.num_leaves;
		snap.max_level = _attr.in_vbd_cfg.max_lvl;
		snap.valid = true;
		snap.id = 0;
		_sb.current_key.id = 1;
		_sb.state = Superblock::NORMAL;
		_sb.degree = _attr.in_vbd_cfg.degree;
		_sb.first_pba = _attr.in_out_pba_alloc.first_pba() - NR_OF_SUPERBLOCK_SLOTS;
		_sb.nr_of_pbas = _attr.in_out_pba_alloc.num_used_pbas() + NR_OF_SUPERBLOCK_SLOTS;
		_sb.free_number = _ft_root.pba;
		_sb.free_gen = _ft_root.gen;
		_sb.free_hash = _ft_root.hash;
		_sb.free_max_level = _attr.in_ft_cfg.max_lvl;
		_sb.free_degree = _attr.in_ft_cfg.degree;
		_sb.free_leaves = _attr.in_ft_cfg.num_leaves;
		_sb.meta_number = _mt_root.pba;
		_sb.meta_gen = _mt_root.gen;
		_sb.meta_hash = _mt_root.hash;
		_sb.meta_max_level = _attr.in_mt_cfg.max_lvl;
		_sb.meta_degree = _attr.in_mt_cfg.degree;
		_sb.meta_leaves = _attr.in_mt_cfg.num_leaves;
		_sb.encode_to_blk(_blk);
		_write_block.generate(_helper, WRITE_BLK, WRITE_BLK_SUCCEEDED, progress, _sb_idx, _blk);
		break;
	}
	case WRITE_BLK: progress |= _write_block.execute(block_io); break;
	case WRITE_BLK_SUCCEEDED:

		_sync_block_io.generate(_helper, SYNC_BLOCK_IO, _sb_idx ? SB_COMPLETE : WRITE_HASH_TO_TA, progress);
		break;

	case SYNC_BLOCK_IO: progress |= _sync_block_io.execute(block_io); break;
	case WRITE_HASH_TO_TA:

		calc_hash(_blk, _hash);
		_write_sb_hash.generate(_helper, WRITE_SB_HASH, SB_COMPLETE, progress, _hash);
		break;

	case WRITE_SB_HASH: progress |= _write_sb_hash.execute(trust_anchor); break;
	case SB_COMPLETE:

		if (_sb_idx < NR_OF_SUPERBLOCK_SLOTS - 1) {
			_sb_idx++;
			_sb = { };
			_sb.encode_to_blk(_blk);
			_write_block.generate(_helper, WRITE_BLK, WRITE_BLK_SUCCEEDED, progress, _sb_idx, _blk);
		} else
			_helper.mark_succeeded(progress);
		break;

	default: break;
	}
	return progress;
}
