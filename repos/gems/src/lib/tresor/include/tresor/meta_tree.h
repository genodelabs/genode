/*
 * \brief  Module for doing PBA allocations for the Free Tree via the Meta Tree
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__META_TREE_H_
#define _TRESOR__META_TREE_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/block_io.h>

namespace Tresor { class Meta_tree; }

class Tresor::Meta_tree : Noncopyable
{
	public:

		class Allocate_pba;

		bool execute(Allocate_pba &, Block_io &);

		static constexpr char const *name() { return "meta_tree"; }
};

class Tresor::Meta_tree::Allocate_pba : Noncopyable
{
	public:

		using Module = Meta_tree;

		struct Attr
		{
			Tree_root &in_out_mt;
			Generation const in_curr_gen;
			Physical_block_address &in_out_pba;
		};

	private:

		enum State { INIT, COMPLETE, READ_BLK, SEEK_DOWN, SEEK_LEFT_OR_UP, WRITE_BLK, WRITE_BLK_SUCCEEDED };

		using Helper = Request_helper<Allocate_pba, State>;

		Helper _helper;
		Attr const _attr;
		Block _blk { };
		Tree_node_index _node_idx[TREE_MAX_NR_OF_LEVELS] { };
		Type_1_node_block _t1_blks[TREE_MAX_NR_OF_LEVELS] { };
		Type_2_node_block _t2_blk { };
		Tree_level_index _lvl { 0 };
		Generatable_request<Helper, State, Block_io::Read> _read_block { };
		Generatable_request<Helper, State, Block_io::Write> _write_block { };

		bool _can_alloc_pba_of(Type_2_node &);

		void _alloc_pba_of(Type_2_node &, Physical_block_address &);

		void _traverse_curr_node(bool &);

		void _start_tree_traversal(bool &);

	public:

		Allocate_pba(Attr const &attr) : _helper(*this), _attr(attr) { }

		~Allocate_pba() { }

		void print(Output &out) const { Genode::print(out, "allocate pba"); }

		bool execute(Block_io &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

#endif /* _TRESOR__META_TREE_H_ */
