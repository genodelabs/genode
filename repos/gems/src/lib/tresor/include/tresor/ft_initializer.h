/*
 * \brief  Module for initializing the FT
 * \author Josef Soentgen
 * \date   2023-03-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__FT_INITIALIZER_H_
#define _TRESOR__FT_INITIALIZER_H_

/* base includes */
#include <base/output.h>

/* tresor includes */
#include <tresor/module.h>

namespace Tresor {

	class Ft_initializer;
	class Ft_initializer_request;
	class Ft_initializer_channel;
}


class Tresor::Ft_initializer_request : public Module_request
{
	public:

		enum Type { INVALID = 0, INIT = 1, };

	private:

		friend class Ft_initializer;
		friend class Ft_initializer_channel;

		Type     _type                           { INVALID };
		uint8_t  _root_node[sizeof(Type_1_node)] { 0 };
		uint64_t _max_level_idx                  { 0 };
		uint64_t _max_child_idx                  { 0 };
		uint64_t _nr_of_leaves                   { 0 };
		bool     _success                        { false };


	public:

		Ft_initializer_request() { }

		Ft_initializer_request(Module_id         src_module_id,
		                        Module_request_id src_request_id);

		static void create(void     *buf_ptr,
		                   size_t    buf_size,
		                   uint64_t  src_module_id,
		                   uint64_t  src_request_id,
		                   size_t    req_type,
		                   uint64_t  max_level_idx,
		                   uint64_t  max_child_idx,
		                   uint64_t  nr_of_leaves);

		void *root_node() { return _root_node; }

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};


class Tresor::Ft_initializer_channel
{
	private:

		friend class Ft_initializer;

		enum State {
			INACTIVE, SUBMITTED, PENDING, IN_PROGRESS, COMPLETE,
			BLOCK_ALLOC_PENDING,
			BLOCK_ALLOC_IN_PROGRESS,
			BLOCK_ALLOC_COMPLETE,
			BLOCK_IO_PENDING,
			BLOCK_IO_IN_PROGRESS,
			BLOCK_IO_COMPLETE,
		};

		enum Child_state { DONE, INIT_BLOCK, INIT_NODE, WRITE_BLOCK, };

		struct Type_1_level
		{
			Type_1_node_block children { };
			Child_state       children_state[NR_OF_T1_NODES_PER_BLK] { DONE };
		};

		struct Type_2_level
		{
			Type_2_node_block children { };
			Child_state       children_state[NR_OF_T2_NODES_PER_BLK] { DONE };
		};

		struct Root_node
		{
			Type_1_node node  { };
			Child_state state { DONE };
		};

		State                  _state                     { INACTIVE };
		Ft_initializer_request _request                   { };
		Root_node              _root_node                 { };
		Type_1_level           _t1_levels[TREE_MAX_LEVEL] { };
		Type_2_level           _t2_level                  { };
		uint64_t               _level_to_write            { 0 };
		uint64_t               _blk_nr                    { 0 };
		uint64_t               _child_pba                 { 0 };
		bool                   _generated_req_success     { false };
		Block                  _encoded_blk               { };

		static void reset_node(Tresor::Type_1_node &node)
		{
			memset(&node, 0, sizeof(Type_1_node));
		}

		static void reset_node(Tresor::Type_2_node &node)
		{
			memset(&node, 0, sizeof(Type_2_node));
		}

		static void reset_level(Type_1_level &level,
		                        Child_state   state)
		{
			for (unsigned int i = 0; i < NR_OF_T1_NODES_PER_BLK; i++) {
				reset_node(level.children.nodes[i]);
				level.children_state[i] = state;
			}
		}

		static void reset_level(Type_2_level &level,
		                        Child_state   state)
		{
			for (unsigned int i = 0; i < NR_OF_T2_NODES_PER_BLK; i++) {
				reset_node(level.children.nodes[i]);
				level.children_state[i] = state;
			}
		}

		static void dump(Type_1_node_block const &node_block)
		{
			for (auto v : node_block.nodes) {
				if (v.pba != 0)
					log(v);
			}
		}

		static void dump(Type_2_node_block const &node_block)
		{
			for (auto v : node_block.nodes) {
				if (v.pba != 0)
					log(v);
			}
		}
};


class Tresor::Ft_initializer : public Module
{
	private:

		using Request = Ft_initializer_request;
		using Channel = Ft_initializer_channel;

		enum { NR_OF_CHANNELS = 1 };

		Channel _channels[NR_OF_CHANNELS] { };

		void _execute_leaf_child(Channel                             &channel,
		                         bool                                &progress,
                               uint64_t                            &nr_of_leaves,
		                         Tresor::Type_2_node                 &child,
		                         Ft_initializer_channel::Child_state &child_state,
		                         uint64_t                             child_index);

		void _execute_inner_t2_child(Channel                              &channel,
		                             bool                                 &progress,
		                             uint64_t                              nr_of_leaves,
		                             uint64_t                             &level_to_write,
		                             Tresor::Type_1_node                  &child,
		                             Ft_initializer_channel::Type_2_level &child_level,
		                             Ft_initializer_channel::Child_state  &child_state,
		                             uint64_t                              level_index,
		                             uint64_t                              child_index);

		void _execute_inner_t1_child(Channel                              &channel,
		                             bool                                 &progress,
		                             uint64_t                              nr_of_leaves,
		                             uint64_t                             &level_to_write,
		                             Tresor::Type_1_node                  &child,
		                             Ft_initializer_channel::Type_1_level &child_level,
		                             Ft_initializer_channel::Child_state  &child_state,
		                             uint64_t                              level_index,
		                             uint64_t                              child_index);

		void _execute(Channel &channel,
		              bool    &progress);

		void _execute_init(Channel &channel,
		                   bool    &progress);

		void _mark_req_failed(Channel    &channel,
		                      bool       &progress,
		                      char const *str);

		void _mark_req_successful(Channel &channel,
		                          bool    &progress);


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		bool _peek_generated_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

		void generated_request_complete(Module_request &req) override;


	public:

		Ft_initializer();

		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		void execute(bool &) override;

};

#endif /* _TRESOR__FT_INITIALIZER_H_ */
