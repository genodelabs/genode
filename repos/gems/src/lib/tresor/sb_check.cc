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

/* base includes */
#include <base/log.h>

/* tresor includes */
#include <tresor/sb_check.h>
#include <tresor/vbd_check.h>
#include <tresor/ft_check.h>
#include <tresor/block_io.h>

using namespace Tresor;


/**********************
 ** Sb_check_request **
 **********************/

Sb_check_request::Sb_check_request(Module_id         src_module_id,
                                   Module_request_id src_request_id)
:
	Module_request { src_module_id, src_request_id, SB_CHECK }
{ }


void Sb_check_request::create(void     *buf_ptr,
                              size_t    buf_size,
                              uint64_t  src_module_id,
                              uint64_t  src_request_id,
                              size_t    req_type)
{
	Sb_check_request req { src_module_id, src_request_id };
	req._type = (Type)req_type;

	if (sizeof(req) > buf_size) {
		class Bad_size_0 { };
		throw Bad_size_0 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


char const *Sb_check_request::type_to_string(Type type)
{
	switch (type) {
	case INVALID: return "invalid";
	case CHECK:   return "check";
	}
	return "?";
}


/**************
 ** Sb_check **
 **************/

char const *Sb_check::_state_to_step_label(Channel::Sb_slot_state state)
{
	switch (state) {
	case Channel::READ_DONE: return "read";
	case Channel::VBD_CHECK_DONE: return "vbd check";
	case Channel::FT_CHECK_DONE: return "ft check";
	case Channel::MT_CHECK_DONE: return "mt check";
	default: break;
	}
	return "?";
}


bool Sb_check::_handle_failed_generated_req(Channel &chan,
                                            bool    &progress)
{
	if (chan._gen_prim_success)
		return false;

	_mark_req_failed(
		chan, progress, _state_to_step_label(chan._sb_slot_state));

	return true;
}

void Sb_check::_execute_check(Channel &chan,
                              bool    &progress)
{
	switch (chan._state) {
	case Channel::INSPECT_SBS:

		switch (chan._sb_slot_state) {
		case Channel::INIT:

			chan._sb_slot_state = Channel::READ_STARTED;
			chan._gen_prim_blk_nr = chan._sb_slot_idx;
			progress = true;
			break;

		case Channel::READ_DONE:
		{
			if (_handle_failed_generated_req(chan, progress))
				break;

			Snapshot &snap {
				chan._sb_slot.snapshots.items[chan._sb_slot.curr_snap] };

			if (chan._sb_slot.valid() &&
			    snap.gen > chan._highest_gen) {

				chan._highest_gen = snap.gen;
				chan._last_sb_slot_idx = chan._sb_slot_idx;
			}
			if (chan._sb_slot_idx < MAX_SUPERBLOCK_INDEX) {

				chan._sb_slot_idx++;
				chan._sb_slot_state = Channel::INIT;
				progress = true;

			} else {

				chan._state = Channel::CHECK_SB;
				chan._sb_slot_idx = chan._last_sb_slot_idx;
				chan._sb_slot_state = Channel::INIT;
				progress = true;

				if (VERBOSE_CHECK)
					log("check superblock ", chan._sb_slot_idx);
			}
			break;
		}
		default:

			break;
		}
		break;

	case Channel::CHECK_SB:

		switch (chan._sb_slot_state) {
		case Channel::INIT:

			chan._sb_slot_state = Channel::READ_STARTED;
			chan._gen_prim_blk_nr = chan._sb_slot_idx;
			progress = true;

			if (VERBOSE_CHECK)
				log("  read superblock");

			break;

		case Channel::READ_DONE:

			if (_handle_failed_generated_req(chan, progress))
				break;

			if (chan._sb_slot.valid()) {

				Snapshot &snap {
					chan._sb_slot.snapshots.items[chan._snap_idx] };

				if (snap.valid) {

					chan._sb_slot_state = Channel::VBD_CHECK_STARTED;
					chan._gen_prim_blk_nr = snap.pba;
					progress = true;

					if (VERBOSE_CHECK)
						log("  check snap ", chan._snap_idx, " (", snap, ")");

				} else {

					chan._sb_slot_state = Channel::VBD_CHECK_DONE;
					progress = true;

					if (VERBOSE_CHECK)
						log("  skip snap ", chan._snap_idx,
						    " as it is unused");
				}
			} else {

				chan._sb_slot_state = Channel::DONE;
				progress = true;

				if (VERBOSE_CHECK)
					log("  skip superblock as it is unused");
			}
			break;

		case Channel::VBD_CHECK_DONE:

			if (_handle_failed_generated_req(chan, progress))
				break;

			if (chan._snap_idx < MAX_SNAP_IDX) {

				chan._snap_idx++;
				chan._sb_slot_state = Channel::READ_DONE;
				progress = true;

			} else {

				chan._snap_idx = 0;
				chan._gen_prim_blk_nr = chan._sb_slot.free_number;
				chan._sb_slot_state = Channel::FT_CHECK_STARTED;
				progress = true;

				if (VERBOSE_CHECK)
					log("  check free tree");
			}
			break;

		case Channel::FT_CHECK_DONE:

			if (_handle_failed_generated_req(chan, progress))
				break;

			chan._sb_slot_state = Channel::MT_CHECK_STARTED;
			progress = true;

			if (VERBOSE_CHECK)
				log("  check meta tree");

			break;

		case Channel::MT_CHECK_DONE:

			if (_handle_failed_generated_req(chan, progress))
				break;

			_mark_req_successful(chan, progress);
			break;

		case Channel::DONE:

			break;

		default:

			break;
		}
		break;

	default:

		break;
	}
}


void Sb_check::_mark_req_failed(Channel    &chan,
                                bool       &progress,
                                char const *str)
{
	error("sb check: request (", chan._request, ") failed at step \"", str, "\"");
	chan._request._success = false;
	chan._sb_slot_state = Channel::DONE;
	progress = true;
}


void Sb_check::_mark_req_successful(Channel &chan,
                                    bool    &progress)
{
	Request &req { chan._request };
	req._success = true;
	chan._sb_slot_state = Channel::DONE;
	progress = true;
}


bool Sb_check::_peek_completed_request(uint8_t *buf_ptr,
                                       size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._sb_slot_state == Channel::DONE) {
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


void Sb_check::_drop_completed_request(Module_request &req)
{
	Module_request_id id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	if (_channels[id]._sb_slot_state != Channel::DONE) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	_channels[id]._sb_slot_state = Channel::INACTIVE;
}


bool Sb_check::_peek_generated_request(uint8_t *buf_ptr,
                                       size_t   buf_size)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {

		Channel &chan { _channels[id] };

		if (chan._sb_slot_state == Channel::INACTIVE)
			continue;

		switch (chan._sb_slot_state) {
		case Channel::READ_STARTED:

			construct_in_buf<Block_io_request>(
				buf_ptr, buf_size, SB_CHECK, id,
				Block_io_request::READ, 0, 0, 0,
				chan._gen_prim_blk_nr, 0, 1, &chan._encoded_blk,
				nullptr);

			return true;

		case Channel::VBD_CHECK_STARTED:
		{
			Snapshot const &snap {
				chan._sb_slot.snapshots.items[chan._snap_idx] };

			construct_in_buf<Vbd_check_request>(
				buf_ptr, buf_size, SB_CHECK, id,
				Vbd_check_request::CHECK, snap.max_level,
				chan._sb_slot.degree - 1,
				snap.nr_of_leaves,
				Type_1_node { snap.pba, snap.gen, snap.hash });

			return true;
		}
		case Channel::FT_CHECK_STARTED:

			construct_in_buf<Ft_check_request>(
				buf_ptr, buf_size, SB_CHECK, id,
				Ft_check_request::CHECK,
				(Tree_level_index)chan._sb_slot.free_max_level,
				(Tree_degree)chan._sb_slot.free_degree - 1,
				(Number_of_leaves)chan._sb_slot.free_leaves,
				Type_1_node {
					chan._sb_slot.free_number,
					chan._sb_slot.free_gen,
					chan._sb_slot.free_hash });

			return true;

		case Channel::MT_CHECK_STARTED:

			construct_in_buf<Ft_check_request>(
				buf_ptr, buf_size, SB_CHECK, id,
				Ft_check_request::CHECK,
				(Tree_level_index)chan._sb_slot.meta_max_level,
				(Tree_degree)chan._sb_slot.meta_degree - 1,
				(Number_of_leaves)chan._sb_slot.meta_leaves,
				Type_1_node {
					chan._sb_slot.meta_number,
					chan._sb_slot.meta_gen,
					chan._sb_slot.meta_hash });

			return true;

		default:
			break;
		}
	}
	return false;
}


void Sb_check::_drop_generated_request(Module_request &req)
{
	Module_request_id const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_0 { };
		throw Exception_0 { };
	}
	Channel &chan { _channels[id] };
	switch (chan._sb_slot_state) {
	case Channel::READ_STARTED: chan._sb_slot_state = Channel::READ_DROPPED; break;
	case Channel::VBD_CHECK_STARTED: chan._sb_slot_state = Channel::VBD_CHECK_DROPPED; break;
	case Channel::FT_CHECK_STARTED: chan._sb_slot_state = Channel::FT_CHECK_DROPPED; break;
	case Channel::MT_CHECK_STARTED: chan._sb_slot_state = Channel::MT_CHECK_DROPPED; break;
	default:
		class Exception_4 { };
		throw Exception_4 { };
	}
}


void Sb_check::generated_request_complete(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	switch (mod_req.dst_module_id()) {
	case BLOCK_IO:
	{
		Block_io_request &gen_req { *static_cast<Block_io_request*>(&mod_req) };
		chan._gen_prim_success = gen_req.success();
		switch (chan._sb_slot_state) {
		case Channel::READ_DROPPED:
			chan._sb_slot.decode_from_blk(chan._encoded_blk);
			chan._sb_slot_state = Channel::READ_DONE;
			break;
		default:
			class Exception_2 { };
			throw Exception_2 { };
		}
		break;
	}
	case VBD_CHECK:
	{
		Vbd_check_request &gen_req { *static_cast<Vbd_check_request*>(&mod_req) };
		chan._gen_prim_success = gen_req.success();
		switch (chan._sb_slot_state) {
		case Channel::VBD_CHECK_DROPPED: chan._sb_slot_state = Channel::VBD_CHECK_DONE; break;
		default:
			class Exception_3 { };
			throw Exception_3 { };
		}
		break;
	}
	case FT_CHECK:
	{
		Ft_check_request &gen_req { *static_cast<Ft_check_request*>(&mod_req) };
		chan._gen_prim_success = gen_req.success();
		switch (chan._sb_slot_state) {
		case Channel::FT_CHECK_DROPPED: chan._sb_slot_state = Channel::FT_CHECK_DONE; break;
		case Channel::MT_CHECK_DROPPED: chan._sb_slot_state = Channel::MT_CHECK_DONE; break;
		default:
			class Exception_3 { };
			throw Exception_3 { };
		}
		break;
	}
	default:
		class Exception_8 { };
		throw Exception_8 { };
	}
}


bool Sb_check::ready_to_submit_request()
{
	for (Channel &chan : _channels) {
		if (chan._sb_slot_state == Channel::INACTIVE)
			return true;
	}
	return false;
}


void Sb_check::submit_request(Module_request &req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		Channel &chan { _channels[id] };
		if (chan._sb_slot_state == Channel::INACTIVE) {
			req.dst_request_id(id);
			chan = Channel { };
			chan._request = *static_cast<Request *>(&req);
			chan._sb_slot_state = Channel::INIT;
			return;
		}
	}
	class Exception_1 { };
	throw Exception_1 { };
}


void Sb_check::execute(bool &progress)
{
	for (Channel &chan : _channels) {

		if (chan._sb_slot_state == Channel::INACTIVE)
			continue;

		Request &req { chan._request };
		switch (req._type) {
		case Request::CHECK:

			_execute_check(chan, progress);
			break;

		default:

			class Exception_1 { };
			throw Exception_1 { };
		}
	}
}
