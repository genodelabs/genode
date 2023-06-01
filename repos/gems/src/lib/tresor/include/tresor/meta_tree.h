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

#ifndef _TRESOR__META_TREE_H_
#define _TRESOR__META_TREE_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/module.h>
#include <tresor/sha256_4k_hash.h>

namespace Tresor {

	class Meta_tree;
	class Meta_tree_request;
	class Meta_tree_channel;
}

class Tresor::Meta_tree_request : public Module_request
{
	public:

		enum Type { INVALID = 0, UPDATE = 1 };

	private:

		friend class Meta_tree;
		friend class Meta_tree_channel;

		Type     _type             { INVALID };
		addr_t   _mt_root_pba_ptr  { 0 };
		addr_t   _mt_root_gen_ptr  { 0 };
		addr_t   _mt_root_hash_ptr { 0 };
		uint64_t _mt_max_lvl       { 0 };
		uint64_t _mt_edges         { 0 };
		uint64_t _mt_leaves        { 0 };
		uint64_t _current_gen      { 0 };
		uint64_t _old_pba          { INVALID_PBA };
		uint64_t _new_pba          { INVALID_PBA };
		bool     _success          { false };

	public:

		Meta_tree_request() { }

		Meta_tree_request(Module_id         src_module_id,
		                  Module_request_id src_request_id);

		static void create(void     *buf_ptr,
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
		                   uint64_t  old_pba);

		uint64_t new_pba() { return _new_pba; }

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);

		char const *type_name() const { return type_to_string(_type); }


		/********************
		 ** Module_request **
		 ********************/

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};


class Tresor::Meta_tree_channel
{
	private:

		friend class Meta_tree;

		enum State {
			INVALID,
			UPDATE,
			COMPLETE,
			TREE_HASH_MISMATCH
		};

		struct Type_1_info
		{
			enum State {
				INVALID, READ, READ_COMPLETE, WRITE, WRITE_COMPLETE, COMPLETE };

			State             state   { INVALID };
			Type_1_node       node    { };
			Type_1_node_block entries { };
			uint8_t           index   { INVALID_NODE_INDEX };
			bool              dirty   { false };
			bool              volatil { false };
		};

		struct Type_2_info
		{
			enum State {
				INVALID, READ, READ_COMPLETE, WRITE, WRITE_COMPLETE, COMPLETE };

			State             state   { INVALID };
			Type_1_node       node    { };
			Type_2_node_block entries { };
			uint8_t           index   { INVALID_NODE_INDEX };
			bool              volatil { false };
		};

		struct Local_cache_request
		{
			enum State { INVALID, PENDING, IN_PROGRESS };
			enum Op { READ, WRITE, SYNC };

			State    state                  { INVALID };
			Op       op                     { READ };
			bool     success                { false };
			uint64_t pba                    { 0 };
			uint64_t level                  { 0 };
			uint8_t  block_data[BLOCK_SIZE] { 0 };

			Local_cache_request(State     state,
			                    Op        op,
			                    bool      success,
			                    uint64_t  pba,
			                    uint64_t  level,
			                    uint8_t  *blk_ptr)
			:
				state   { state },
				op      { op },
				success { success },
				pba     { pba },
				level   { level }
			{
				if (blk_ptr != nullptr) {
					memcpy(&block_data, blk_ptr, BLOCK_SIZE);
				}
			}

			Local_cache_request() { }
		};

		State               _state                                { INVALID };
		Meta_tree_request   _request                              { };
		Local_cache_request _cache_request                        { };
		Type_2_info         _level_1_node                         { };
		Type_1_info         _level_n_nodes[TREE_MAX_NR_OF_LEVELS] { }; /* index starts at 2 */
		bool                _finished                             { false };
		bool                _root_dirty                           { false };
};

class Tresor::Meta_tree : public Module
{
	private:

		using Request = Meta_tree_request;
		using Channel = Meta_tree_channel;
		using Local_cache_request = Channel::Local_cache_request;
		using Type_1_info = Channel::Type_1_info;
		using Type_2_info = Channel::Type_2_info;

		enum { NR_OF_CHANNELS = 1 };

		Channel _channels[NR_OF_CHANNELS] { };

		void _handle_level_n_nodes(Channel &channel,
		                           bool    &handled);

		void _handle_level_1_node(Channel &channel,
		                          bool    &handled);

		void _exchange_request_pba(Channel     &channel,
		                           Type_2_node &t2_entry);

		void _exchange_nv_inner_nodes(Channel     &channel,
		                              Type_2_node &t2_entry,
		                              bool        &exchanged);

		void _exchange_nv_level_1_node(Channel     &channel,
		                               Type_2_node &t2_entry,
		                               bool        &exchanged);

		bool _node_volatile(Type_1_node const &node,
		                    uint64_t           gen);

		void _handle_level_0_nodes(Channel &channel,
		                           bool    &handled);

		void _update_parent(Type_1_node   &node,
		                    uint8_t const *blk_ptr,
		                    uint64_t       gen,
		                    uint64_t       pba);

		void _handle_level_0_nodes(bool &handled);

		void _execute_update(Channel &channel,
		                     bool    &progress);

		void _mark_req_failed(Channel    &channel,
		                      bool       &progress,
		                      char const *str);

		void _mark_req_successful(Channel &channel,
		                          bool    &progress);


		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		bool _peek_completed_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		void execute(bool &) override;

		bool _peek_generated_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

		void generated_request_complete(Module_request &req) override;

	public:

		Meta_tree();
};

#endif /* _TRESOR__META_TREE_H_ */
