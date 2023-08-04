/*
 * \brief  Framework for component internal modularization
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__MODULE_H_
#define _TRESOR__MODULE_H_

/* base includes */
#include <util/avl_tree.h>

/* tresor includes */
#include <tresor/verbosity.h>
#include <tresor/noncopyable.h>
#include <tresor/assertion.h>

namespace Tresor {

	using namespace Genode;

	using Module_id = uint64_t;
	using Module_channel_id = uint64_t;

	enum { INVALID_MODULE_ID = ~(Module_id)0, INVALID_MODULE_CHANNEL_ID = ~(Module_channel_id)0 };

	enum Module_id_enum : Module_id {
		CRYPTO = 0, CLIENT_DATA = 1, TRUST_ANCHOR = 2, COMMAND_POOL = 3, BLOCK_IO = 4, CACHE = 5, META_TREE = 6,
		FREE_TREE = 7, VIRTUAL_BLOCK_DEVICE = 8, SUPERBLOCK_CONTROL = 9, VBD_INITIALIZER = 10, FT_INITIALIZER = 11,
		SB_INITIALIZER = 12, REQUEST_POOL = 13, SB_CHECK = 14, VBD_CHECK = 15, FT_CHECK = 16, SPLITTER = 17, MAX_MODULE_ID = 17 };

	char const *module_name(Module_id module_id);

	class Module_request;
	class Module_channel;
	class Module;
	class Module_composition;
}


class Tresor::Module_request : public Interface
{
	private:

		Module_id _src_module_id;
		Module_channel_id _src_chan_id;
		Module_id _dst_module_id;
		Module_channel_id _dst_chan_id { INVALID_MODULE_CHANNEL_ID };

		NONCOPYABLE(Module_request);

	public:

		Module_request(Module_id, Module_channel_id, Module_id);

		void dst_chan_id(Module_channel_id id) { _dst_chan_id = id; }

		Module_id src_module_id() const { return _src_module_id; }
		Module_channel_id src_chan_id() const { return _src_chan_id; }
		Module_id dst_module_id() const { return _dst_module_id; }
		Module_channel_id dst_chan_id() const { return _dst_chan_id; }

		virtual void print(Output &) const = 0;

		virtual ~Module_request() { }
};


class Tresor::Module_channel : private Avl_node<Module_channel>
{
	friend class Module;
	friend class Avl_node<Module_channel>;
	friend class Avl_tree<Module_channel>;

	public:

		using State_uint = uint64_t;

	private:

		enum { GEN_REQ_BUF_SIZE = 4000 };

		enum Generated_request_state { NONE, PENDING, IN_PROGRESS };

		Module_request *_req_ptr { nullptr };
		Module_id _module_id;
		Module_channel_id _id;
		Generated_request_state _gen_req_state { NONE };
		uint8_t _gen_req_buf[GEN_REQ_BUF_SIZE] { };
		State_uint _gen_req_complete_state { 0 };

		NONCOPYABLE(Module_channel);

		bool higher(Module_channel *ptr) { return ptr->_id > _id; }

		virtual void _generated_req_completed(State_uint) { ASSERT_NEVER_REACHED; }

		virtual void _request_submitted(Module_request &) { ASSERT_NEVER_REACHED; }

		virtual bool _request_complete() { ASSERT_NEVER_REACHED; }

	public:

		Module_channel(Module_id module_id, Module_channel_id id) : _module_id { module_id }, _id { id } { };

		template <typename REQUEST, typename... ARGS>
		void generate_req(State_uint complete_state, bool &progress, ARGS &&... args)
		{
			ASSERT(_gen_req_state == NONE);
			static_assert(sizeof(REQUEST) <= GEN_REQ_BUF_SIZE);
			construct_at<REQUEST>(_gen_req_buf, _module_id, _id, args...);
			_gen_req_state = PENDING;
			_gen_req_complete_state = complete_state;
			progress = true;
		}

		template <typename CHAN, typename FUNC>
		void with_channel(Module_channel_id id, FUNC && func)
		{
			if (id != _id) {
				Module_channel *chan_ptr { Avl_node<Module_channel>::child(id > _id) };
				ASSERT(chan_ptr);
				chan_ptr->with_channel<CHAN>(id, func);
			} else
				func(*static_cast<CHAN *>(this));
		}

		void generated_req_completed();

		bool try_submit_request(Module_request &);

		Module_channel_id id() const { return _id; }

		virtual ~Module_channel() { }
};


class Tresor::Module : public Interface
{
	private:

		Avl_tree<Module_channel> _channels { };

		NONCOPYABLE(Module);

	public:

		template <typename CHAN = Module_channel, typename FUNC>
		void with_channel(Module_channel_id id, FUNC && func)
		{
			ASSERT(_channels.first());
			_channels.first()->with_channel<CHAN>(id, func);
		}

		template <typename CHAN = Module_channel, typename FUNC>
		void for_each_channel(FUNC && func)
		{
			_channels.for_each([&] (Module_channel const &const_chan) {
				func(*static_cast<CHAN *>(const_cast<Module_channel *>(&const_chan))); });
		}

		template <typename FUNC>
		void for_each_generated_request(FUNC && handle_request)
		{
			for_each_channel([&] (Module_channel &chan) {
				if (chan._gen_req_state != Module_channel::PENDING)
					return;

				Module_request &req = *(Module_request *)chan._gen_req_buf;
				if (handle_request(req)) {
					chan._gen_req_state = Module_channel::IN_PROGRESS;
					return;
				}
			});
		}

		template <typename FUNC>
		void for_each_completed_request(FUNC && handle_request)
		{
			for_each_channel([&] (Module_channel &chan) {
				if (chan._req_ptr && chan._request_complete()) {
					handle_request(*chan._req_ptr);
					chan._req_ptr = nullptr;
				}
			});
			return;
		}

		bool try_submit_request(Module_request &);

		void add_channel(Module_channel &chan) { _channels.insert(&chan); }

		Module() { }

		virtual ~Module() { }

		virtual void execute(bool &) { }
};


class Tresor::Module_composition
{
	private:

		Module *_module_ptrs[MAX_MODULE_ID + 1] { };

	public:

		void add_module(Module_id module_id, Module &mod);

		void remove_module(Module_id module_id);

		void execute_modules();
};

#endif /* _TRESOR__MODULE_H_ */
