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
#include <tresor/block_io.h>

namespace Tresor { class Ft_initializer; }

class Tresor::Ft_initializer : Noncopyable
{
	public:

		class Initialize : Noncopyable
		{
			public:

				using Module = Ft_initializer;

				struct Attr
				{
					Tree_configuration const in_tree_cfg;
					Type_1_node &out_tree_root;
					Pba_allocator &in_out_pba_alloc;
				};

			private:

				enum Node_state { DONE, INIT_BLOCK, INIT_NODE, WRITING_BLOCK };

				enum State { INIT, COMPLETE, EXECUTE_NODES, WRITE_BLOCK };

				using Helper = Request_helper<Initialize, State>;

				Helper _helper;
				Attr const _attr;
				Type_2_node_block _t2_blk { };
				Type_1_node_block_walk _t1_blks { };
				Node_state _t1_node_states[TREE_MAX_NR_OF_LEVELS][NUM_NODES_PER_BLK] { };
				Node_state _t2_node_states[NUM_NODES_PER_BLK] { };
				Number_of_leaves _num_remaining_leaves { 0 };
				Block _blk { };
				Generatable_request<Helper, State, Block_io::Write> _write_block { };

				void _reset_level(Tree_level_index, Node_state);

				bool _execute_t2_node(Tree_node_index, bool &);

				bool _execute_t1_node(Tree_level_index, Tree_node_index, bool &);

			public:

				Initialize(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Initialize() { }

				void print(Output &out) const { Genode::print(out, "initialize"); }

				bool execute(Block_io &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		bool execute(Initialize &req, Block_io &block_io) { return req.execute(block_io); }

		static constexpr char const *name() { return "ft_initializer"; }
};

#endif /* _TRESOR__FT_INITIALIZER_H_ */
