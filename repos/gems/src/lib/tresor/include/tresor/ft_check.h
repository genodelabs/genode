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
#include <tresor/block_io.h>

namespace Tresor { class Ft_check; }

struct Tresor::Ft_check : Noncopyable
{
	class Check : Noncopyable
	{
		public:

			using Module = Ft_check;

			struct Attr { Tree_root const &in_ft; };

		private:

			enum State { INIT, IN_PROGRESS, COMPLETE, READ_BLK, READ_BLK_SUCCEEDED };

			using Helper = Request_helper<Check, State>;

			Helper _helper;
			Attr const _attr;
			Type_1_node_block_walk _t1_blks { };
			Type_2_node_block _t2_blk { };
			bool _check_node[TREE_MAX_NR_OF_LEVELS + 1][NUM_NODES_PER_BLK] { };
			Number_of_leaves _num_remaining_leaves { 0 };
			Block _blk { };
			Generatable_request<Helper, State, Block_io::Read> _read_block { };

			bool _execute_node(Block_io &, Tree_level_index, Tree_node_index, bool &);

		public:

			Check(Attr const &attr) : _helper(*this), _attr(attr) { }

			void print(Output &out) const { Genode::print(out, "check ", _attr.in_ft); }

			bool execute(Block_io &);

			bool complete() const { return _helper.complete(); }
			bool success() const { return _helper.success(); }
	};

	Ft_check() { }

	bool execute(Check &req, Block_io &block_io) { return req.execute(block_io); }

	static constexpr char const *name() { return "ft_check"; }
};

#endif /* _TRESOR__FT_CHECK_H_ */
