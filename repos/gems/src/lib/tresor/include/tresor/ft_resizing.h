/*
 * \brief  Module for file tree resizing
 * \author Martin Stein
 * \date   2023-03-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__FT_RESIZING_H_
#define _TRESOR__FT_RESIZING_H_

/* tresor includes */
#include <tresor/module.h>
#include <tresor/types.h>
#include <tresor/vfs_utilities.h>

namespace Tresor {

	class Ft_resizing;
	class Ft_resizing_request;
	class Ft_resizing_channel;
}


class Tresor::Ft_resizing_request : public Module_request
{
	public:

		enum Type { INVALID = 0, FT_EXTENSION_STEP = 1 };

	private:

		friend class Ft_resizing;
		friend class Ft_resizing_channel;

		Type                   _type             { INVALID };
		Generation             _curr_gen         { INVALID_GENERATION };
		Type_1_node            _ft_root          { };
		Tree_level_index       _ft_max_lvl       { 0 };
		Number_of_leaves       _ft_nr_of_leaves  { 0 };
		Tree_degree            _ft_degree        { TREE_MIN_DEGREE };
		addr_t                 _mt_root_pba_ptr  { 0 };
		addr_t                 _mt_root_gen_ptr  { 0 };
		addr_t                 _mt_root_hash_ptr { 0 };
		Tree_level_index       _mt_max_level     { 0 };
		Tree_degree            _mt_degree        { 0 };
		Number_of_leaves       _mt_leaves        { 0 };
		Physical_block_address _pba              { 0 };
		Number_of_blocks       _nr_of_pbas       { 0 };
		Number_of_leaves       _nr_of_leaves     { 0 };
		bool                   _success          { false };

	public:

		Ft_resizing_request() { }

		Ft_resizing_request(uint64_t               src_module_id,
		                    uint64_t               src_request_id,
		                    Type                   type,
		                    Generation             curr_gen,
		                    Type_1_node            ft_root,
		                    Tree_level_index       ft_max_lvl,
		                    Number_of_leaves       ft_nr_of_leaves,
		                    Tree_degree            ft_degree,
		                    addr_t                 mt_root_pba_ptr,
		                    addr_t                 mt_root_gen_ptr,
		                    addr_t                 mt_root_hash_ptr,
		                    Tree_level_index       mt_max_level,
		                    Tree_degree            mt_degree,
		                    Number_of_leaves       mt_leaves,
		                    Physical_block_address pba,
		                    Number_of_blocks       nr_of_pbas);

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);

		Type_1_node ft_root() const { return _ft_root; }
		Tree_level_index ft_max_lvl() const { return _ft_max_lvl; }
		Number_of_leaves ft_nr_of_leaves() const { return _ft_nr_of_leaves; }
		Number_of_leaves nr_of_leaves() const { return _nr_of_leaves; }
		Physical_block_address pba() const { return _pba; }
		Number_of_blocks nr_of_pbas() const { return _nr_of_pbas; }


		/********************
		 ** Module_request **
		 ********************/

		void print(Output &out) const override
		{
			Genode::print(out, type_to_string(_type), " root ", _ft_root, " leaves ", _ft_nr_of_leaves, " max_lvl ", _ft_max_lvl);
		}
};

class Tresor::Ft_resizing_channel
{
	private:

		friend class Ft_resizing;

		enum State {
			SUBMITTED,

			READ_ROOT_NODE_PENDING,
			READ_ROOT_NODE_IN_PROGRESS,
			READ_ROOT_NODE_COMPLETED,

			READ_INNER_NODE_PENDING,
			READ_INNER_NODE_IN_PROGRESS,
			READ_INNER_NODE_COMPLETED,

			ALLOC_PBA_PENDING,
			ALLOC_PBA_IN_PROGRESS,
			ALLOC_PBA_COMPLETED,

			EXTEND_MT_BY_ONE_LEAF_PENDING,
			EXTEND_MT_BY_ONE_LEAF_IN_PROGRESS,
			EXTEND_MT_BY_ONE_LEAF_COMPLETED,

			WRITE_INNER_NODE_PENDING,
			WRITE_INNER_NODE_IN_PROGRESS,
			WRITE_INNER_NODE_COMPLETED,

			WRITE_ROOT_NODE_PENDING,
			WRITE_ROOT_NODE_IN_PROGRESS,
			WRITE_ROOT_NODE_COMPLETED,

			COMPLETED
		};

		enum Tag_type
		{
			TAG_INVALID,
			TAG_FT_RSZG_CACHE,
			TAG_FT_RSZG_MT_ALLOC,
		};

		struct Generated_prim
		{
			enum Type { READ, WRITE };

			Type     op     { READ };
			bool     succ   { false };
			Tag_type tg     { TAG_INVALID };
			uint64_t blk_nr { 0 };
			uint64_t idx    { 0 };
		};

		struct Type_1_node_blocks
		{
			Type_1_node_block items[TREE_MAX_LEVEL] { };
		};

		struct Generations
		{
			Generation items[TREE_MAX_LEVEL + 1] { };
		};

		Ft_resizing_request   _request         { };
		State                 _state           { SUBMITTED };
		Generated_prim        _generated_prim  { };
		Type_1_node_blocks    _t1_blks         { };
		Type_2_node_block     _t2_blk          { };
		Tree_level_index      _lvl_idx         { 0 };
		Tree_level_index      _alloc_lvl_idx   { 0 };
		Virtual_block_address _vba             { };
		Tree_walk_pbas        _old_pbas        { };
		Generations           _old_generations { };
		Tree_walk_pbas        _new_pbas        { };
		Block                 _encoded_blk     { };
};

class Tresor::Ft_resizing : public Module
{
	private:

		using Request = Ft_resizing_request;
		using Channel = Ft_resizing_channel;

		enum { NR_OF_CHANNELS = 1 };

		Channel _channels[NR_OF_CHANNELS] { };

		void _set_args_for_write_back_of_inner_lvl(Tree_level_index const,
		                                           Tree_level_index const,
		                                           Physical_block_address const,
		                                           unsigned const prim_idx,
		                                           Channel::State &,
		                                           bool &progress,
		                                           Channel::Generated_prim &);

		void _add_new_root_lvl_to_ft_using_pba_contingent(Type_1_node &,
		                                                  Tree_level_index &,
		                                                  Number_of_leaves const,
		                                                  Generation const,
		                                                  Channel::Type_1_node_blocks &,
		                                                  Tree_walk_pbas &,
		                                                  Physical_block_address &,
		                                                  Number_of_blocks &);

		void _add_new_branch_to_ft_using_pba_contingent(Tree_level_index const,
		                                                Tree_node_index const,
		                                                Tree_degree const,
		                                                Generation const,
		                                                Physical_block_address &,
		                                                Number_of_blocks &,
		                                                Channel::Type_1_node_blocks &,
		                                                Type_2_node_block &,
		                                                Tree_walk_pbas &,
		                                                Tree_level_index &,
		                                                Number_of_leaves &);

		void _execute_ft_extension_step(Channel &, unsigned const idx, bool &);

		void _execute_ft_ext_step_read_inner_node_completed(Channel &,
		                                                    unsigned const job_idx,
		                                                    bool &progress);

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

		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		void execute(bool &) override;
};

#endif /* _TRESOR__FT_RESIZING_H_ */
