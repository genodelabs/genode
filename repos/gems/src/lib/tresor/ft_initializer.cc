/*
 * \brief  Module for initializing the free tree
 * \author Josef Soentgen
 * \date   2023-03-09
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
#include <tresor/block_allocator.h>
#include <tresor/block_io.h>
#include <tresor/sha256_4k_hash.h>
#include <tresor/ft_initializer.h>

using namespace Tresor;

static constexpr bool DEBUG = false;


Ft_initializer_request::Ft_initializer_request(Module_id         src_module_id,
                                               Module_request_id src_request_id)
:
	Module_request { src_module_id, src_request_id, FT_INITIALIZER }
{ }


void Ft_initializer_request::create(void     *buf_ptr,
                                    size_t    buf_size,
                                    uint64_t  src_module_id,
                                    uint64_t  src_request_id,
                                    size_t    req_type,
                                    uint64_t  max_level_idx,
                                    uint64_t  max_child_idx,
                                    uint64_t  nr_of_leaves)
{
	Ft_initializer_request req { src_module_id, src_request_id };

	req._type          = (Type)req_type;
	req._max_level_idx = max_level_idx;
	req._max_child_idx = max_child_idx;
	req._nr_of_leaves  = nr_of_leaves;

	if (sizeof(req) > buf_size) {
		class Bad_size_0 { };
		throw Bad_size_0 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


char const *Ft_initializer_request::type_to_string(Type type)
{
	switch (type) {
	case INVALID: return "invalid";
	case INIT:    return "init";
	}
	return "?";
}


void Ft_initializer::_execute_leaf_child(Channel                              &channel,
                                          bool                                &progress,
                                          uint64_t                            &nr_of_leaves,
                                          Type_2_node                         &child,
                                          Ft_initializer_channel::Child_state &child_state,
                                          uint64_t                             child_index)
{
	using CS = Ft_initializer_channel::Child_state;

	switch (child_state) {
	case CS::INIT_BLOCK:
		child_state = CS::INIT_NODE;
		progress = true;
		return;

	case CS::INIT_NODE:
		if (nr_of_leaves == 0) {

			if (DEBUG)
				log("[ft_init] node: ", 1, " ", child_index,
				    " assign pba 0, leaf unused");

			Ft_initializer_channel::reset_node(child);
			child_state = CS::DONE;
			progress = true;
		} else {

			switch (channel._state) {
			case Channel::IN_PROGRESS:
				channel._state = Channel::BLOCK_ALLOC_PENDING;
				progress = true;
				break;

			case Channel::BLOCK_ALLOC_PENDING:
				break;

			case Channel::BLOCK_ALLOC_IN_PROGRESS:
				break;

			case Channel::BLOCK_ALLOC_COMPLETE:
				/* bail early in case the allocator failed */
				if (!channel._generated_req_success) {
					_mark_req_failed(channel, progress,
					                 "allocate block for FT initialization");
					break;
				}
				channel._state = Channel::IN_PROGRESS;

				Ft_initializer_channel::reset_node(child);

				child.pba = channel._blk_nr;
				child_state = CS::DONE;
				--nr_of_leaves;
				progress = true;

				if (DEBUG)
					log("[ft_init] node: ", 1, " ", child_index,
					    " assign pba: ", channel._blk_nr, " leaves left: ",
					    nr_of_leaves);
				break;

			default:
				break;
			}
		}
	default:
		break;
	}
}


void Ft_initializer::_execute_inner_t2_child(Channel                               &channel,
                                              bool                                 &progress,
                                              uint64_t                              nr_of_leaves,
                                              uint64_t                             &level_to_write,
                                              Type_1_node                          &child,
                                              Ft_initializer_channel::Type_2_level &child_level,
                                              Ft_initializer_channel::Child_state  &child_state,
                                              uint64_t                              level_index,
                                              uint64_t                              child_index)

{
	using CS = Ft_initializer_channel::Child_state;

	switch (child_state) {
	case CS::INIT_BLOCK:

		if (nr_of_leaves == 0) {

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " assign pba 0, inner node unused");

			Ft_initializer_channel::reset_node(child);
			child_state = CS::DONE;
			progress = true;
			return;
		} else {

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " reset level: ", level_index - 1);

			Ft_initializer_channel::reset_level(child_level, CS::INIT_BLOCK);
			child_state = CS::INIT_NODE;
			progress = true;
			return;
		}
		break;

	case CS::INIT_NODE:

		switch (channel._state) {
		case Channel::IN_PROGRESS:
			channel._state = Channel::BLOCK_ALLOC_PENDING;
			progress = true;
			break;

		case Channel::BLOCK_ALLOC_PENDING:
			break;

		case Channel::BLOCK_ALLOC_IN_PROGRESS:
			break;

		case Channel::BLOCK_ALLOC_COMPLETE:
		{
			/* bail early in case the allocator failed */
			if (!channel._generated_req_success) {
				_mark_req_failed(channel, progress,
				                 "allocate block for FT initialization");
				break;
			}
			channel._state = Channel::IN_PROGRESS;

			Ft_initializer_channel::reset_node(child);
			child.pba = channel._blk_nr;

			Block blk { };
			child_level.children.encode_to_blk(blk);
			calc_sha256_4k_hash(blk, child.hash);

			child_state = CS::WRITE_BLOCK;
			progress = true;

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " assign pba: ", channel._blk_nr);
			break;
		}
		default:
			break;
		}
		break;

	case CS::WRITE_BLOCK:

		switch (channel._state) {
		case Channel::IN_PROGRESS:
		{
			channel._state = Channel::BLOCK_IO_PENDING;
			channel._child_pba = child.pba;
			level_to_write = level_index - 1;
			progress = true;
			break;
		}

		case Channel::BLOCK_IO_PENDING:
			break;

		case Channel::BLOCK_IO_IN_PROGRESS:
			break;

		case Channel::BLOCK_IO_COMPLETE:
			/* bail early in case the allocator failed */
			if (!channel._generated_req_success) {
				_mark_req_failed(channel, progress,
				                 "write block for FT initialization");
				break;
			}
			channel._state = Channel::IN_PROGRESS;

			child_state = CS::DONE;
			progress = true;

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " write pba: ", channel._child_pba, " level: ",
				    level_index -1, " (child: ", child, ")");
			break;
		default:
			break;
		}

		break;

	default:
		break;
	}
}


void Ft_initializer::_execute_inner_t1_child(Channel                               &channel,
                                              bool                                 &progress,
                                              uint64_t                              nr_of_leaves,
                                              uint64_t                             &level_to_write,
                                              Type_1_node                          &child,
                                              Ft_initializer_channel::Type_1_level &child_level,
                                              Ft_initializer_channel::Child_state  &child_state,
                                              uint64_t                              level_index,
                                              uint64_t                              child_index)

{
	using CS = Ft_initializer_channel::Child_state;

	switch (child_state) {
	case CS::INIT_BLOCK:

		if (nr_of_leaves == 0) {

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " assign pba 0, inner node unused");

			Ft_initializer_channel::reset_node(child);
			child_state = CS::DONE;
			progress = true;
			return;
		} else {

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " reset level: ", level_index - 1);

			Ft_initializer_channel::reset_level(child_level, CS::INIT_BLOCK);
			child_state = CS::INIT_NODE;
			progress = true;
			return;
		}
		break;

	case CS::INIT_NODE:

		switch (channel._state) {
		case Channel::IN_PROGRESS:
			channel._state = Channel::BLOCK_ALLOC_PENDING;
			progress = true;
			break;

		case Channel::BLOCK_ALLOC_PENDING:
			break;

		case Channel::BLOCK_ALLOC_IN_PROGRESS:
			break;

		case Channel::BLOCK_ALLOC_COMPLETE:
		{
			/* bail early in case the allocator failed */
			if (!channel._generated_req_success) {
				_mark_req_failed(channel, progress,
				                 "allocate block for FT initialization");
				break;
			}
			channel._state = Channel::IN_PROGRESS;

			Ft_initializer_channel::reset_node(child);
			child.pba = channel._blk_nr;

			Block blk { };
			child_level.children.encode_to_blk(blk);
			calc_sha256_4k_hash(blk, child.hash);

			child_state = CS::WRITE_BLOCK;
			progress = true;

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " assign pba: ", channel._blk_nr);
			break;
		}
		default:
			break;
		}
		break;

	case CS::WRITE_BLOCK:

		switch (channel._state) {
		case Channel::IN_PROGRESS:
		{
			channel._state = Channel::BLOCK_IO_PENDING;
			channel._child_pba = child.pba;
			level_to_write = level_index - 1;
			progress = true;
			break;
		}

		case Channel::BLOCK_IO_PENDING:
			break;

		case Channel::BLOCK_IO_IN_PROGRESS:
			break;

		case Channel::BLOCK_IO_COMPLETE:
			/* bail early in case the allocator failed */
			if (!channel._generated_req_success) {
				_mark_req_failed(channel, progress,
				                 "write block for FT initialization");
				break;
			}
			channel._state = Channel::IN_PROGRESS;

			child_state = CS::DONE;
			progress = true;

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " write pba: ", channel._child_pba, " level: ",
				    level_index -1, " (child: ", child, ")");
			break;
		default:
			break;
		}

		break;

	default:
		break;
	}
}


void Ft_initializer::_execute(Channel &channel,
                               bool    &progress)
{
	Request &req { channel._request };

	/*
	 * First handle all leaf child nodes that starts after
	 * triggering the inner T2 nodes below.
	 */
	for (uint64_t child_idx = 0; child_idx <= req._max_child_idx; child_idx++) {

		Ft_initializer_channel::Child_state &state =
			channel._t2_level.children_state[child_idx];

		if (state != Ft_initializer_channel::Child_state::DONE) {

			Type_2_node &child =
				channel._t2_level.children.nodes[child_idx];

			_execute_leaf_child(channel, progress, req._nr_of_leaves,
			                    child, state, child_idx);
		}
	}

	/*
	 * Second handle all inner child nodes that starts after
	 * triggering the root node below.
	 */
	for (uint64_t level_idx = 1; level_idx <= req._max_level_idx; level_idx++) {

		for (uint64_t child_idx = 0; child_idx <= req._max_child_idx; child_idx++) {

			Ft_initializer_channel::Child_state &state =
				channel._t1_levels[level_idx].children_state[child_idx];

			if (state != Ft_initializer_channel::Child_state::DONE) {

				Type_1_node &child =
					channel._t1_levels[level_idx].children.nodes[child_idx];

				if (level_idx == 2) {
					Ft_initializer_channel::Type_2_level &t2_level =
						channel._t2_level;

					_execute_inner_t2_child(channel, progress,
					                        req._nr_of_leaves,
					                        channel._level_to_write,
					                        child, t2_level, state,
					                        level_idx, child_idx);
				} else {

					Ft_initializer_channel::Type_1_level &t1_level =
						channel._t1_levels[level_idx - 1];

					_execute_inner_t1_child(channel, progress,
					                        req._nr_of_leaves,
					                        channel._level_to_write,
					                        child, t1_level, state,
					                        level_idx, child_idx);
				}
				return;
			}
		}
	}

	/*
	 * Checking the root node will trigger the initialization process as
	 * well as will finish it.
	 */
	if (channel._root_node.state != Ft_initializer_channel::Child_state::DONE) {

		Ft_initializer_channel::Type_1_level &t1_level =
			channel._t1_levels[req._max_level_idx];

		_execute_inner_t1_child(channel, progress,
		                        req._nr_of_leaves,
		                        channel._level_to_write,
		                        channel._root_node.node, t1_level, channel._root_node.state,
		                        req._max_level_idx + 1, 0);
		return;
	}

	/*
	 * We will end up here when the root state is 'DONE'.
	 */
	if (req._nr_of_leaves == 0)
		_mark_req_successful(channel, progress);
	else
		_mark_req_failed(channel, progress, "initialize FT");
}


void Ft_initializer::_execute_init(Channel &channel,
                                    bool    &progress)
{
	switch (channel._state) {
	case Channel::SUBMITTED:

		/* clean residual state */
		for (unsigned int i = 0; i < TREE_MAX_LEVEL; i++) {
			Ft_initializer_channel::reset_level(channel._t1_levels[i],
			                                     Ft_initializer_channel::Child_state::DONE);
		}
		channel._level_to_write = 0;

		channel._state = Channel::PENDING;
		channel._root_node.state = Ft_initializer_channel::Child_state::INIT_BLOCK;
		progress = true;

		return;

	case Channel::PENDING:

		channel._state = Channel::IN_PROGRESS;
		progress = true;
		return;

	case Channel::IN_PROGRESS:

		_execute(channel, progress);
		return;

	case Channel::BLOCK_ALLOC_COMPLETE:

		_execute(channel, progress);
		return;

	case Channel::BLOCK_IO_COMPLETE:

		_execute(channel, progress);
		return;

	default:
		/*
		 * Omit other states related to ALLOC and IO as those
		 * are handled via Module API.
		 */
		return;
	}
}


void Ft_initializer::_mark_req_failed(Channel    &channel,
                                       bool       &progress,
                                       char const *str)
{
	error("request failed: failed to ", str);
	channel._request._success = false;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Ft_initializer::_mark_req_successful(Channel &channel,
                                           bool    &progress)
{
	Request &req { channel._request };

	memcpy(req._root_node, &channel._root_node.node, sizeof (req._root_node));
	req._success = true;

	channel._state = Channel::COMPLETE;
	progress = true;
}


bool Ft_initializer::_peek_completed_request(uint8_t *buf_ptr,
                                             size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::COMPLETE) {
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


void Ft_initializer::_drop_completed_request(Module_request &req)
{
	Module_request_id id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	if (_channels[id]._state != Channel::COMPLETE) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	_channels[id]._state = Channel::INACTIVE;
}


bool Ft_initializer::_peek_generated_request(uint8_t *buf_ptr,
                                             size_t   buf_size)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {

		Channel &channel { _channels[id] };

		if (channel._state != Ft_initializer_channel::State::INACTIVE)

		switch (channel._state) {
		case Ft_initializer_channel::State::BLOCK_ALLOC_PENDING:
		{
			Block_allocator_request::Type const block_allocator_req_type {
				Block_allocator_request::GET };

			Block_allocator_request::create(
				buf_ptr, buf_size, FT_INITIALIZER, id,
				block_allocator_req_type);

			return true;
		}
		case Ft_initializer_channel::State::BLOCK_IO_PENDING:
		{
			Block_io_request::Type const block_io_req_type {
				Block_io_request::WRITE };

			if (channel._level_to_write == 1)
				channel._t2_level.children.encode_to_blk(channel._encoded_blk);
			else
				channel._t1_levels[channel._level_to_write].children.encode_to_blk(channel._encoded_blk);

			construct_in_buf<Block_io_request>(
				buf_ptr, buf_size, FT_INITIALIZER, id, block_io_req_type, 0,
				0, 0, channel._child_pba, 0, 1,
				(void *)&channel._encoded_blk, nullptr);

			if (DEBUG) {
				log("BLOCK_IO_PENDING write ", channel._child_pba);
				if (channel._level_to_write == 1)
					Ft_initializer_channel::dump(channel._t2_level.children);
				else
					Ft_initializer_channel::dump(channel._t1_levels[channel._level_to_write].children);
			}

			return true;
		}
		default:
			break;
		}
	}
	return false;
}


void Ft_initializer::_drop_generated_request(Module_request &req)
{
	Module_request_id const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Bad_id { };
		throw Bad_id { };
	}
	switch (_channels[id]._state) {
	case Ft_initializer_channel::State::BLOCK_ALLOC_PENDING:
		_channels[id]._state = Ft_initializer_channel::State::BLOCK_ALLOC_IN_PROGRESS;
		break;
	case Ft_initializer_channel::State::BLOCK_IO_PENDING:
		_channels[id]._state = Ft_initializer_channel::State::BLOCK_IO_IN_PROGRESS;
		break;
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}


void Ft_initializer::generated_request_complete(Module_request &req)
{
	Module_request_id const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	switch (_channels[id]._state) {
	case Channel::BLOCK_ALLOC_IN_PROGRESS:
	{
		if (req.dst_module_id() != BLOCK_ALLOCATOR) {
			class Exception_3 { };
			throw Exception_3 { };
		}
		Block_allocator_request const *block_allocator_req = static_cast<Block_allocator_request const*>(&req);
		_channels[id]._state = Channel::BLOCK_ALLOC_COMPLETE;
		_channels[id]._blk_nr = block_allocator_req->blk_nr();
		_channels[id]._generated_req_success = block_allocator_req->success();
		break;
	}
	case Channel::BLOCK_IO_IN_PROGRESS:
	{
		if (req.dst_module_id() != BLOCK_IO) {
			class Exception_4 { };
			throw Exception_4 { };
		}
		Block_io_request const *block_io_req = static_cast<Block_io_request const*>(&req);
		_channels[id]._state = Channel::BLOCK_IO_COMPLETE;
		_channels[id]._generated_req_success = block_io_req->success();
		break;
	}
	default:
		class Exception_2 { };
		throw Exception_2 { };
	}
}


Ft_initializer::Ft_initializer()
{ }


bool Ft_initializer::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::INACTIVE)
			return true;
	}
	return false;
}


void Ft_initializer::submit_request(Module_request &req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		if (_channels[id]._state == Channel::INACTIVE) {
			req.dst_request_id(id);
			_channels[id]._request = *static_cast<Request *>(&req);
			_channels[id]._state = Channel::SUBMITTED;
			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}


void Ft_initializer::execute(bool &progress)
{
	for (Channel &channel : _channels) {

		if (channel._state == Channel::INACTIVE)
			continue;

		Request &req { channel._request };
		switch (req._type) {
		case Request::INIT:

			_execute_init(channel, progress);

			break;
		default:

			class Exception_1 { };
			throw Exception_1 { };
		}
	}
}
