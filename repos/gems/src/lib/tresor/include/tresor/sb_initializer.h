/*
 * \brief  Module for initializing the superblocks of a new Tresor
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2023-03-14
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__SB_INITIALIZER_H_
#define _TRESOR__SB_INITIALIZER_H_

/* tresor includes */
#include <tresor/types.h>

namespace Tresor {

	class Sb_initializer;
	class Sb_initializer_request;
	class Sb_initializer_channel;
}


class Tresor::Sb_initializer_request : public Module_request
{
	friend class Sb_initializer_channel;

	private:

		Tree_level_index _vbd_max_lvl;
		Tree_degree _vbd_degree;
		Number_of_leaves _vbd_num_leaves;
		Tree_level_index _ft_max_lvl;
		Tree_degree _ft_degree;
		Number_of_leaves _ft_num_leaves;
		Tree_level_index _mt_max_lvl;
		Tree_degree _mt_degree;
		Number_of_leaves _mt_num_leaves;
		Pba_allocator &_pba_alloc;
		bool &_success;

		NONCOPYABLE(Sb_initializer_request);

	public:

		Sb_initializer_request(Module_id, Module_channel_id, Tree_level_index, Tree_degree, Number_of_leaves,
		                       Tree_level_index, Tree_degree, Number_of_leaves, Tree_level_index, Tree_degree,
		                       Number_of_leaves, Pba_allocator &, bool &);

		void print(Output &out) const override { Genode::print(out, "init"); }
};


class Tresor::Sb_initializer_channel : public Module_channel
{
	private:

		using Request = Sb_initializer_request;

		enum State {
			REQ_SUBMITTED, START_NEXT_SB, SB_COMPLETE, REQ_COMPLETE, INIT_FT_SUCCEEDED, INIT_MT_SUCCEEDED,
			WRITE_HASH_TO_TA, CREATE_KEY_SUCCEEDED, ENCRYPT_KEY_SUCCEEDED, SECURE_SB_SUCCEEDED, INIT_VBD_SUCCEEDED,
			WRITE_BLK_SUCCEEDED, REQ_GENERATED };

		State _state { REQ_COMPLETE };
		Request *_req_ptr { };
		Superblock_index _sb_idx { 0 };
		Superblock _sb { };
		Block _blk { };
		Hash _hash { };
		Constructible<Tree_root> _vbd { };
		Constructible<Tree_root> _mt { };
		Constructible<Tree_root> _ft { };
		bool _generated_req_success { false };

		NONCOPYABLE(Sb_initializer_channel);

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

		void _mark_req_successful(bool &);

	public:

		Sb_initializer_channel(Module_channel_id id) : Module_channel { SB_INITIALIZER, id } { }

		void execute(bool &);
};


class Tresor::Sb_initializer : public Module
{
	private:

		using Channel = Sb_initializer_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Sb_initializer);

	public:

		Sb_initializer();

		void execute(bool &) override;
};

#endif /* _TRESOR__SB_INITIALIZER_H_ */
