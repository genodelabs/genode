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
#include <tresor/block_io.h>
#include <tresor/meta_tree.h>

namespace Tresor { class Free_tree; }

class Tresor::Free_tree : Noncopyable
{
	public:

		class Allocate_pbas;
		class Extend_tree;

		template <typename REQUEST>
		bool execute(REQUEST &req, Block_io &block_io, Meta_tree &meta_tree) { return req.execute(block_io, meta_tree); }

		static constexpr char const *name() { return "free_tree"; }
};

class Tresor::Free_tree::Allocate_pbas : Noncopyable
{
	public:

		using Module = Free_tree;

		enum Application { NON_REKEYING, REKEYING_IN_CURRENT_GENERATION, REKEYING_IN_OLDER_GENERATION };

		struct Attr
		{
			Tree_root &in_out_ft;
			Tree_root &in_out_mt;
			Snapshots const &in_snapshots;
			Generation const in_last_secured_gen;
			Generation const in_curr_gen;
			Generation const in_free_gen;
			Number_of_blocks const in_num_required_pbas;
			Tree_walk_pbas &in_out_new_blocks;
			Type_1_node_walk const &in_old_blocks;
			Tree_level_index const in_max_lvl;
			Virtual_block_address const in_vba;
			Tree_degree const in_vbd_degree;
			Virtual_block_address const in_vbd_max_vba;
			bool const in_rekeying;
			Key_id const in_prev_key_id;
			Key_id const in_curr_key_id;
			Virtual_block_address const in_rekeying_vba;
			Application const in_application;
		};

	private:

		enum State {
			INIT, COMPLETE, SEEK_DOWN, SEEK_LEFT_OR_UP, READ_BLK, READ_BLK_SUCCEEDED,
			ALLOC_PBA, ALLOC_PBA_SUCCEEDED, WRITE_BLK, WRITE_BLK_SUCCEEDED };

		using Helper = Request_helper<Allocate_pbas, State>;

		Helper _helper;
		Attr const _attr;
		Virtual_block_address _vba { };
		Tree_walk_pbas _old_pbas { };
		Tree_walk_pbas _new_pbas { };
		Tree_walk_generations _old_generations { };
		Number_of_blocks _num_pbas { 0 };
		Block _blk { };
		Tree_node_index _node_idx[TREE_MAX_NR_OF_LEVELS] { };
		bool _apply_allocation { false };
		Type_1_node_block _t1_blks[TREE_MAX_NR_OF_LEVELS] { };
		Type_2_node_block _t2_blk { };
		Tree_degree_log_2 _vbd_degree_log_2 { 0 };
		Tree_level_index _lvl { 0 };
		Generatable_request<Helper, State, Block_io::Read> _read_block { };
		Generatable_request<Helper, State, Block_io::Write> _write_block { };
		Generatable_request<Helper, State, Meta_tree::Allocate_pba> _allocate_pba { };

		void _alloc_pba_of(Type_2_node &);

		bool _can_alloc_pba_of(Type_2_node &);

		void _traverse_curr_node(bool &);

		void _start_tree_traversal(bool &);

	public:

		Allocate_pbas(Attr const &attr) : _helper(*this), _attr(attr) { }

		~Allocate_pbas() { }

		void print(Output &out) const { Genode::print(out, "allocate pbas"); }

		bool execute(Block_io &, Meta_tree &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Free_tree::Extend_tree : Noncopyable
{
	public:

		using Module = Free_tree;

		struct Attr
		{
			Generation const in_curr_gen;
			Tree_root &in_out_ft;
			Tree_root &in_out_mt;
			Physical_block_address &in_out_first_pba;
			Number_of_blocks &in_out_num_pbas;
		};

	private:

		enum State {
			INIT, COMPLETE, READ_BLK, READ_BLK_SUCCEEDED, ALLOC_PBA, ALLOC_PBA_SUCCEEDED, WRITE_BLK,
			WRITE_BLK_SUCCEEDED };

		using Helper = Request_helper<Extend_tree, State>;

		Helper _helper;
		Attr const _attr;
		Number_of_leaves _num_leaves { 0 };
		Virtual_block_address _vba { };
		Tree_walk_pbas _old_pbas { };
		Tree_walk_generations _old_generations { };
		Tree_walk_pbas _new_pbas { };
		Tree_level_index _lvl { 0 };
		Block _blk { };
		Type_1_node_block _t1_blks[TREE_MAX_NR_OF_LEVELS] { };
		Type_2_node_block _t2_blk { };
		Tree_level_index _alloc_lvl { 0 };
		Physical_block_address _alloc_pba { 0 };
		Generatable_request<Helper, State, Block_io::Read> _read_block { };
		Generatable_request<Helper, State, Block_io::Write> _write_block { };
		Generatable_request<Helper, State, Meta_tree::Allocate_pba> _allocate_pba { };

		void _add_new_branch_at(Tree_level_index, Tree_node_index);

		void _add_new_root_lvl();

		void _generate_write_blk_req(bool &);

	public:

		Extend_tree(Attr const &attr) : _helper(*this), _attr(attr) { }

		~Extend_tree() { }

		void print(Output &out) const { Genode::print(out, "extend tree"); }

		bool execute(Block_io &, Meta_tree &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

#endif /* _TRESOR__FREE_TREE_H_ */
