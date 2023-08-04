/*
 * \brief  Module for checking all hashes of a free tree or meta tree
 * \author Martin Stein
 * \date   2023-05-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__FT_CHECK_H_
#define _TRESOR__FT_CHECK_H_

/* tresor includes */
#include <tresor/types.h>

namespace Tresor {

	class Ft_check;
	class Ft_check_request;
	class Ft_check_channel;
}


class Tresor::Ft_check_request : public Module_request
{
	friend class Ft_check_channel;

	private:

		Tree_root const &_ft;
		bool &_success;

		NONCOPYABLE(Ft_check_request);

	public:

		Ft_check_request(Module_id, Module_channel_id, Tree_root const &, bool &);

		void print(Output &out) const override { Genode::print(out, "check ", _ft); }
};


class Tresor::Ft_check_channel : public Module_channel
{
	private:

		using Request = Ft_check_request;

		enum State : State_uint { REQ_SUBMITTED, REQ_IN_PROGRESS, REQ_COMPLETE, REQ_GENERATED, READ_BLK_SUCCEEDED };

		State _state { REQ_COMPLETE };
		Type_1_node_block_walk _t1_blks { };
		Type_2_node_block _t2_blk { };
		bool _check_node[TREE_MAX_NR_OF_LEVELS + 1][NUM_NODES_PER_BLK] { };
		Number_of_leaves _num_remaining_leaves { 0 };
		Request *_req_ptr { };
		Block _blk { };
		bool _generated_req_success { false };

		NONCOPYABLE(Ft_check_channel);

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _mark_req_failed(bool &, Error_string);

		void _mark_req_successful(bool &);

		bool _execute_node(Tree_level_index, Tree_node_index, bool &);

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

	public:

		Ft_check_channel(Module_channel_id id) : Module_channel { FT_CHECK, id } { }

		void execute(bool &);
};


class Tresor::Ft_check : public Module
{
	private:

		using Channel = Ft_check_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Ft_check);

	public:

		Ft_check();

		void execute(bool &) override;
};

#endif /* _TRESOR__FT_CHECK_H_ */
