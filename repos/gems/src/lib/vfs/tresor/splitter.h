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

#ifndef _TRESOR__IO_SPLITTER_H_
#define _TRESOR__IO_SPLITTER_H_

/* tresor includes */
#include <tresor/request_pool.h>

namespace Tresor {

	struct Lookup_buffer : Genode::Interface
	{
		virtual Block const &src_for_writing_vba(Request_tag, Virtual_block_address) = 0;
		virtual Block &dst_for_reading_vba(Request_tag, Virtual_block_address) = 0;
	};

	class Splitter_request;
	class Splitter_channel;
	class Splitter;
}


class Tresor::Splitter_request : public Tresor::Module_request
{
	friend class Splitter_channel;

	public:

		enum Operation { READ, WRITE };

	private:

		Operation const _op;
		Request_offset const _off;
		Key_id const _key_id;
		Generation const _gen;
		Byte_range_ptr const _buf;
		bool &_success;

		NONCOPYABLE(Splitter_request);

	public:

		Splitter_request(Module_id, Module_channel_id, Operation, bool &, Request_offset, Byte_range_ptr const &, Key_id, Generation);

		static char const *op_to_string(Operation);

		void print(Genode::Output &out) const override { Genode::print(out, op_to_string(_op), " off ", _off, " size ", _buf.num_bytes); }
};


class Tresor::Splitter_channel : public Tresor::Module_channel
{
	private:

		using Request = Splitter_request;


		enum State : State_uint {
			PROTRUDING_FIRST_BLK_WRITTEN, PROTRUDING_LAST_BLK_WRITTEN, PROTRUDING_FIRST_BLK_READ, PROTRUDING_LAST_BLK_READ, INSIDE_BLKS_ACCESSED,
			REQ_SUBMITTED, REQ_GENERATED, REQ_COMPLETE };

		State _state { };
		Request *_req_ptr { };
		addr_t _curr_off { };
		addr_t _curr_buf_addr { };
		Block _blk  { };
		Generation _gen { };
		bool _generated_req_success { };

		NONCOPYABLE(Splitter_channel);

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		Virtual_block_address _curr_vba() const { return (Virtual_block_address)(_curr_off / BLOCK_SIZE); }

		addr_t _curr_buf_off() const
		{
			ASSERT(_curr_off >= _req_ptr->_off && _curr_off <= _req_ptr->_off + _req_ptr->_buf.num_bytes);
			return _curr_off - _req_ptr->_off;
		}

		addr_t _num_remaining_bytes() const
		{
			ASSERT(_curr_off >= _req_ptr->_off && _curr_off <= _req_ptr->_off + _req_ptr->_buf.num_bytes);
			return _req_ptr->_off + _req_ptr->_buf.num_bytes - _curr_off;
		}

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

		void _mark_req_successful(bool &);

		void _advance_curr_off(addr_t, Tresor::Request::Operation, bool &);

		void _read(bool &progress);

		void _write(bool &progress);

		Block &_blk_buf_for_vba(Virtual_block_address);

	public:

		Splitter_channel(Module_channel_id id) : Module_channel { SPLITTER, id } { }

		void execute(bool &progress);

		Block const &src_for_writing_vba(Virtual_block_address vba) { return _blk_buf_for_vba(vba); }

		Block &dst_for_reading_vba(Virtual_block_address vba) { return _blk_buf_for_vba(vba); }
};


class Tresor::Splitter : public Tresor::Module, public Tresor::Lookup_buffer
{
	private:

		using Channel = Splitter_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Splitter);

	public:

		Splitter();

		void execute(bool &) override;

		Block const &src_for_writing_vba(Request_tag, Virtual_block_address) override;

		Block &dst_for_reading_vba(Request_tag, Virtual_block_address) override;
};


#endif /* _TRESOR__IO_SPLITTER_H_ */
