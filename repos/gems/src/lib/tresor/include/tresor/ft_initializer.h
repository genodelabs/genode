/*
 * \brief  Module for initializing the FT
 * \author Martin Stein
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

/* tresor includes */
#include <tresor/types.h>

namespace Tresor {

	class Ft_initializer;
	class Ft_initializer_request;
	class Ft_initializer_channel;
}


class Tresor::Ft_initializer_request : public Module_request
{
	friend class Ft_initializer_channel;

	private:

		Tree_root &_ft;
		Pba_allocator &_pba_alloc;
		bool &_success;

		NONCOPYABLE(Ft_initializer_request);

	public:

		Ft_initializer_request(Module_id, Module_channel_id, Tree_root &, Pba_allocator &, bool &);

		void print(Output &out) const override { Genode::print(out, "init"); }
};


class Tresor::Ft_initializer_channel : public Module_channel
{
	private:

		using Request = Ft_initializer_request;

		enum State { REQ_GENERATED, REQ_SUBMITTED, EXECUTE_NODES, REQ_COMPLETE };

		enum Node_state { DONE, INIT_BLOCK, INIT_NODE, WRITE_BLK };

		State _state { REQ_COMPLETE };
		Request *_req_ptr { };
		Type_2_node_block _t2_blk { };
		Type_1_node_block_walk _t1_blks { };
		Node_state _t1_node_states[TREE_MAX_NR_OF_LEVELS][NUM_NODES_PER_BLK] { };
		Node_state _t2_node_states[NUM_NODES_PER_BLK] { };
		Number_of_leaves _num_remaining_leaves { 0 };
		bool _generated_req_success { false };
		Block _blk { };

		NONCOPYABLE(Ft_initializer_channel);

		void _reset_level(Tree_level_index, Node_state);

		void _generated_req_completed(State_uint) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _request_submitted(Module_request &) override;

		bool _execute_t2_node(Tree_node_index, bool &);

		bool _execute_t1_node(Tree_level_index, Tree_node_index, bool &);

		void _mark_req_failed(bool &, char const *);

		void _mark_req_successful(bool &);

	public:

		Ft_initializer_channel(Module_channel_id id) : Module_channel { FT_INITIALIZER, id } { }

		void execute(bool &);
};


class Tresor::Ft_initializer : public Module
{
	private:

		using Channel = Ft_initializer_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Ft_initializer);

	public:

		Ft_initializer();

		void execute(bool &) override;

};

#endif /* _TRESOR__FT_INITIALIZER_H_ */
