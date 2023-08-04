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

#ifndef _TRESOR__REQUEST_POOL_H_
#define _TRESOR__REQUEST_POOL_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/superblock_control.h>

namespace Tresor {

	class Request;
	class Request_pool;
	class Request_pool_channel;
	class Request_pool_channel_queue;
}

class Tresor::Request : public Module_request
{
	NONCOPYABLE(Request);

	friend class Request_pool_channel;

	public:

		enum Operation {
			READ, WRITE, SYNC, CREATE_SNAPSHOT, DISCARD_SNAPSHOT, REKEY, EXTEND_VBD,
			EXTEND_FT, RESUME_REKEYING, DEINITIALIZE, INITIALIZE, };

	private:

		Operation _op;
		Virtual_block_address const _vba;
		Request_offset const _offset;
		Number_of_blocks const _count;
		Key_id const _key_id;
		Request_tag const _tag;
		Generation &_gen;
		bool &_success;

	public:

		static char const *op_to_string(Operation);

		Request(Module_id, Module_channel_id, Operation, Virtual_block_address, Request_offset,
		        Number_of_blocks, Key_id, Request_tag, Generation &, bool &);

		void print(Output &) const override;
};

class Tresor::Request_pool_channel : public Module_channel
{
	private:

		enum State : State_uint {
			INVALID, REQ_SUBMITTED, REQ_RESUMED, REQ_GENERATED, REKEY_INIT_SUCCEEDED, PREPONED_REQUESTS_COMPLETE,
			TREE_EXTENSION_STEP_SUCCEEDED, FORWARD_TO_SB_CTRL_SUCCEEDED, ACCESS_VBA_AT_SB_CTRL_SUCCEEDED,
			REKEY_VBA_SUCCEEDED, INITIALIZE_SB_CTRL_SUCCEEDED, DEINITIALIZE_SB_CTRL_SUCCEEDED, REQ_COMPLETE };

		State _state { INVALID };
		Number_of_blocks _num_blks { 0 };
		Superblock::State _sb_state { Superblock::INVALID };
		uint32_t _num_requests_preponed { 0 };
		bool _request_finished { false };
		bool _generated_req_success { false };
		Request_pool_channel_queue &_chan_queue;
		Request *_req_ptr { nullptr };

		NONCOPYABLE(Request_pool_channel);

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &req) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _access_vbas(bool &, Superblock_control_request::Type);

		void _forward_to_sb_ctrl(bool &, Superblock_control_request::Type);

		void _gen_sb_control_req(bool &, Superblock_control_request::Type, State, Virtual_block_address);

		void _rekey(bool &);

		void _mark_req_successful(bool &);

		void _reset();

		void _try_prepone_requests(bool &);

		void _extend_tree(Superblock_control_request::Type, bool &);

		void _initialize(bool &);

		void _resume_request(bool &, Request::Operation);

	public:

		Request_pool_channel(Module_channel_id id, Request_pool_channel_queue &chan_queue) : Module_channel { REQUEST_POOL, id }, _chan_queue { chan_queue } { }

		void execute(bool &);
};


class Tresor::Request_pool_channel_queue
{
	NONCOPYABLE(Request_pool_channel_queue);

	public:

		enum { NUM_SLOTS = 16 };

	private:

		using Channel = Request_pool_channel;
		using Slot_index = uint64_t;
		using Number_of_slots = uint64_t;

		Slot_index _head { 0 };
		Slot_index _tail { 0 };
		Number_of_slots _num_used_slots { 0 };
		Channel *_slots[NUM_SLOTS] { 0 };

	public:

		Request_pool_channel_queue() { }

		bool empty() const { return _num_used_slots == 0; }

		bool full() const { return _num_used_slots >= NUM_SLOTS; }

		Channel &head() const;

		void enqueue(Channel &);

		void move_one_slot_towards_tail(Channel const &);

		bool is_tail(Channel const &) const;

		Channel &next(Channel const &) const;

		void dequeue(Channel const &);
};


class Tresor::Request_pool : public Module
{
	NONCOPYABLE(Request_pool);

	private:

		using Channel = Request_pool_channel;

		enum { NUM_CHANNELS = Request_pool_channel_queue::NUM_SLOTS };

		bool _init_success { false };
		Generation _init_gen { INVALID_GENERATION };
		Request _init_req { INVALID_MODULE_ID, INVALID_MODULE_CHANNEL_ID, Request::INITIALIZE, 0, 0, 0, 0, 0, _init_gen, _init_success };
		Constructible<Channel> _channels[NUM_CHANNELS] { };
		Request_pool_channel_queue _chan_queue { };

	public:

		void execute(bool &) override;

		Request_pool();
};

#endif /* _TRESOR__REQUEST_POOL_H_ */
