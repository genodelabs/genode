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
#include <tresor/block_io.h>
#include <tresor/vbd_initializer.h>
#include <tresor/ft_initializer.h>
#include <tresor/trust_anchor.h>
#include <tresor/sb_initializer.h>

using namespace Tresor;

Sb_initializer_request::
Sb_initializer_request(Module_id src_mod, Module_channel_id src_chan, Tree_level_index vbd_max_lvl,
                       Tree_degree vbd_degree, Number_of_leaves vbd_num_leaves, Tree_level_index ft_max_lvl,
                       Tree_degree ft_degree, Number_of_leaves ft_num_leaves, Tree_level_index mt_max_lvl,
                       Tree_degree mt_degree, Number_of_leaves mt_num_leaves, Pba_allocator &pba_alloc, bool &success)
:
	Module_request { src_mod, src_chan, SB_INITIALIZER }, _vbd_max_lvl { vbd_max_lvl },
	_vbd_degree { vbd_degree }, _vbd_num_leaves { vbd_num_leaves }, _ft_max_lvl { ft_max_lvl },
	_ft_degree { ft_degree }, _ft_num_leaves { ft_num_leaves }, _mt_max_lvl { mt_max_lvl },
	_mt_degree { mt_degree }, _mt_num_leaves { mt_num_leaves }, _pba_alloc { pba_alloc }, _success { success }
{ }


void Sb_initializer_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("sb initializer: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Sb_initializer_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = REQ_SUBMITTED;
}


void Sb_initializer_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Sb_initializer_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:
	{
		_sb_idx = 0;
		_sb = { };
		Snapshot &snap = _sb.snapshots.items[0];
		_vbd.construct(snap.pba, snap.gen, snap.hash, req._vbd_max_lvl, req._vbd_degree, req._vbd_num_leaves);
		_generate_req<Vbd_initializer_request>(INIT_VBD_SUCCEEDED, progress, *_vbd, req._pba_alloc);
		progress = true;
		break;
	}
	case INIT_VBD_SUCCEEDED:

		_ft.construct(_sb.free_number, _sb.free_gen, _sb.free_hash, req._ft_max_lvl, req._ft_degree, req._ft_num_leaves);
		_generate_req<Ft_initializer_request>(INIT_FT_SUCCEEDED, progress, *_ft, req._pba_alloc);
		break;

	case INIT_FT_SUCCEEDED:

		_mt.construct(_sb.meta_number, _sb.meta_gen, _sb.meta_hash, req._ft_max_lvl, req._ft_degree, req._ft_num_leaves);
		_generate_req<Ft_initializer_request>(INIT_MT_SUCCEEDED, progress, *_mt, req._pba_alloc);
		break;

	case INIT_MT_SUCCEEDED:

		_generate_req<Trust_anchor::Create_key>(CREATE_KEY_SUCCEEDED, progress, _sb.current_key.value);
		break;

	case CREATE_KEY_SUCCEEDED:

		_generate_req<Trust_anchor::Encrypt_key>(ENCRYPT_KEY_SUCCEEDED, progress, _sb.current_key.value, _sb.current_key.value);
		break;

	case ENCRYPT_KEY_SUCCEEDED:
	{
		Snapshot &snap = _sb.snapshots.items[0];
		snap.gen = 0;
		snap.nr_of_leaves = req._vbd_num_leaves;
		snap.max_level = req._vbd_max_lvl;
		snap.valid = true;
		snap.id = 0;
		_sb.current_key.id = 1;
		_sb.state = Superblock::NORMAL;
		_sb.degree = req._vbd_degree;
		_sb.first_pba = req._pba_alloc.first_pba() - NR_OF_SUPERBLOCK_SLOTS;
		_sb.nr_of_pbas = req._pba_alloc.num_used_pbas() + NR_OF_SUPERBLOCK_SLOTS;
		_sb.free_max_level = _ft->max_lvl;
		_sb.free_degree = _ft->degree;
		_sb.free_leaves = _ft->num_leaves;
		_sb.meta_max_level = _mt->max_lvl;
		_sb.meta_degree = _mt->degree;
		_sb.meta_leaves = _mt->num_leaves;
		_sb.encode_to_blk(_blk);
		_generate_req<Block_io::Write>(WRITE_BLK_SUCCEEDED, progress, _sb_idx, _blk);
		break;
	}
	case WRITE_BLK_SUCCEEDED:

		_generate_req<Block_io::Sync>(_sb_idx ? SB_COMPLETE : WRITE_HASH_TO_TA, progress);
		progress = true;
		break;

	case WRITE_HASH_TO_TA:

		calc_hash(_blk, _hash);
		_generate_req<Trust_anchor::Write_hash>(SB_COMPLETE, progress, _hash);
		break;

	case SB_COMPLETE:

		if (_sb_idx < NR_OF_SUPERBLOCK_SLOTS - 1) {
			_sb_idx++;
			_sb = { };
			_sb.encode_to_blk(_blk);
			_generate_req<Block_io::Write>(WRITE_BLK_SUCCEEDED, progress, _sb_idx, _blk);
		} else
			_mark_req_successful(progress);
		break;

	default: break;
	}
}


Sb_initializer::Sb_initializer()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}


void Sb_initializer::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}
