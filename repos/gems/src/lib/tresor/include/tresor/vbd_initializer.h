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
#include <tresor/block_io.h>

namespace Tresor { class Vbd_initializer; }

class Tresor::Vbd_initializer : Noncopyable
{
	public:

		class Initialize : Noncopyable
		{
			public:

				using Module = Vbd_initializer;

				struct Attr
				{
					Tree_configuration const in_tree_cfg;
					Type_1_node &out_tree_root;
					Pba_allocator &in_out_pba_alloc;
				};

			private:

				enum State { INIT, COMPLETE, WRITE_BLOCK, EXECUTE_NODES };

				enum Node_state { DONE, INIT_BLOCK, INIT_NODE, WRITING_BLOCK };

				using Helper = Request_helper<Initialize, State>;

				Helper _helper;
				Attr const _attr;
				Type_1_node_block_walk _t1_blks { };
				Node_state _node_states[TREE_MAX_NR_OF_LEVELS + 1][NUM_NODES_PER_BLK] { DONE };
				bool _generated_req_success { false };
				Block _blk { };
				Number_of_leaves _num_remaining_leaves { };
				Generatable_request<Helper, State, Block_io::Write> _write_block { };

				void _reset_level(Tree_level_index, Node_state);

				bool _execute_node(Tree_level_index, Tree_node_index, bool &);

			public:

				Initialize(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Initialize() { }

				void print(Output &out) const { Genode::print(out, "initialize"); }

				bool execute(Block_io &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		bool execute(Initialize &req, Block_io &block_io) { return req.execute(block_io); }

		static constexpr char const *name() { return "vbd_initializer"; }
};

#endif /* _TRESOR__VBD_INITIALIZER_H_ */
