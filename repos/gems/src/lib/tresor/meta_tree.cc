/*
 * \brief  Module for doing free tree COW allocations on the meta tree
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/meta_tree.h>
#include <tresor/block_io.h>
#include <tresor/sha256_4k_hash.h>

using namespace Tresor;


/***************
 ** Utilities **
 ***************/

static bool check_level_0_usable(Generation   gen,
                                 Type_2_node &node)
{
	return node.alloc_gen != gen;
}


/***********************
 ** Meta_tree_request **
 ***********************/

void Meta_tree_request::create(void     *buf_ptr,
                               size_t    buf_size,
                               uint64_t  src_module_id,
                               uint64_t  src_request_id,
                               size_t    req_type,
                               void     *mt_root_pba_ptr,
                               void     *mt_root_gen_ptr,
                               void     *mt_root_hash_ptr,
                               uint64_t  mt_max_lvl,
                               uint64_t  mt_edges,
                               uint64_t  mt_leaves,
                               uint64_t  curr_gen,
                               uint64_t  old_pba)
{
	Meta_tree_request req { src_module_id, src_request_id };
	req._type             = (Type)req_type;
	req._mt_root_pba_ptr  = (addr_t)mt_root_pba_ptr;
	req._mt_root_gen_ptr  = (addr_t)mt_root_gen_ptr;
	req._mt_root_hash_ptr = (addr_t)mt_root_hash_ptr;
	req._mt_max_lvl       = mt_max_lvl;
	req._mt_edges         = mt_edges;
	req._mt_leaves        = mt_leaves;
	req._current_gen      = curr_gen;
	req._old_pba          = old_pba;
	if (sizeof(req) > buf_size) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


Meta_tree_request::Meta_tree_request(Module_id         src_module_id,
                                     Module_request_id src_request_id)
:
	Module_request { src_module_id, src_request_id, META_TREE }
{ }


char const *Meta_tree_request::type_to_string(Type type)
{
	switch (type) {
	case INVALID: return "invalid";
	case UPDATE: return "update";
	}
	return "?";
}


/***************
 ** Meta_tree **
 ***************/

bool Meta_tree::_peek_generated_request(uint8_t *buf_ptr,
                                        size_t   buf_size)
{
	for (uint32_t id { 0 }; id < NR_OF_CHANNELS; id++) {

		Channel &channel { _channels[id] };
		Local_cache_request const &local_req { channel._cache_request };
		if (local_req.state == Local_cache_request::PENDING) {

			Block_io_request::Type blk_io_req_type {
				local_req.op == Local_cache_request::READ ?
				                   Block_io_request::READ :
				                Local_cache_request::WRITE ?
				                   Block_io_request::WRITE :
				                   Block_io_request::INVALID };

			if (blk_io_req_type == Block_io_request::INVALID) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			construct_in_buf<Block_io_request>(
				buf_ptr, buf_size, META_TREE, id, blk_io_req_type,
				0, 0, 0, local_req.pba, 0, 1,
				channel._cache_request.block_data, nullptr);

			return true;
		}
	}
	return false;
}


void Meta_tree::_drop_generated_request(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Local_cache_request &local_req { _channels[id]._cache_request };
	if (local_req.state != Local_cache_request::PENDING) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	local_req.state = Local_cache_request::IN_PROGRESS;
}


void Meta_tree::generated_request_complete(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Local_cache_request &local_req { _channels[id]._cache_request };
	if (local_req.state != Local_cache_request::IN_PROGRESS) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	if (mod_req.dst_module_id() != BLOCK_IO) {
		class Exception_3 { };
		throw Exception_3 { };
	}
	Block_io_request &blk_io_req { *static_cast<Block_io_request *>(&mod_req) };
	Channel &channel { _channels[id] };
	if (!blk_io_req.success()) {

		channel._request._success = false;
		channel._request._new_pba = INVALID_PBA;
		channel._state = Channel::COMPLETE;
		return;

	}
	Type_1_info &t1_info { channel._level_n_nodes[local_req.level] };
	Type_2_info &t2_info { channel._level_1_node };

	switch (local_req.op) {
	case Local_cache_request::SYNC:

		class Exception_3 { };
		throw Exception_3 { };

	case Local_cache_request::READ:

		if (local_req.level > T2_NODE_LVL) {

			if (!check_sha256_4k_hash(channel._cache_request.block_data, &t1_info.node.hash)) {

				channel._state = Channel::TREE_HASH_MISMATCH;

			} else {

				memcpy(&t1_info.entries, channel._cache_request.block_data, BLOCK_SIZE);
				t1_info.index = 0;
				t1_info.state = Type_1_info::READ_COMPLETE;
			}
		} else if (local_req.level == T2_NODE_LVL) {

			if (!check_sha256_4k_hash(channel._cache_request.block_data, &t2_info.node.hash)) {

				channel._state = Channel::TREE_HASH_MISMATCH;

			} else {

				memcpy(&t2_info.entries, channel._cache_request.block_data, BLOCK_SIZE);
				t2_info.index = 0;
				t2_info.state = Type_2_info::READ_COMPLETE;
			}
		} else {
			class Exception_4 { };
			throw Exception_4 { };
		}
		break;

	case Local_cache_request::WRITE:

		if (local_req.level > T2_NODE_LVL) {

			t1_info.state = Type_1_info::WRITE_COMPLETE;

		} else if (local_req.level == T2_NODE_LVL) {

			t2_info.state = Type_2_info::WRITE_COMPLETE;

		} else {

			class Exception_5 { };
			throw Exception_5 { };
		}
		break;
	}
	local_req = Local_cache_request {
		Local_cache_request::INVALID, Local_cache_request::READ,
		false, 0, 0, nullptr };
}


void Meta_tree::_mark_req_failed(Channel    &chan,
                                 bool       &progress,
                                 char const *str)
{
	error(chan._request.type_name(), " request failed, reason: \"", str, "\"");
	chan._request._success = false;
	chan._state = Channel::COMPLETE;
	progress = true;
}


void Meta_tree::_mark_req_successful(Channel &channel,
                                     bool    &progress)
{
	channel._request._success = true;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Meta_tree::_update_parent(Type_1_node   &node,
                               uint8_t const *blk_ptr,
                               uint64_t       gen,
                               uint64_t       pba)
{
	calc_sha256_4k_hash(blk_ptr, &node.hash);
	node.gen = gen;
	node.pba = pba;
}


void Meta_tree::_exchange_nv_inner_nodes(Channel     &channel,
                                         Type_2_node &t2_entry,
                                         bool        &exchanged)
{
	Request &req { channel._request };
	uint64_t pba;
	exchanged = false;

	// loop non-volatile inner nodes
	for (uint64_t lvl { MT_LOWEST_T1_LVL }; lvl <= TREE_MAX_LEVEL; lvl++) {

		Type_1_info &t1_info { channel._level_n_nodes[lvl] };
		if (t1_info.node.valid() && !t1_info.volatil) {

			pba = t1_info.node.pba;
			t1_info.node.pba   = t2_entry.pba;
			t1_info.node.gen   = req._current_gen;
			t1_info.volatil    = true;
			t2_entry.pba       = pba;
			t2_entry.alloc_gen = req._current_gen;
			t2_entry.free_gen  = req._current_gen;
			t2_entry.reserved  = false;

			exchanged = true;
			break;
		}
	}
}


void Meta_tree::_exchange_nv_level_1_node(Channel     &channel,
                                          Type_2_node &t2_entry,
                                          bool        &exchanged)
{
	Request &req { channel._request };
	uint64_t pba { channel._level_1_node.node.pba };
	exchanged = false;

	if (!channel._level_1_node.volatil) {

		channel._level_1_node.node.pba = t2_entry.pba;
		channel._level_1_node.volatil  = true;

		t2_entry.pba       = pba;
		t2_entry.alloc_gen = req._current_gen;
		t2_entry.free_gen  = req._current_gen;
		t2_entry.reserved  = false;

		exchanged = true;
	}
}


void Meta_tree::_exchange_request_pba(Channel     &channel,
                                      Type_2_node &t2_entry)
{
	Request &req { channel._request };
	req._success = true;
	req._new_pba = t2_entry.pba;
	channel._finished = true;

	t2_entry.pba       = req._old_pba;
	t2_entry.alloc_gen = req._current_gen;
	t2_entry.free_gen  = req._current_gen;
	t2_entry.reserved  = false;
}


void Meta_tree::_handle_level_0_nodes(Channel &channel,
                                      bool    &handled)
{
	Request &req { channel._request };
	Type_2_node tmp_t2_entry;
	handled = false;

	for(unsigned i = 0; i <= req._mt_edges - 1; i++) {

		tmp_t2_entry = channel._level_1_node.entries.nodes[i];

		if (tmp_t2_entry.valid() &&
			check_level_0_usable(req._current_gen, tmp_t2_entry))
		{
			bool exchanged_level_1;
			bool exchanged_level_n { false };
			bool exchanged_request_pba { false };

			// first try to exchange the level 1 node ...
			_exchange_nv_level_1_node(
				channel, tmp_t2_entry, exchanged_level_1);

			// ... next the inner level n nodes ...
			if (!exchanged_level_1)
				_exchange_nv_inner_nodes(
					channel, tmp_t2_entry, exchanged_level_n);

			// ... and than satisfy the original mt request
			if (!exchanged_level_1 && !exchanged_level_n) {
				_exchange_request_pba(channel, tmp_t2_entry);
				exchanged_request_pba = true;
			}
			channel._level_1_node.entries.nodes[i] = tmp_t2_entry;
			handled = true;

			if (exchanged_request_pba)
				return;
		}
	}
}


void Meta_tree::_handle_level_1_node(Channel &channel,
                                     bool    &handled)
{
	Type_1_info &t1_info { channel._level_n_nodes[MT_LOWEST_T1_LVL] };
	Type_2_info &t2_info { channel._level_1_node };
	Request &req { channel._request };

	switch (t2_info.state) {
	case Type_2_info::INVALID:

		handled = false;
		break;

	case Type_2_info::READ:

		channel._cache_request = Local_cache_request {
			Local_cache_request::PENDING, Local_cache_request::READ, false,
			t2_info.node.pba, 1, nullptr };

		handled = true;
		break;

	case Type_2_info::READ_COMPLETE:

		_handle_level_0_nodes(channel, handled);
		if (handled) {
			t2_info.state = Type_2_info::WRITE;
		} else {
			t2_info.state = Type_2_info::COMPLETE;
			handled = true;
		}
		break;

	case Type_2_info::WRITE:
	{
		uint8_t block_data[BLOCK_SIZE];
		memcpy(block_data, &t2_info.entries, BLOCK_SIZE);

		_update_parent(
			t1_info.entries.nodes[t1_info.index], block_data, req._current_gen,
			t2_info.node.pba);

		channel._cache_request = Local_cache_request {
			Local_cache_request::PENDING, Local_cache_request::WRITE, false,
			t2_info.node.pba, 1, block_data };

		t1_info.dirty = true;
		handled = true;
		break;
	}
	case Type_2_info::WRITE_COMPLETE:

		t1_info.index++;
		t2_info.state = Type_2_info::INVALID;
		handled = true;
		break;

	case Type_2_info::COMPLETE:

		t1_info.index++;
		t2_info.state = Type_2_info::INVALID;
		handled = true;
		break;
	}
}


void Meta_tree::_execute_update(Channel &channel,
                                bool    &progress)
{
	bool handled_level_1_node;
	bool handled_level_n_nodes;
	_handle_level_1_node(channel, handled_level_1_node);
	if (handled_level_1_node) {
		progress = true;
		return;
	}
	_handle_level_n_nodes(channel, handled_level_n_nodes);
	progress = progress || handled_level_n_nodes;
}


void Meta_tree::execute(bool &progress)
{
	for (Channel &channel : _channels) {

		if (channel._cache_request.state != Local_cache_request::INVALID)
			continue;

		switch(channel._state) {
		case Channel::INVALID:
			break;
		case Channel::UPDATE:
			_execute_update(channel, progress);
			break;
		case Channel::COMPLETE:
			break;
		case Channel::TREE_HASH_MISMATCH:
			_mark_req_failed(channel, progress, "node hash mismatch");
			break;
		}
	}
}


Meta_tree::Meta_tree() { }


bool Meta_tree::_peek_completed_request(uint8_t *buf_ptr,
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


void Meta_tree::_drop_completed_request(Module_request &req)
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
	_channels[id]._state = Channel::INVALID;
}


bool Meta_tree::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::INVALID)
			return true;
	}
	return false;
}


bool Meta_tree::_node_volatile(Type_1_node const &node,
                               uint64_t           gen)
{
   return node.gen == 0 || node.gen != gen;
}


void Meta_tree::submit_request(Module_request &mod_req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		Channel &chan { _channels[id] };
		if (chan._state == Channel::INVALID) {

			mod_req.dst_request_id(id);

			chan._request = *static_cast<Request *>(&mod_req);
			chan._finished = false;
			chan._state = Channel::UPDATE;
			for (Type_1_info &t1_info : chan._level_n_nodes) {
				t1_info = Type_1_info { };
			}
			chan._level_1_node = Type_2_info { };

			Request &req { chan._request };
			Type_1_node root_node { };
			root_node.pba = *(uint64_t *)req._mt_root_pba_ptr;
			root_node.gen = *(uint64_t *)req._mt_root_gen_ptr;
			memcpy(&root_node.hash, (uint8_t *)req._mt_root_hash_ptr,
			       HASH_SIZE);

			chan._level_n_nodes[req._mt_max_lvl].node = root_node;
			chan._level_n_nodes[req._mt_max_lvl].state = Type_1_info::READ;
			chan._level_n_nodes[req._mt_max_lvl].volatil =
				_node_volatile(root_node, req._current_gen);

			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}


void Meta_tree::_handle_level_n_nodes(Channel &channel,
                                      bool    &handled)
{
	Request &req { channel._request };
	handled = false;

	for (uint64_t lvl { MT_LOWEST_T1_LVL }; lvl <= TREE_MAX_LEVEL; lvl++) {

		Type_1_info &t1_info { channel._level_n_nodes[lvl] };

		switch (t1_info.state) {
		case Type_1_info::INVALID:

			break;

		case Type_1_info::READ:

			channel._cache_request = Local_cache_request {
				Local_cache_request::PENDING, Local_cache_request::READ, false,
				t1_info.node.pba, lvl, nullptr };

			handled = true;
			return;

		case Type_1_info::READ_COMPLETE:

			if (t1_info.index < req._mt_edges &&
				t1_info.entries.nodes[t1_info.index].valid() &&
				!channel._finished) {

				if (lvl != MT_LOWEST_T1_LVL) {
					channel._level_n_nodes[lvl - 1] = {
						Type_1_info::READ, t1_info.entries.nodes[t1_info.index],
						{ }, 0, false,
						_node_volatile(t1_info.node, req._current_gen) };

				} else {
					channel._level_1_node = {
						Type_2_info::READ, t1_info.entries.nodes[t1_info.index],
						{ }, 0,
						_node_volatile(t1_info.node, req._current_gen) };
				}

			} else {

				if (t1_info.dirty)
					t1_info.state = Type_1_info::WRITE;
				else
					t1_info.state = Type_1_info::COMPLETE;
			}
			handled = true;
			return;

		case Type_1_info::WRITE:
		{
			uint8_t block_data[BLOCK_SIZE];
			memcpy(&block_data, &t1_info.entries, BLOCK_SIZE);

			if (lvl == req._mt_max_lvl) {

				Type_1_node root_node { };
				root_node.pba = *(uint64_t *)req._mt_root_pba_ptr;
				root_node.gen = *(uint64_t *)req._mt_root_gen_ptr;
				memcpy(&root_node.hash, (uint8_t *)req._mt_root_hash_ptr,
				       HASH_SIZE);

				_update_parent(
					root_node, block_data, req._current_gen,
					t1_info.node.pba);

				*(uint64_t *)req._mt_root_pba_ptr = root_node.pba;
				*(uint64_t *)req._mt_root_gen_ptr = root_node.gen;
				memcpy((uint8_t *)req._mt_root_hash_ptr, &root_node.hash,
				       HASH_SIZE);

				channel._root_dirty = true;

			} else {

				Type_1_info &parent { channel._level_n_nodes[lvl + 1] };
				_update_parent(
					parent.entries.nodes[parent.index], block_data,
					req._current_gen, t1_info.node.pba);

				parent.dirty = true;
			}
			channel._cache_request = Local_cache_request {
				Local_cache_request::PENDING, Local_cache_request::WRITE,
				false, t1_info.node.pba, lvl, block_data };

			handled = true;
			return;
		}
		case Type_1_info::WRITE_COMPLETE:

			if (lvl == req._mt_max_lvl)
				channel._state = Channel::COMPLETE;
			else
				channel._level_n_nodes[lvl + 1].index++;

			channel._cache_request = Local_cache_request {
				Local_cache_request::INVALID, Local_cache_request::READ,
				false, 0, 0, nullptr };

			t1_info.state = Type_1_info::INVALID;
			handled = true;
			return;

		case Type_1_info::COMPLETE:

			if (lvl == req._mt_max_lvl)
				channel._state = Channel::COMPLETE;
			else
				channel._level_n_nodes[lvl + 1].index++;

			t1_info.state = Type_1_info::INVALID;
			handled = true;
			return;
		}
	}
}
