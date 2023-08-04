/*
 * \brief  Module for splitting unaligned/uneven I/O requests
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2023-09-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* vfs tresor includes */
#include <splitter.h>

using namespace Tresor;

Splitter_request::Splitter_request(Module_id src_mod, Module_channel_id src_chan, Operation op, bool &success,
                                   Request_offset off, Byte_range_ptr const &buf, Key_id key_id, Generation gen)
:
	Module_request { src_mod, src_chan, SPLITTER }, _op { op }, _off { off }, _key_id { key_id }, _gen { gen },
	_buf { buf.start, buf.num_bytes }, _success { success }
{ }


char const *Splitter_request::op_to_string(Operation op)
{
	switch (op) {
	case Operation::READ: return "read";
	case Operation::WRITE: return "write";
	}
	ASSERT_NEVER_REACHED;
}


void Splitter_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("splitter: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Splitter_channel::_mark_req_successful(bool &progress)
{
	Request &req { *_req_ptr };
	req._success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Splitter_channel::_request_submitted(Module_request &req)
{
	_req_ptr = static_cast<Request*>(&req);
	_state = REQ_SUBMITTED;
}


void Splitter_channel::_advance_curr_off(addr_t advance, Tresor::Request::Operation op, bool &progress)
{
	Splitter_request &req { *_req_ptr };
	_curr_off += advance;
	if (!_num_remaining_bytes()) {
		_mark_req_successful(progress);
	} else if (_curr_off % BLOCK_SIZE) {
		_curr_buf_addr = (addr_t)&_blk;
		_generate_req<Tresor::Request>(
			PROTRUDING_FIRST_BLK_READ, progress, Tresor::Request::READ, _curr_vba(), 0, 1, req._key_id, id(), _gen);
	} else if (_num_remaining_bytes() < BLOCK_SIZE) {
		_curr_buf_addr = (addr_t)&_blk;
		_generate_req<Tresor::Request>(
			PROTRUDING_LAST_BLK_READ, progress, Tresor::Request::READ, _curr_vba(), 0, 1, req._key_id, id(), _gen);
	} else {
		_curr_buf_addr = (addr_t)req._buf.start + _curr_buf_off();
		_generate_req<Tresor::Request>(
			INSIDE_BLKS_ACCESSED, progress, op, _curr_vba(), 0, _num_remaining_bytes() / BLOCK_SIZE, req._key_id, id(), _gen);
	}
}


void Splitter_channel::_write(bool &progress)
{
	Splitter_request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		_curr_off = 0;
		_gen = req._gen;
		_advance_curr_off(req._off, Tresor::Request::WRITE, progress);
		break;

	case PROTRUDING_FIRST_BLK_READ:
	{
		size_t num_outside_bytes { _curr_off % BLOCK_SIZE };
		size_t num_inside_bytes { min(_num_remaining_bytes(), BLOCK_SIZE - num_outside_bytes) };
		memcpy((void *)((addr_t)&_blk + num_outside_bytes), req._buf.start, num_inside_bytes);
		_curr_buf_addr = (addr_t)&_blk;
		_generate_req<Tresor::Request>(
			PROTRUDING_FIRST_BLK_WRITTEN, progress, Tresor::Request::WRITE, _curr_vba(), 0, 1, req._key_id, id(), _gen);
		break;
	}
	case PROTRUDING_FIRST_BLK_WRITTEN:
	{
		size_t num_outside_bytes { _curr_off % BLOCK_SIZE };
		size_t num_inside_bytes { min(_num_remaining_bytes(), BLOCK_SIZE - num_outside_bytes) };
		_advance_curr_off(num_inside_bytes, Tresor::Request::WRITE, progress);
		break;
	}
	case INSIDE_BLKS_ACCESSED:

		_advance_curr_off((_num_remaining_bytes() / BLOCK_SIZE) * BLOCK_SIZE, Tresor::Request::WRITE, progress);
		break;

	case PROTRUDING_LAST_BLK_READ:

		memcpy(&_blk, (void *)((addr_t)req._buf.start + _curr_buf_off()), _num_remaining_bytes());
		_curr_buf_addr = (addr_t)&_blk;
		_generate_req<Tresor::Request>(
			PROTRUDING_LAST_BLK_WRITTEN, progress, Tresor::Request::WRITE, _curr_vba(), 0, 1, req._key_id, id(), _gen);
		break;

	case PROTRUDING_LAST_BLK_WRITTEN: _advance_curr_off(_num_remaining_bytes(), Tresor::Request::WRITE, progress); break;
	default: break;
	}
}


void Splitter_channel::_read(bool &progress)
{
	Splitter_request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		_curr_off = 0;
		_gen = req._gen;
		_advance_curr_off(req._off, Tresor::Request::READ, progress);
		break;

	case PROTRUDING_FIRST_BLK_READ:
	{
		size_t num_outside_bytes { _curr_off % BLOCK_SIZE };
		size_t num_inside_bytes { min(_num_remaining_bytes(), BLOCK_SIZE - num_outside_bytes) };
		memcpy(req._buf.start, (void *)((addr_t)&_blk + num_outside_bytes), num_inside_bytes);
		_advance_curr_off(num_inside_bytes, Tresor::Request::READ, progress);
		break;
	}
	case INSIDE_BLKS_ACCESSED:

		_advance_curr_off((_num_remaining_bytes() / BLOCK_SIZE) * BLOCK_SIZE, Tresor::Request::READ, progress);
		break;

	case PROTRUDING_LAST_BLK_READ:

		memcpy((void *)((addr_t)req._buf.start + _curr_buf_off()), &_blk, _num_remaining_bytes());
		_advance_curr_off(_num_remaining_bytes(), Tresor::Request::READ, progress);
		break;

	default: break;
	}
}


void Splitter_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	switch (_req_ptr->_op) {
	case Request::READ: _read(progress); break;
	case Request::WRITE: _write(progress); break;
	}
}


Block &Splitter_channel::_blk_buf_for_vba(Virtual_block_address vba)
{
	ASSERT(_state == REQ_GENERATED);
	return *(Block *)(_curr_buf_addr + (vba - _curr_vba()) * BLOCK_SIZE);
}


Block const &Splitter::src_for_writing_vba(Request_tag tag, Virtual_block_address vba)
{
	Block const *blk_ptr { };
	with_channel<Splitter_channel>(tag, [&] (Splitter_channel &chan) {
		blk_ptr = &chan.src_for_writing_vba(vba); });
	ASSERT(blk_ptr);
	return *blk_ptr;
}


Block &Splitter::dst_for_reading_vba(Request_tag tag, Virtual_block_address vba)
{
	Block *blk_ptr { };
	with_channel<Splitter_channel>(tag, [&] (Splitter_channel &chan) {
		blk_ptr = &chan.dst_for_reading_vba(vba); });
	ASSERT(blk_ptr);
	return *blk_ptr;
}


Splitter::Splitter()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}


void Splitter::execute(bool &progress)
{
	for_each_channel<Splitter_channel>([&] (Splitter_channel &chan) {
		chan.execute(progress); });
}
