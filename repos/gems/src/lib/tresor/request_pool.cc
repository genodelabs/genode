/*
 * \brief  Module for scheduling requests for processing
 * \author Martin Stein
 * \date   2023-03-17
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/request_pool.h>
#include <tresor/superblock_control.h>

using namespace Tresor;


char const *Request::op_to_string(Operation op) { return to_string(op); }


void Request_pool::_execute_read(Channel &channel, Index_queue &indices,
                                 Slots_index const idx, bool &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:
		channel._nr_of_blks = 0;

		channel._prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_READ_VBA,
			.pl_idx = idx,
			.blk_nr = channel._request.block_number() + channel._nr_of_blks,
			.idx    = 0
		};

		channel._state = Channel::State::READ_VBA_AT_SB_CTRL_PENDING;
		progress       = true;

		break;
	case Channel::State::READ_VBA_AT_SB_CTRL_COMPLETE:
		if (channel._prim.succ) {

			channel._nr_of_blks += 1;

			if (channel._nr_of_blks < channel._request.count()) {

				channel._prim = {
					.op     = Channel::Generated_prim::Type::READ,
					.succ   = false,
					.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_READ_VBA,
					.pl_idx = idx,
					.blk_nr = channel._request.block_number() + channel._nr_of_blks,
					.idx    = 0
				};

				channel._state = Channel::State::READ_VBA_AT_SB_CTRL_PENDING;
				progress = true;
			} else {
				channel._request.success(true);
				channel._state = Channel::State::COMPLETE;
				indices.dequeue(idx);
				progress = true;
			}
		} else {
			channel._request.success(false);
			channel._state = Channel::State::COMPLETE;
			indices.dequeue(idx);
			progress = true;
		}
		break;
	default:
		break;
	}
}


void Request_pool::_execute_write(Channel &channel, Index_queue &indices,
                                  Slots_index const idx, bool &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		channel._nr_of_blks = 0;

		channel._prim = {
			.op     = Channel::Generated_prim::Type::WRITE,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_WRITE_VBA,
			.pl_idx = idx,
			.blk_nr = channel._request.block_number() + channel._nr_of_blks,
			.idx    = 0
		};
		channel._state = Channel::State::WRITE_VBA_AT_SB_CTRL_PENDING;
		progress = true;
		break;

	case Channel::State::WRITE_VBA_AT_SB_CTRL_COMPLETE:

		if (channel._prim.succ) {
			channel._nr_of_blks += 1;

			if (channel._nr_of_blks < channel._request.count()) {

				channel._prim = {
					.op     = Channel::Generated_prim::Type::WRITE,
					.succ   = false,
					.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_WRITE_VBA,
					.pl_idx = idx,
					.blk_nr = channel._request.block_number() + channel._nr_of_blks,
					.idx    = 0
				};

				channel._state = Channel::State::WRITE_VBA_AT_SB_CTRL_PENDING;
				progress       = true;

			} else {

				channel._request.success(true);
				channel._state = Channel::State::COMPLETE;
				indices.dequeue(idx);
				progress = true;

			}
		} else {

			channel._request.success(false);
			channel._state = Channel::State::COMPLETE;
			indices.dequeue(idx);
			progress = true;

		}

		break;
	default:
		break;
	}
}


void Request_pool::_execute_create_snap(Channel &channel,
                                        Index_queue &indices,
                                        Slots_index const idx,
                                        bool &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		channel._prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_CREATE_SNAP,
			.pl_idx = idx,
			.blk_nr = 0,
			.idx    = 0
		};
		channel._state = Channel::State::CREATE_SNAP_AT_SB_CTRL_PENDING;
		progress = true;
		break;

	case Channel::State::CREATE_SNAP_AT_SB_CTRL_COMPLETE:

		if (channel._prim.succ) {
			channel._request.success(true);
		} else
			channel._request.success(false);

		channel._state = Channel::State::COMPLETE;
		indices.dequeue(idx);
		progress = true;
		break;

	default:

		break;
	}
}


void Request_pool::_execute_discard_snap(Channel &channel,
                                         Index_queue &indices,
                                         Slots_index const idx,
                                         bool &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		channel._prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_DISCARD_SNAP,
			.pl_idx = idx,
			.blk_nr = 0,
			.idx    = 0
		};
		channel._state = Channel::State::DISCARD_SNAP_AT_SB_CTRL_PENDING;
		progress = true;
		break;

	case Channel::State::DISCARD_SNAP_AT_SB_CTRL_COMPLETE:

		if (channel._prim.succ) {
			channel._request.success(true);
		} else
			channel._request.success(false);

		channel._state = Channel::State::COMPLETE;
		indices.dequeue(idx);
		progress = true;
		break;

	default:

		break;
	}
}


void Request_pool::_execute_sync(Channel &channel, Index_queue &indices,
                                 Slots_index const idx, bool &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		channel._prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_SYNC,
			.pl_idx = idx,
			.blk_nr = 0,
			.idx    = 0
		};

		channel._state = Channel::State::SYNC_AT_SB_CTRL_PENDING;
		progress       = true;

		break;
	case Channel::State::SYNC_AT_SB_CTRL_COMPLETE:

		if (channel._prim.succ) {
			channel._request.success(true);
		} else
			channel._request.success(false);

		channel._state = Channel::State::COMPLETE;
		indices.dequeue(idx);
		progress = true;

		break;
	default:
		break;
	}
}


char const *Request_pool::_state_to_step_label(Channel::State state)
{
	switch (state) {
	case Channel::State::TREE_EXTENSION_STEP_COMPLETE: return "tree ext step";
	default: break;
	}
	return "?";
}


void Request_pool::_mark_req_failed(Channel    &chan,
                                    bool       &progress,
                                    char const *str)
{
	error("request_pool: request (", chan._request,
	      ") failed at step \"", str, "\"");

	chan._request.success(false);
	chan._state = Channel::COMPLETE;
	progress = true;
}


bool Request_pool::_handle_failed_generated_req(Channel &chan,
                                                bool    &progress)
{
	if (chan._prim.succ)
		return false;

	_mark_req_failed(chan, progress, _state_to_step_label(chan._state));
	return true;
}


void Request_pool::_mark_req_successful(Channel     &chan,
                                        Slots_index  idx,
                                        bool        &progress)
{
	chan._request.success(true);
	chan._state = Channel::COMPLETE;
	_indices.dequeue(idx);
	progress = true;
}


void Request_pool::_execute_extend_tree(Channel        &chan,
                                        Slots_index     idx,
                                        Channel::State  tree_ext_step_pending,
                                        bool           &progress)
{
	switch (chan._state) {
	case Channel::SUBMITTED:

		chan._prim = {
			.op     = Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::TAG_POOL_SB_CTRL_TREE_EXT_STEP,
			.pl_idx = idx,
			.blk_nr = 0,
			.idx    = 0
		};
		chan._state = tree_ext_step_pending;
		progress = true;
		break;

	case Channel::TREE_EXTENSION_STEP_COMPLETE:

		if (_handle_failed_generated_req(chan, progress))
			break;

		if (chan._request_finished) {

			_mark_req_successful(chan, idx, progress);

		} else {

			chan._nr_of_requests_preponed = 0;
			chan._state = Channel::PREPONE_REQUESTS_PENDING;
			progress = true;
		}
		break;

	case Channel::PREPONE_REQUESTS_PENDING:
	{
		bool requests_preponed { false };
		bool at_req_that_cannot_be_preponed { false };

		while (chan._nr_of_requests_preponed < MAX_NR_OF_REQUESTS_PREPONED_AT_A_TIME &&
		       !at_req_that_cannot_be_preponed &&
		       !_indices.item_is_tail(idx))
		{
			Pool_index const next_idx { _indices.next_item(idx) };
			switch (_channels[next_idx]._request.operation()) {
			case Request::READ:
			case Request::WRITE:
			case Request::SYNC:
			case Request::DISCARD_SNAPSHOT:

				_indices.move_one_item_towards_tail(idx);
				chan._nr_of_requests_preponed++;
				requests_preponed = true;
				progress = true;
				break;

			default:

				at_req_that_cannot_be_preponed = true;
				break;
			}
		}
		if (!requests_preponed) {

			chan._state = Channel::PREPONE_REQUESTS_COMPLETE;
			progress = true;
		}
		break;
	}
	case Channel::PREPONE_REQUESTS_COMPLETE:

		chan._prim = {
			.op     = Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::TAG_POOL_SB_CTRL_TREE_EXT_STEP,
			.pl_idx = idx,
			.blk_nr = 0,
			.idx    = 0
		};
		chan._state = tree_ext_step_pending;
		progress = true;
		break;

	default:

		break;
	}
}


void Request_pool::_execute_rekey(Channel     &chan,
                                  Index_queue &indices,
                                  Slots_index  idx,
                                  bool        &progress)
{
	Request &req { chan._request };

	switch (chan._state) {
	case Channel::State::SUBMITTED:

		chan._prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_INIT_REKEY,
			.pl_idx = idx,
			.blk_nr = 0,
			.idx    = 0
		};
		chan._state = Channel::State::REKEY_INIT_PENDING;
		progress    = true;
		break;

	case Channel::State::SUBMITTED_RESUME_REKEYING:

		chan._prim = { };
		chan._nr_of_requests_preponed = 0;
		chan._state = Channel::State::PREPONE_REQUESTS_PENDING;
		progress = true;
		break;

	case Channel::State::REKEY_INIT_COMPLETE:

		if (!chan._prim.succ) {
			class Exception_1 { };
			throw Exception_1 { };
		}
		chan._nr_of_requests_preponed = 0;
		chan._state = Channel::State::PREPONE_REQUESTS_PENDING;
		progress = true;
		break;

	case Channel::State::REKEY_VBA_COMPLETE:

		if (!chan._prim.succ) {
			class Exception_1 { };
			throw Exception_1 { };
		}
		if (chan._request_finished) {

			req.success(true);
			chan._state = Channel::State::COMPLETE;
			_indices.dequeue(idx);
			progress = true;
			break;

		} else {

			chan._nr_of_requests_preponed = 0;
			chan._state = Channel::State::PREPONE_REQUESTS_PENDING;
			progress = true;
			break;
		}

	case Channel::State::PREPONE_REQUESTS_PENDING:
	{
		bool requests_preponed { false };
		while (true) {

			bool exit_loop { false };

			if (chan._nr_of_requests_preponed >= MAX_NR_OF_REQUESTS_PREPONED_AT_A_TIME ||
			    indices.item_is_tail(idx))
				break;

			Pool_index const next_idx { indices.next_item(idx) };
			switch (_channels[next_idx]._request.operation()) {
			case Request::READ:
			case Request::WRITE:
			case Request::SYNC:
			case Request::DISCARD_SNAPSHOT:

				indices.move_one_item_towards_tail(idx);
				chan._nr_of_requests_preponed++;
				requests_preponed = true;
				progress = true;
				break;

			default:

				exit_loop = true;
				break;
			}
			if (exit_loop)
				break;
		}
		if (!requests_preponed) {
			chan._state = Channel::State::PREPONE_REQUESTS_COMPLETE;
			progress = true;
		}
		break;
	}
	case Channel::State::PREPONE_REQUESTS_COMPLETE:

		chan._prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_REKEY_VBA,
			.pl_idx = idx,
			.blk_nr = 0,
			.idx    = 0
		};
		chan._state = Channel::State::REKEY_VBA_PENDING;
		progress    = true;
		break;

	default:

		break;
	}
}


void Request_pool::_execute_initialize(Channel &channel, Index_queue &indices,
                                       Slots_index const idx, bool &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		channel._prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_INITIALIZE,
			.pl_idx = idx,
			.blk_nr = 0,
			.idx    = 0
		};

		channel._state = Channel::State::INITIALIZE_SB_CTRL_PENDING;
		progress       = true;

		break;

	case Channel::State::INITIALIZE_SB_CTRL_COMPLETE:

		if (not channel._prim.succ) {
			class Initialize_primitive_not_successfull { };
			throw Initialize_primitive_not_successfull { };
		}

		switch (channel._sb_state) {
		case Superblock::INVALID:
			class Initialize_sb_ctrl_invalid { };
			throw Initialize_sb_ctrl_invalid { };

			break;
		case Superblock::NORMAL:

			indices.dequeue(idx);
			channel.invalidate();
			progress = true;

			break;

		case Superblock::REKEYING:

			class Exception_rekeying { };
			throw Exception_rekeying { };
			channel._request = Tresor::Request(Request::Operation::REKEY,
			                                false, 0, 0, 0, 0, 0, 0,
			                                INVALID_MODULE_ID,
			                                INVALID_MODULE_REQUEST_ID);
			indices.enqueue(idx);
			progress = true;

			break;
		case Superblock::EXTENDING_VBD:

			class Exception_ext_vbd { };
			throw Exception_ext_vbd { };
			channel._state = Channel::State::SUBMITTED;

			channel._request = Tresor::Request(Request::Operation::EXTEND_VBD,
			                                false, 0, 0, 0, 0, 0, 0,
			                                INVALID_MODULE_ID,
			                                INVALID_MODULE_REQUEST_ID);

			indices.enqueue(idx);

			progress = true;

			break;
		case Superblock::EXTENDING_FT:

			class Exception_ext_ft { };
			throw Exception_ext_ft { };
			channel._state = Channel::State::SUBMITTED;

			channel._request = Tresor::Request(Request::Operation::EXTEND_FT,
			                                false, 0, 0, 0, 0, 0, 0,
			                                INVALID_MODULE_ID,
			                                INVALID_MODULE_REQUEST_ID);

			indices.enqueue(idx);

			progress = true;

			break;
		}

		break;
	default:
		break;
	}
}


void Request_pool::_execute_deinitialize(Channel &channel, Index_queue &indices,
                                         Slots_index const idx, bool &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		channel._prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_DEINITIALIZE,
			.pl_idx = idx,
			.blk_nr = 0,
			.idx    = 0
		};

		channel._state = Channel::State::DEINITIALIZE_SB_CTRL_PENDING;
		progress       = true;

		break;

	case Channel::State::DEINITIALIZE_SB_CTRL_COMPLETE:

		if (not channel._prim.succ) {
			class Deinitialize_primitive_not_successfull { };
			throw Deinitialize_primitive_not_successfull { };
		}

		channel._request.success(true);
		channel._state = Channel::State::COMPLETE;
		indices.dequeue(idx);
		progress = true;

		break;
	default:
		break;
	}
}


void Request_pool::execute(bool &progress)
{
	if (_indices.empty())
		return;

	class Not_implemented { };

	auto const idx = _indices.head();

	if (idx >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &channel = _channels[idx];
	Request &request = { channel._request };

	switch (request.operation()) {
	case Tresor::Request::Operation::READ:             _execute_read(channel, _indices, idx, progress); break;
	case Tresor::Request::Operation::WRITE:            _execute_write(channel, _indices, idx, progress); break;
	case Tresor::Request::Operation::SYNC:             _execute_sync(channel, _indices, idx, progress); break;
	case Tresor::Request::Operation::REKEY:            _execute_rekey(channel, _indices, idx, progress); break;
	case Tresor::Request::Operation::EXTEND_VBD:       _execute_extend_tree(channel, idx, Channel::VBD_EXTENSION_STEP_PENDING, progress); break;
	case Tresor::Request::Operation::EXTEND_FT:        _execute_extend_tree(channel, idx, Channel::FT_EXTENSION_STEP_PENDING, progress); break;
	case Tresor::Request::Operation::INITIALIZE:       _execute_initialize(channel, _indices, idx, progress); break;
	case Tresor::Request::Operation::DEINITIALIZE:     _execute_deinitialize(channel, _indices, idx, progress); break;
	case Tresor::Request::Operation::CREATE_SNAPSHOT:  _execute_create_snap(channel, _indices, idx, progress); break;
	case Tresor::Request::Operation::DISCARD_SNAPSHOT: _execute_discard_snap(channel, _indices, idx, progress); break;
	default:                                           break;
	}
}


void Request_pool::submit_request(Module_request &mod_req)
{
	for (Module_request_id idx { 0 }; idx < NR_OF_CHANNELS; idx++) {
		if (_channels[idx]._state == Channel::INVALID) {
			Request &req { *static_cast<Request *>(&mod_req) };
			switch (req.operation()) {
			case Request::INITIALIZE:

				class Exception_1 { };
				throw Exception_1 { };

			case Request::SYNC:
			case Request::READ:
			case Request::WRITE:
			case Request::DEINITIALIZE:
			case Request::REKEY:
			case Request::EXTEND_VBD:
			case Request::EXTEND_FT:
			case Request::CREATE_SNAPSHOT:
			case Request::DISCARD_SNAPSHOT:

				mod_req.dst_request_id(idx);
				_channels[idx]._state = Channel::SUBMITTED;
				_channels[idx]._request = req;
				_indices.enqueue((Slots_index)idx);
				return;

			default:

				class Exception_2 { };
				throw Exception_2 { };
			}
		}
	}
	class Exception_3 { };
	throw Exception_3 { };
}


bool Request_pool::_peek_generated_request(uint8_t *buf_ptr,
                                           size_t   buf_size)
{
	if (_indices.empty())
		return false;

	Slots_index const idx { _indices.head() };
	Channel &chan { _channels[idx] };
	Superblock_control_request::Type scr_type;

	switch (chan._state) {
	case Channel::READ_VBA_AT_SB_CTRL_PENDING: scr_type = Superblock_control_request::READ_VBA; break;
	case Channel::WRITE_VBA_AT_SB_CTRL_PENDING: scr_type = Superblock_control_request::WRITE_VBA; break;
	case Channel::SYNC_AT_SB_CTRL_PENDING: scr_type = Superblock_control_request::SYNC; break;
	case Channel::CREATE_SNAP_AT_SB_CTRL_PENDING: scr_type = Superblock_control_request::CREATE_SNAPSHOT; break;
	case Channel::DISCARD_SNAP_AT_SB_CTRL_PENDING: scr_type = Superblock_control_request::DISCARD_SNAPSHOT; break;
	case Channel::INITIALIZE_SB_CTRL_PENDING: scr_type = Superblock_control_request::INITIALIZE; break;
	case Channel::DEINITIALIZE_SB_CTRL_PENDING: scr_type = Superblock_control_request::DEINITIALIZE; break;
	case Channel::REKEY_INIT_PENDING: scr_type = Superblock_control_request::INITIALIZE_REKEYING; break;
	case Channel::REKEY_VBA_PENDING: scr_type = Superblock_control_request::REKEY_VBA; break;
	case Channel::VBD_EXTENSION_STEP_PENDING: scr_type = Superblock_control_request::VBD_EXTENSION_STEP; break;
	case Channel::FT_EXTENSION_STEP_PENDING: scr_type = Superblock_control_request::FT_EXTENSION_STEP; break;
	default: return false;
	}
	Superblock_control_request::create(
		buf_ptr, buf_size, REQUEST_POOL, idx, scr_type,
		chan._request.offset(), chan._request.tag(),
		chan._request.count(), chan._prim.blk_nr, chan._request._gen);

	return true;
}


void Request_pool::_drop_generated_request(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	switch (chan._state) {
	case Channel::READ_VBA_AT_SB_CTRL_PENDING: chan._state = Channel::READ_VBA_AT_SB_CTRL_IN_PROGRESS; break;
	case Channel::WRITE_VBA_AT_SB_CTRL_PENDING: chan._state = Channel::WRITE_VBA_AT_SB_CTRL_IN_PROGRESS; break;
	case Channel::SYNC_AT_SB_CTRL_PENDING: chan._state = Channel::SYNC_AT_SB_CTRL_IN_PROGRESS; break;
	case Channel::REKEY_INIT_PENDING: chan._state = Channel::REKEY_INIT_IN_PROGRESS; break;
	case Channel::REKEY_VBA_PENDING: chan._state = Channel::REKEY_VBA_IN_PROGRESS; break;
	case Channel::VBD_EXTENSION_STEP_PENDING: chan._state = Channel::TREE_EXTENSION_STEP_IN_PROGRESS; break;
	case Channel::FT_EXTENSION_STEP_PENDING: chan._state = Channel::TREE_EXTENSION_STEP_IN_PROGRESS; break;
	case Channel::CREATE_SNAP_AT_SB_CTRL_PENDING: chan._state = Channel::CREATE_SNAP_AT_SB_CTRL_IN_PROGRESS; break;
	case Channel::DISCARD_SNAP_AT_SB_CTRL_PENDING: chan._state = Channel::DISCARD_SNAP_AT_SB_CTRL_IN_PROGRESS; break;
	case Channel::INITIALIZE_SB_CTRL_PENDING: chan._state = Channel::INITIALIZE_SB_CTRL_IN_PROGRESS; break;
	case Channel::DEINITIALIZE_SB_CTRL_PENDING: chan._state = Channel::DEINITIALIZE_SB_CTRL_IN_PROGRESS; break;
	default:
		class Exception_2 { };
		throw Exception_2 { };
	}
}


void Request_pool::generated_request_complete(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	switch (mod_req.dst_module_id()) {
	case SUPERBLOCK_CONTROL:
	{
		Superblock_control_request &gen_req { *static_cast<Superblock_control_request *>(&mod_req) };
		chan._prim.succ = gen_req.success();
		switch (chan._state) {
		case Channel::READ_VBA_AT_SB_CTRL_IN_PROGRESS: chan._state = Channel::READ_VBA_AT_SB_CTRL_COMPLETE; break;
		case Channel::WRITE_VBA_AT_SB_CTRL_IN_PROGRESS: chan._state = Channel::WRITE_VBA_AT_SB_CTRL_COMPLETE; break;
		case Channel::SYNC_AT_SB_CTRL_IN_PROGRESS: chan._state = Channel::SYNC_AT_SB_CTRL_COMPLETE; break;
		case Channel::TREE_EXTENSION_STEP_IN_PROGRESS:
			chan._state = Channel::TREE_EXTENSION_STEP_COMPLETE;
			chan._request_finished = gen_req.request_finished();
			break;
		case Channel::CREATE_SNAP_AT_SB_CTRL_IN_PROGRESS:
			chan._state = Channel::CREATE_SNAP_AT_SB_CTRL_COMPLETE;
			break;
		case Channel::DISCARD_SNAP_AT_SB_CTRL_IN_PROGRESS:
			chan._state = Channel::DISCARD_SNAP_AT_SB_CTRL_COMPLETE;
			break;
		case Channel::INITIALIZE_SB_CTRL_IN_PROGRESS:
			chan._sb_state = gen_req.sb_state();
			chan._state = Channel::INITIALIZE_SB_CTRL_COMPLETE;
			break;
		case Channel::DEINITIALIZE_SB_CTRL_IN_PROGRESS: chan._state = Channel::DEINITIALIZE_SB_CTRL_COMPLETE; break;
		case Channel::REKEY_INIT_IN_PROGRESS: chan._state = Channel::REKEY_INIT_COMPLETE; break;
		case Channel::REKEY_VBA_IN_PROGRESS:
			chan._request_finished = gen_req.request_finished();
			chan._state = Channel::REKEY_VBA_COMPLETE;
			break;
		default:
			class Exception_2 { };
			throw Exception_2 { };
		}
		break;
	}
	default:
		class Exception_5 { };
		throw Exception_5 { };
	}
}


Request_pool::Request_pool()
{
	Slots_index const idx { 0 };
	_channels[idx]._state = Channel::SUBMITTED;
	_channels[idx]._request = Request {
		Request::INITIALIZE, false, 0, 0, 0, 0, 0, 0,
		INVALID_MODULE_ID, INVALID_MODULE_REQUEST_ID };

	_indices.enqueue(idx);
}


bool Request_pool::_peek_completed_request(uint8_t *buf_ptr,
                                           size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._request.operation() != Request::INVALID &&
		    channel._state == Channel::COMPLETE) {

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


void Request_pool::_drop_completed_request(Module_request &req)
{
	Module_request_id id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	if (chan._request.operation() == Request::INVALID ||
	    chan._state != Channel::COMPLETE) {

		class Exception_2 { };
		throw Exception_2 { };
	}
	chan = Channel { };
}
