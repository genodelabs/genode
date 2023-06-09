/*
 * \brief  Module for checking all hashes of a VBD snapshot
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
#include <tresor/vbd_check.h>
#include <tresor/block_io.h>
#include <tresor/sha256_4k_hash.h>

using namespace Tresor;


/***********************
 ** Vbd_check_request **
 ***********************/

Vbd_check_request::Vbd_check_request(uint64_t          src_module_id,
                                     uint64_t          src_request_id,
                                     Type              type,
                                     Tree_level_index  max_lvl,
                                     Tree_node_index max_child_idx,
                                     Number_of_leaves  nr_of_leaves,
                                     Type_1_node       root)
:
	Module_request { src_module_id, src_request_id, VBD_CHECK },
	_type          { type },
	_max_lvl       { max_lvl },
	_max_child_idx { max_child_idx },
	_nr_of_leaves  { nr_of_leaves },
	_root          { root }
{ }


char const *Vbd_check_request::type_to_string(Type type)
{
	switch (type) {
	case INVALID: return "invalid";
	case CHECK:   return "check";
	}
	return "?";
}


/***************
 ** Vbd_check **
 ***************/

void Vbd_check::_execute_inner_t1_child(Channel           &chan,
                                        Type_1_node const &child,
                                        Type_1_level      &child_lvl,
                                        Child_state       &child_state,
                                        Tree_level_index   lvl,
                                        Tree_node_index  child_idx,
                                        bool              &progress)
{
	Request &req { chan._request };
	if (child_state == Channel::READ_BLOCK) {

		if (!child.valid()) {

			if (req._nr_of_leaves == 0) {

				child_state = Channel::DONE;
				progress = true;

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._max_lvl },
					    "    lvl ", lvl, " child ", child_idx,
					    ": expectedly invalid");

			} else {

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._max_lvl },
					    "    lvl ", lvl, " child ", child_idx, " (", child,"): unexpectedly invalid");

				_mark_req_failed(chan, progress, "check for valid child");
			}

		} else if (!chan._gen_prim.valid()) {

			chan._gen_prim = {
				.success = false,
				.tag = Channel::BLOCK_IO,
				.blk_nr = child.pba,
				.dropped = false };

			chan._lvl_to_read = lvl - 1;
			progress = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._max_lvl },
				    "    lvl ", lvl, " child ", child_idx, " (", child, "): load to lvl ", lvl - 1);

		} else if (chan._gen_prim.tag != Channel::BLOCK_IO ||
		           chan._gen_prim.blk_nr != child.pba) {

			class Exception_1 { };
			throw Exception_1 { };

		} else if (!chan._gen_prim.success) {

		} else {

			for (Child_state &state : child_lvl.children_state) {
				state = Channel::READ_BLOCK;
			}
			chan._gen_prim = { };
			child_state = Channel::CHECK_HASH;
			progress = true;
		}

	} else if (child_state == Channel::CHECK_HASH) {

		Block blk { };
		child_lvl.children.encode_to_blk(blk);

		if (child.gen == INITIAL_GENERATION ||
		    check_sha256_4k_hash(blk, child.hash)) {

			child_state = Channel::DONE;
			if (&child_state == &chan._root_state) {
				chan._request._success = true;
			}
			progress = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._max_lvl },
				    "    lvl ", lvl, " child ", child_idx, ": good hash");

		} else {

			if (VERBOSE_CHECK) {

				Hash hash;
				calc_sha256_4k_hash(blk, hash);
				log(Level_indent { lvl, req._max_lvl },
				    "    lvl ", lvl, " child ", child_idx, " (", child, "): bad hash ", hash);
			}

			_mark_req_failed(chan, progress, "check inner hash");
		}
	}
}


void Vbd_check::_execute_leaf_child(Channel           &chan,
                                    Type_1_node const &child,
                                    Block       const &child_lvl,
                                    Child_state       &child_state,
                                    Tree_level_index   lvl,
                                    Tree_node_index  child_idx,
                                    bool              &progress)
{
	Request &req { chan._request };
	if (child_state == Channel::READ_BLOCK) {

		if (req._nr_of_leaves == 0) {

			if (child.valid()) {

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._max_lvl },
					    "    lvl ", lvl, " child ", child_idx, " (", child, "): unexpectedly valid");

				_mark_req_failed(chan, progress, "check for unused child");

			} else {

				child_state = Channel::DONE;
				progress = true;

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._max_lvl },
					    "    lvl ", lvl, " child ", child_idx, ": expectedly invalid");
			}

		} else if (child.gen == INITIAL_GENERATION) {

			req._nr_of_leaves--;
			child_state = Channel::DONE;
			progress = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._max_lvl },
				    "    lvl ", lvl, " child ", child_idx, ": uninitialized");

		} else if (!chan._gen_prim.valid()) {

			chan._gen_prim = {
				.success = false,
				.tag = Channel::BLOCK_IO,
				.blk_nr = child.pba,
				.dropped = false };

			chan._lvl_to_read = lvl - 1;
			progress = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._max_lvl },
				    "    lvl ", lvl, " child ", child_idx, " (", child, "): load to lvl ", lvl - 1);

		} else if (chan._gen_prim.tag != Channel::BLOCK_IO ||
		           chan._gen_prim.blk_nr != child.pba) {

			class Exception_1 { };
			throw Exception_1 { };

		} else if (!chan._gen_prim.success) {

		} else {

			chan._gen_prim = { };
			child_state = Channel::CHECK_HASH;
			progress = true;
		}

	} else if (child_state == Channel::CHECK_HASH) {

		if (check_sha256_4k_hash(child_lvl, child.hash)) {

			req._nr_of_leaves--;
			child_state = Channel::DONE;
			progress = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._max_lvl },
				    "    lvl ", lvl, " child ", child_idx, ": good hash");

		} else {

			if (VERBOSE_CHECK) {

				Hash hash;
				calc_sha256_4k_hash(child_lvl, hash);
				log(Level_indent { lvl, req._max_lvl },
				    "    lvl ", lvl, " child ", child_idx, " (", child, "): bad hash ", hash);
			}

			_mark_req_failed(chan, progress, "check leaf hash");
		}
	}
}


void Vbd_check::_execute_check(Channel &chan,
                               bool    &progress)
{
	Request &req { chan._request };
	for (Tree_level_index lvl { VBD_LOWEST_T1_LVL }; lvl <= req._max_lvl; lvl++) {
		for (Tree_node_index child_idx { 0 };
		     child_idx <= req._max_child_idx;
		     child_idx++) {

			Type_1_level &t1_lvl { chan._t1_lvls[lvl] };
			if (t1_lvl.children_state[child_idx] != Channel::DONE) {

				if (lvl == VBD_LOWEST_T1_LVL)
					_execute_leaf_child(
						chan,
						chan._t1_lvls[lvl].children.nodes[child_idx],
						chan._leaf_lvl,
						chan._t1_lvls[lvl].children_state[child_idx],
						lvl, child_idx, progress);
				else
					_execute_inner_t1_child(
						chan,
						chan._t1_lvls[lvl].children.nodes[child_idx],
						chan._t1_lvls[lvl - 1],
						chan._t1_lvls[lvl].children_state[child_idx],
						lvl, child_idx, progress);

				return;
			}
		}
	}
	if (chan._root_state != Channel::DONE) {

		_execute_inner_t1_child(
			chan, req._root, chan._t1_lvls[req._max_lvl], chan._root_state,
			req._max_lvl + 1, 0, progress);

		return;
	}
}


void Vbd_check::_mark_req_failed(Channel    &chan,
                                 bool       &progress,
                                 char const *str)
{
	error("vbd check: request (", chan._request, ") failed at step \"", str, "\"");
	chan._request._success = false;
	chan._root_state = Channel::DONE;
	progress = true;
}


bool Vbd_check::_peek_completed_request(uint8_t *buf_ptr,
                                        size_t   buf_size)
{
	for (Channel &chan : _channels) {

		if (chan._request._type != Request::INVALID &&
		    chan._root_state == Channel::DONE) {

			if (sizeof(chan._request) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			memcpy(buf_ptr, &chan._request, sizeof(chan._request));
			return true;
		}
	}
	return false;
}


void Vbd_check::_drop_completed_request(Module_request &req)
{
	Module_request_id id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {

		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	if (chan._request._type == Request::INVALID &&
	    chan._root_state != Channel::DONE) {

		class Exception_2 { };
		throw Exception_2 { };
	}
	chan = Channel { };
}


bool Vbd_check::_peek_generated_request(uint8_t *buf_ptr,
                                        size_t   buf_size)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {

		Channel &chan { _channels[id] };

		if (!chan._gen_prim.valid() || chan._gen_prim.dropped)
			continue;

		switch (chan._gen_prim.tag) {
		case Channel::BLOCK_IO:

			construct_in_buf<Block_io_request>(
				buf_ptr, buf_size, VBD_CHECK, id,
				Block_io_request::READ, 0, 0, 0,
				chan._gen_prim.blk_nr, 0, 1,
				chan._lvl_to_read == 0 ?
					(void *)&chan._leaf_lvl :
					(void *)&chan._encoded_blk,
				nullptr);

			return true;

		default:

			class Exception_1 { };
			throw Exception_1 { };
		}
	}
	return false;
}


void Vbd_check::_drop_generated_request(Module_request &req)
{
	Module_request_id const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_0 { };
		throw Exception_0 { };
	}
	Channel &chan { _channels[id] };
	chan._gen_prim.dropped = true;
}


void Vbd_check::generated_request_complete(Module_request &mod_req)
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
		chan._gen_prim.success = gen_req.success();
		if (chan._lvl_to_read > 0)
			chan._t1_lvls[chan._lvl_to_read].children.decode_from_blk(chan._encoded_blk);
		break;
	}
	default:
		class Exception_8 { };
		throw Exception_8 { };
	}
}


bool Vbd_check::ready_to_submit_request()
{
	for (Channel &chan : _channels) {
		if (chan._request._type == Request::INVALID)
			return true;
	}
	return false;
}


void Vbd_check::submit_request(Module_request &req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		Channel &chan { _channels[id] };
		if (chan._request._type == Request::INVALID) {
			req.dst_request_id(id);
			chan._request = *static_cast<Request *>(&req);
			chan._root_state = Channel::READ_BLOCK;
			return;
		}
	}
	class Exception_1 { };
	throw Exception_1 { };
}


void Vbd_check::execute(bool &progress)
{
	for (Channel &chan : _channels) {

		Request &req { chan._request };
		switch (req._type) {
		case Request::CHECK:

			_execute_check(chan, progress);
			break;

		default:

			break;
		}
	}
}
