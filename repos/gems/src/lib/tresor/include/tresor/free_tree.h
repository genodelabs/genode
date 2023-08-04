/*
 * \brief  Module for doing VBD COW allocations on the free tree
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__FREE_TREE_H_
#define _TRESOR__FREE_TREE_H_

/* tresor includes */
#include <tresor/types.h>

namespace Tresor {

	class Free_tree;
	class Free_tree_request;
	class Free_tree_channel;
}

class Tresor::Free_tree_request : public Module_request
{
	friend class Free_tree_channel;

	public:

		enum Type { ALLOC_FOR_NON_RKG, ALLOC_FOR_RKG_CURR_GEN_BLKS, ALLOC_FOR_RKG_OLD_GEN_BLKS, EXTENSION_STEP };

	private:

		Type const _type;
		Tree_root &_ft;
		Tree_root &_mt;
		Generation const _curr_gen;
		Generation const _free_gen;
		Number_of_blocks const _num_required_pbas;
		Tree_walk_pbas &_new_blocks;
		Type_1_node_walk const &_old_blocks;
		Tree_level_index const _max_lvl;
		Virtual_block_address const _vba;
		Tree_degree const _vbd_degree;
		Virtual_block_address const _vbd_max_vba;
		bool const _rekeying;
		Key_id const _prev_key_id;
		Key_id const _curr_key_id;
		Virtual_block_address const _rekeying_vba;
		bool &_success;
		Snapshots const &_snapshots;
		Generation const _last_secured_gen;
		Physical_block_address &_pba;
		Number_of_blocks &_num_pbas;

		NONCOPYABLE(Free_tree_request);

	public:

		Free_tree_request(Module_id, Module_channel_id, Type, Tree_root &, Tree_root &, Snapshots const &,
		                  Generation, Generation, Generation, Number_of_blocks, Tree_walk_pbas &, Type_1_node_walk const &,
		                  Tree_level_index, Virtual_block_address, Tree_degree, Virtual_block_address,
		                  bool, Key_id, Key_id, Virtual_block_address, Physical_block_address &, Number_of_blocks &, bool &);

		static char const *type_to_string(Type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};


class Tresor::Free_tree_channel : public Module_channel
{
	private:

		using Request = Free_tree_request;

		enum State {
			REQ_SUBMITTED, REQ_GENERATED, SEEK_DOWN, SEEK_LEFT_OR_UP, WRITE_BLK, READ_BLK_SUCCEEDED,
			ALLOC_PBA_SUCCEEDED, WRITE_BLK_SUCCEEDED, REQ_COMPLETE };

		Request *_req_ptr { nullptr };
		State _state { REQ_COMPLETE };
		Virtual_block_address _vba { };
		Tree_walk_pbas _old_pbas { };
		Tree_walk_pbas _new_pbas { };
		Tree_walk_generations _old_generations { };
		Number_of_leaves _num_leaves { 0 };
		Physical_block_address _alloc_pba { 0 };
		Tree_level_index _alloc_lvl { 0 };
		Number_of_blocks _num_pbas { 0 };
		Block _blk { };
		Tree_node_index _node_idx[TREE_MAX_NR_OF_LEVELS] { };
		bool _apply_allocation { false };
		Type_1_node_block _t1_blks[TREE_MAX_NR_OF_LEVELS] { };
		Type_2_node_block _t2_blk { };
		Tree_degree_log_2 _vbd_degree_log_2 { 0 };
		Tree_level_index _lvl { 0 };
		bool _generated_req_success { false };

		NONCOPYABLE(Free_tree_channel);

		void _generated_req_completed(State_uint) override;

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _mark_req_failed(bool &, char const *);

		bool _can_alloc_pba_of(Type_2_node &);

		void _alloc_pba_of(Type_2_node &);

		void _traverse_curr_node(bool &);

		void _mark_req_successful(bool &);

		void _start_tree_traversal(bool &);

		void _advance_to_next_node();

		void _add_new_branch_at(Tree_level_index, Tree_node_index);

		void _add_new_root_lvl();

		void _generate_write_blk_req(bool &);

		void _extension_step(bool &);

		void _alloc_pbas(bool &);

	public:

		Free_tree_channel(Module_channel_id id) : Module_channel { FREE_TREE, id } { }

		void execute(bool &);
};

class Tresor::Free_tree : public Module
{
	private:

		using Channel = Free_tree_channel;
		using Request = Free_tree_request;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Free_tree);

		void execute(bool &) override;

	public:

		struct Extension_step : Request
		{
			Extension_step(Module_id mod_id, Module_channel_id chan_id, Generation curr_gen, Tree_root &ft, Tree_root &mt,
			               Physical_block_address &pba, Number_of_blocks &num_pbas, bool &succ)
			: Request(mod_id, chan_id, Request::EXTENSION_STEP, ft, mt, *(Snapshots *)0, 0, curr_gen, 0, 0, *(Tree_walk_pbas*)0,
			                    *(Type_1_node_walk*)0, 0, 0, 0, 0, 0, 0, 0, 0, pba, num_pbas, succ) { }
		};

		Free_tree();
};

#endif /* _TRESOR__FREE_TREE_H_ */
