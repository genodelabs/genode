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
#include <tresor/module.h>
#include <tresor/types.h>
#include <tresor/vfs_utilities.h>

namespace Tresor {

	class Request_pool;
	class Request_pool_request;
	class Request_pool_channel;


	class Request : public Module_request
	{
		friend class Request_pool;

		public:

			enum Operation : uint32_t {
				INVALID = 0,
				READ = 1,
				WRITE = 2,
				SYNC = 3,
				CREATE_SNAPSHOT = 4,
				DISCARD_SNAPSHOT = 5,
				REKEY = 6,
				EXTEND_VBD = 7,
				EXTEND_FT = 8,
				RESUME_REKEYING = 10,
				DEINITIALIZE = 11,
				INITIALIZE = 12,
			};

		private:

			Operation        _operation;
			bool             _success;
			uint64_t         _block_number;
			uint64_t         _offset;
			Number_of_blocks _count;
			uint32_t         _key_id;
			uint32_t         _tag;
			Generation       _gen;

		public:

			Request(Operation        operation,
			        bool             success,
			        uint64_t         block_number,
			        uint64_t         offset,
			        Number_of_blocks count,
			        uint32_t         key_id,
			        uint32_t         tag,
			        Generation       gen,
			        Module_id            src_module_id,
			        Module_request_id    src_request_id)
			:
				Module_request { src_module_id, src_request_id, REQUEST_POOL },
				_operation     { operation    },
				_success       { success      },
				_block_number  { block_number },
				_offset        { offset       },
				_count         { count        },
				_key_id        { key_id       },
				_tag           { tag          },
				_gen           { gen          }
			{ }

			Request()
			:
				Module_request { },
				_operation     { Operation::INVALID },
				_success       { false },
				_block_number  { 0 },
				_offset        { 0 },
				_count         { 0 },
				_key_id        { 0 },
				_tag           { 0 },
				_gen           { 0 }
			{ }

			bool valid() const
			{
				return _operation != Operation::INVALID;
			}


			/***************
			 ** Accessors **
			 ***************/

			bool read()             const { return _operation == Operation::READ; }
			bool write()            const { return _operation == Operation::WRITE; }
			bool sync()             const { return _operation == Operation::SYNC; }
			bool create_snapshot()  const { return _operation == Operation::CREATE_SNAPSHOT; }
			bool discard_snapshot() const { return _operation == Operation::DISCARD_SNAPSHOT; }
			bool rekey()            const { return _operation == Operation::REKEY; }
			bool extend_vbd()       const { return _operation == Operation::EXTEND_VBD; }
			bool extend_ft()        const { return _operation == Operation::EXTEND_FT; }
			bool resume_rekeying()  const { return _operation == Operation::RESUME_REKEYING; }
			bool deinitialize()     const { return _operation == Operation::DEINITIALIZE; }
			bool initialize()       const { return _operation == Operation::INITIALIZE; }

			Operation        operation()    const { return _operation; }
			bool             success()      const { return _success; }
			uint64_t         block_number() const { return _block_number; }
			uint64_t         offset()       const { return _offset; }
			Number_of_blocks count()        const { return _count; }
			uint32_t         key_id()       const { return _key_id; }
			uint32_t         tag()          const { return _tag; }
			Generation       gen()          const { return _gen; }

			void offset(uint64_t arg)  { _offset = arg; }
			void success(bool arg)     { _success = arg; }
			void tag(uint32_t arg)     { _tag = arg; }
			void gen(Generation arg)   { _gen = arg; }

			static char const *op_to_string(Operation op);


			/********************
			 ** Module_request **
			 ********************/

			void print(Output &out) const override
			{
				Genode::print(out, op_to_string(_operation));
				switch (_operation) {
				case READ:
				case WRITE:
				case SYNC:
					if (_count > 1)
						Genode::print(out, " vbas ", _block_number, "..", _block_number + _count - 1);
					else
						Genode::print(out, " vba ", _block_number);
					break;
				default:
					break;
				}
			}
	};
}

class Tresor::Request_pool_channel
{
	private:

		friend class Request_pool;

		enum State {
			INVALID,
			SUBMITTED,
			SUBMITTED_RESUME_REKEYING,
			REKEY_INIT_PENDING,
			REKEY_INIT_IN_PROGRESS,
			REKEY_INIT_COMPLETE,
			PREPONE_REQUESTS_PENDING,
			PREPONE_REQUESTS_COMPLETE,
			VBD_EXTENSION_STEP_PENDING,
			FT_EXTENSION_STEP_PENDING,
			TREE_EXTENSION_STEP_IN_PROGRESS,
			TREE_EXTENSION_STEP_COMPLETE,
			CREATE_SNAP_AT_SB_CTRL_PENDING,
			CREATE_SNAP_AT_SB_CTRL_IN_PROGRESS,
			CREATE_SNAP_AT_SB_CTRL_COMPLETE,
			SYNC_AT_SB_CTRL_PENDING,
			SYNC_AT_SB_CTRL_IN_PROGRESS,
			SYNC_AT_SB_CTRL_COMPLETE,
			READ_VBA_AT_SB_CTRL_PENDING,
			READ_VBA_AT_SB_CTRL_IN_PROGRESS,
			READ_VBA_AT_SB_CTRL_COMPLETE,
			WRITE_VBA_AT_SB_CTRL_PENDING,
			WRITE_VBA_AT_SB_CTRL_IN_PROGRESS,
			WRITE_VBA_AT_SB_CTRL_COMPLETE,
			DISCARD_SNAP_AT_SB_CTRL_PENDING,
			DISCARD_SNAP_AT_SB_CTRL_IN_PROGRESS,
			DISCARD_SNAP_AT_SB_CTRL_COMPLETE,
			REKEY_VBA_PENDING,
			REKEY_VBA_IN_PROGRESS,
			REKEY_VBA_COMPLETE,
			INITIALIZE_SB_CTRL_PENDING,
			INITIALIZE_SB_CTRL_IN_PROGRESS,
			INITIALIZE_SB_CTRL_COMPLETE,
			DEINITIALIZE_SB_CTRL_PENDING,
			DEINITIALIZE_SB_CTRL_IN_PROGRESS,
			DEINITIALIZE_SB_CTRL_COMPLETE,
			COMPLETE
		};

		enum Tag_type {
			TAG_POOL_SB_CTRL_TREE_EXT_STEP,
			TAG_POOL_SB_CTRL_READ_VBA,
			TAG_POOL_SB_CTRL_WRITE_VBA,
			TAG_POOL_SB_CTRL_SYNC,
			TAG_POOL_SB_CTRL_INITIALIZE,
			TAG_POOL_SB_CTRL_DEINITIALIZE,
			TAG_POOL_SB_CTRL_INIT_REKEY,
			TAG_POOL_SB_CTRL_REKEY_VBA,
			TAG_POOL_SB_CTRL_CREATE_SNAP,
			TAG_POOL_SB_CTRL_DISCARD_SNAP
		};

		using Pool_index = uint32_t;

		struct Generated_prim {
			enum Type { READ, WRITE };

			Type       op;
			bool       succ;
			Tag_type   tg;
			Pool_index pl_idx;
			uint64_t   blk_nr;
			uint64_t   idx;
		};

		Tresor::Request   _request                 { };
		State             _state                   { INVALID };
		Generated_prim    _prim                    { };
		uint64_t          _nr_of_blks              { 0 };
		Superblock::State _sb_state                { Superblock::INVALID };
		uint32_t          _nr_of_requests_preponed { 0 };
		bool              _request_finished        { false };

		void invalidate()
		{
			_request    = { };
			_state      = { INVALID };
			_prim       = { };
			_nr_of_blks =  0;
			_sb_state   = { Superblock::INVALID };
		}
};

class Tresor::Request_pool : public Module
{
	private:

		enum { MAX_NR_OF_REQUESTS_PREPONED_AT_A_TIME = 8 };

		using Channel = Request_pool_channel;
		using Request = Tresor::Request;
		using Slots_index = uint32_t;
		using Pool_index = Channel::Pool_index;
		using Generated_prim = Channel::Generated_prim;

		enum { NR_OF_CHANNELS = 16 };

		struct Index_queue
		{
			using Index = Slots_index;

			Slots_index _head                  { 0 };
			Slots_index _tail                  { 0 };
			unsigned    _nr_of_used_slots      { 0 };
			Slots_index _slots[NR_OF_CHANNELS] { 0 };

			bool empty() const { return _nr_of_used_slots == 0; }

			bool full() const {
				return _nr_of_used_slots >= NR_OF_CHANNELS; }

			Slots_index head() const
			{
				if (empty()) {
					class Index_queue_empty_head { };
					throw Index_queue_empty_head { };
				}
				return _slots[_head];
			}

			void enqueue(Slots_index const idx)
			{
				if (full()) {
					class Index_queue_enqueue_full { };
					throw Index_queue_enqueue_full { };
				}

				_slots[_tail] = idx;

				_tail = (_tail + 1) % NR_OF_CHANNELS;

				_nr_of_used_slots += 1;
			}

			void move_one_item_towards_tail(Index idx)
			{
				Slots_index slot_idx { _head };
				Slots_index next_slot_idx;
				Index next_idx;

				if (empty()) {
					class Exception_1 { };
					throw Exception_1 { };
				}
				while (true) {

					if (slot_idx < NR_OF_CHANNELS - 1)
						next_slot_idx = slot_idx + 1;
					else
						next_slot_idx = 0;

					if (next_slot_idx == _tail) {
						class Exception_2 { };
						throw Exception_2 { };
					}
					if (_slots[slot_idx] == idx) {
						next_idx = _slots[next_slot_idx];
						_slots[next_slot_idx] = _slots[slot_idx];
						_slots[slot_idx] = next_idx;
						return;
					} else
						slot_idx = next_slot_idx;
				}
			}

			bool item_is_tail(Slots_index idx) const
			{
				Slots_index slot_idx;

				if (empty()) {
					class Exception_1 { };
					throw Exception_1 { };
				}
				if (_tail > 0)
					slot_idx = _tail - 1;
				else
					slot_idx = NR_OF_CHANNELS - 1;

				return _slots[slot_idx] == idx;
			}

			Index next_item(Index idx) const
			{
				Slots_index slot_idx { _head };
				Slots_index next_slot_idx;
				if (empty()) {
					class Exception_1 { };
					throw Exception_1 { };
				}
				while (true) {

					if (slot_idx < NR_OF_CHANNELS - 1)
						next_slot_idx = slot_idx + 1;
					else
						next_slot_idx = 0;

					if (next_slot_idx == _tail) {
						class Exception_2 { };
						throw Exception_2 { };
					}
					if (_slots[slot_idx] == idx)
						return _slots[next_slot_idx];
					else
						slot_idx = next_slot_idx;
				}
			}

			void dequeue(Slots_index const idx)
			{
				if (empty() or head() != idx) {
					class Index_queue_dequeue_error { };
					throw Index_queue_dequeue_error { };
				}

				_head = (_head + 1) % NR_OF_CHANNELS;

				_nr_of_used_slots -= 1;
			}
		};

		static char const *_state_to_step_label(Channel::State state);

		void _mark_req_successful(Channel     &chan,
		                          Slots_index  idx,
		                          bool        &progress);

		bool _handle_failed_generated_req(Channel &chan,
		                                  bool    &progress);

		void _mark_req_failed(Channel    &chan,
		                      bool       &progress,
		                      char const *str);

		Channel     _channels[NR_OF_CHANNELS] { };
		Index_queue _indices { };

		void _execute_read (Channel &, Index_queue &, Slots_index const, bool &);

		void _execute_write(Channel &, Index_queue &, Slots_index const, bool &);

		void _execute_sync (Channel &, Index_queue &, Slots_index const, bool &);

		void _execute_create_snap(Channel &channel,
		                          Index_queue &indices,
		                          Slots_index const idx,
		                          bool &progress);

		void _execute_discard_snap(Channel &channel,
		                           Index_queue &indices,
		                           Slots_index const idx,
		                           bool &progress);

		void _execute_rekey(Channel     &chan,
		                    Index_queue &indices,
		                    Slots_index  idx,
		                    bool        &progress);

		void _execute_extend_tree(Channel       &chan,
		                         Slots_index     idx,
		                         Channel::State  tree_ext_step_pending,
		                         bool           &progress);

		void _execute_initialize(Channel &, Index_queue &, Slots_index const,
		                         bool &);
		void _execute_deinitialize(Channel &, Index_queue &, Slots_index const,
		                           bool &);


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		void execute(bool &) override;

		bool _peek_generated_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

		void generated_request_complete(Module_request &req) override;

	public:

		Request_pool();


		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override { return !_indices.full(); }

		void submit_request(Module_request &req) override;
};


inline char const *to_string(Tresor::Request::Operation op)
{
	switch (op) {
	case Tresor::Request::INVALID: return "invalid";
	case Tresor::Request::READ: return "read";
	case Tresor::Request::WRITE: return "write";
	case Tresor::Request::SYNC: return "sync";
	case Tresor::Request::CREATE_SNAPSHOT: return "create_snapshot";
	case Tresor::Request::DISCARD_SNAPSHOT: return "discard_snapshot";
	case Tresor::Request::REKEY: return "rekey";
	case Tresor::Request::EXTEND_VBD: return "extend_vbd";
	case Tresor::Request::EXTEND_FT: return "extend_ft";
	case Tresor::Request::RESUME_REKEYING: return "resume_rekeying";
	case Tresor::Request::DEINITIALIZE: return "deinitialize";
	case Tresor::Request::INITIALIZE: return "initialize";
	}
	class Exception_1 { };
	throw Exception_1 { };
}

#endif /* _TRESOR__REQUEST_POOL_H_ */
