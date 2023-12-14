/*
 * \brief  Module for checking all hashes of a VBD snapshot
 * \author Martin Stein
 * \date   2023-05-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__VBD_CHECK_H_
#define _TRESOR__VBD_CHECK_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/block_io.h>

namespace Tresor { class Vbd_check; }

struct Tresor::Vbd_check : Noncopyable
{
	class Check : Noncopyable
	{
		public:

			using Module = Vbd_check;

			struct Attr { Tree_root const &in_vbd; };

		private:

			enum State { INIT, IN_PROGRESS, COMPLETE, READ_BLK, READ_BLK_SUCCEEDED };

			Request_helper<Check, State> _helper;
			Attr const _attr;
			Type_1_node_block_walk _t1_blks { };
			bool _check_node[TREE_MAX_NR_OF_LEVELS][NUM_NODES_PER_BLK] { };
			Block _blk { };
			Number_of_leaves _num_remaining_leaves { 0 };
			Generatable_request<Request_helper<Check, State>, State, Block_io::Read> _read_block { };

			bool _execute_node(Block_io &, Tree_level_index, Tree_node_index, bool &);

		public:

			Check(Attr const &attr) : _helper(*this), _attr(attr) { }

			void print(Output &out) const { Genode::print(out, "check ", _attr.in_vbd); }

			bool execute(Block_io &);

			bool complete() const { return _helper.complete(); }
			bool success() const { return _helper.success(); }
	};

	Vbd_check() { }

	bool execute(Check &req, Block_io &block_io) { return req.execute(block_io); }

	static constexpr char const *name() { return "vbd_check"; }
};

#endif /* _TRESOR__VBD_CHECK_H_ */
