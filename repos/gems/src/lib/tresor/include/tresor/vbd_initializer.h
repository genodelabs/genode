/*
 * \brief  Module for initializing the VBD
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2023-03-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__VBD_INITIALIZER_H_
#define _TRESOR__VBD_INITIALIZER_H_

/* tresor includes */
#include <tresor/types.h>

namespace Tresor {

	class Vbd_initializer;
	class Vbd_initializer_request;
	class Vbd_initializer_channel;
}


class Tresor::Vbd_initializer_request : public Module_request
{
	friend class Vbd_initializer_channel;

	private:

		Tree_root &_vbd;
		Pba_allocator &_pba_alloc;
		bool &_success;

		NONCOPYABLE(Vbd_initializer_request);

	public:

		Vbd_initializer_request(Module_id, Module_channel_id, Tree_root &, Pba_allocator &, bool &);

		void print(Output &out) const override { Genode::print(out, "init"); }
};


class Tresor::Vbd_initializer_channel : public Module_channel
{
	private:

		using Request = Vbd_initializer_request;

		enum State { REQ_GENERATED, SUBMITTED, COMPLETE, EXECUTE_NODES };

		enum Node_state { DONE, INIT_BLOCK, INIT_NODE, WRITE_BLOCK };

		State _state { COMPLETE };
		Vbd_initializer_request *_req_ptr { };
		Type_1_node_block_walk _t1_blks { };
		Node_state _node_states[TREE_MAX_NR_OF_LEVELS][NUM_NODES_PER_BLK] { DONE };
		bool _generated_req_success { false };
		Block _blk { };
		Number_of_leaves _num_remaining_leaves { };

		NONCOPYABLE(Vbd_initializer_channel);

		void _generated_req_completed(State_uint) override;

		bool _request_complete() override { return _state == COMPLETE; }

		void _request_submitted(Module_request &) override;

		void _reset_level(Tree_level_index, Node_state);

		bool _execute_node(Tree_level_index, Tree_node_index, bool &);

		void _mark_req_failed(bool &, char const *);

		void _mark_req_successful(bool &);

	public:

		Vbd_initializer_channel(Module_channel_id id) : Module_channel { VBD_INITIALIZER, id } { }

		void execute(bool &);
};


class Tresor::Vbd_initializer : public Module
{
	private:

		using Channel = Vbd_initializer_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Vbd_initializer);

	public:

		Vbd_initializer();

		void execute(bool &) override;
};

#endif /* _TRESOR__VBD_INITIALIZER_H_ */
