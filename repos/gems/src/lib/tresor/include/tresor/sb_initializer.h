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
#include <tresor/block_io.h>
#include <tresor/vbd_initializer.h>
#include <tresor/ft_initializer.h>
#include <tresor/trust_anchor.h>

namespace Tresor { class Sb_initializer; }

class Tresor::Sb_initializer : Noncopyable
{
	public:

		class Initialize : Noncopyable
		{
			public:

				using Module = Sb_initializer;

				struct Attr
				{
					Tree_configuration const in_vbd_cfg;
					Tree_configuration const in_ft_cfg;
					Tree_configuration const in_mt_cfg;
					Pba_allocator &in_out_pba_alloc;
				};

			private:

				enum State {
					INIT, COMPLETE, START_NEXT_SB, SB_COMPLETE, INIT_FT, INIT_FT_SUCCEEDED, INIT_MT_SUCCEEDED,
					WRITE_HASH_TO_TA, GENERATE_KEY, GENERATE_KEY_SUCCEEDED, ENCRYPT_KEY, ENCRYPT_KEY_SUCCEEDED,
					SECURE_SB_SUCCEEDED, INIT_VBD, INIT_VBD_SUCCEEDED, WRITE_BLK, WRITE_BLK_SUCCEEDED,
					SYNC_BLOCK_IO, WRITE_SB_HASH };

				using Helper = Request_helper<Initialize, State>;

				Helper _helper;
				Attr const _attr;
				Superblock_index _sb_idx { 0 };
				Superblock _sb { };
				Block _blk { };
				Hash _hash { };
				Type_1_node _vbd_root { };
				Type_1_node _ft_root { };
				Type_1_node _mt_root { };
				Generatable_request<Helper, State, Block_io::Write> _write_block { };
				Generatable_request<Helper, State, Block_io::Sync> _sync_block_io { };
				Generatable_request<Helper, State, Trust_anchor::Generate_key> _generate_key { };
				Generatable_request<Helper, State, Trust_anchor::Write_hash> _write_sb_hash { };
				Generatable_request<Helper, State, Trust_anchor::Encrypt_key> _encrypt_key { };
				Generatable_request<Helper, State, Ft_initializer::Initialize> _init_ft { };
				Generatable_request<Helper, State, Vbd_initializer::Initialize> _init_vbd { };

			public:

				Initialize(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Initialize() { }

				void print(Output &out) const { Genode::print(out, "initialize"); }

				bool execute(Block_io &, Trust_anchor &, Vbd_initializer &, Ft_initializer &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		bool execute(Initialize &req, Block_io &block_io, Trust_anchor &trust_anchor, Vbd_initializer &vbd_initializer, Ft_initializer &ft_initializer)
		{
			return req.execute(block_io, trust_anchor, vbd_initializer, ft_initializer);
		}

		static constexpr char const *name() { return "sb_initializer"; }
};

#endif /* _TRESOR__SB_INITIALIZER_H_ */
