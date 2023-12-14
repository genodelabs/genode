/*
 * \brief  Module for accessing and managing the superblocks
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__SUPERBLOCK_CONTROL_H_
#define _TRESOR__SUPERBLOCK_CONTROL_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/virtual_block_device.h>
#include <tresor/trust_anchor.h>
#include <tresor/block_io.h>

namespace Tresor { class Superblock_control; }

class Tresor::Superblock_control : Noncopyable
{
	private:

		class Secure_superblock : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { };

				struct Execute_attr
				{
					Superblock &sb;
					Superblock_index &sb_idx;
					Generation &curr_gen;
					Block_io &block_io;
					Trust_anchor &trust_anchor;
				};

			private:

				enum State {
					INIT, COMPLETE, WRITE_BLOCK, WRITE_BLOCK_SUCCEEDED, SYNC_BLOCK_IO, SYNC_BLOCK_IO_SUCCEEDED,
					ENCRYPT_KEY, ENCRYPT_CURR_KEY_SUCCEEDED, ENCRYPT_PREV_KEY_SUCCEEDED, WRITE_SB_HASH,
					WRITE_SB_HASH_SUCCEEDED };

				using Helper = Request_helper<Secure_superblock, State>;

				Helper _helper;
				Attr const _attr;
				Superblock _sb_ciphertext { };
				Block _blk { };
				Hash _hash { };
				Generation _gen { };
				Generatable_request<Helper, State, Block_io::Write> _write_block { };
				Generatable_request<Helper, State, Block_io::Sync> _sync_block_io { };
				Generatable_request<Helper, State, Trust_anchor::Encrypt_key> _encrypt_key { };
				Generatable_request<Helper, State, Trust_anchor::Write_hash> _write_sb_hash { };

			public:

				Secure_superblock(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Secure_superblock() { }

				void print(Output &out) const { Genode::print(out, "secure sb"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		Superblock _sb { };
		Superblock_index _sb_idx { };
		Generation _curr_gen { };

	public:

		class Write_vbas : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr
				{
					Virtual_block_address const in_first_vba;
					Number_of_blocks const in_num_vbas;
					Request_offset const in_client_req_offset;
					Request_tag const in_client_req_tag;
				};

				struct Execute_attr
				{
					Virtual_block_device &vbd;
					Client_data_interface &client_data;
					Block_io &block_io;
					Free_tree &free_tree;
					Meta_tree &meta_tree;
					Crypto &crypto;
					Superblock &sb;
					Generation const &curr_gen;
				};

			private:

				enum State { INIT, COMPLETE, WRITE_VBA, WRITE_VBA_SUCCEEDED };

				using Helper = Request_helper<Write_vbas, State>;

				Helper _helper;
				Attr const _attr;
				Number_of_blocks _num_written_vbas { };
				Constructible<Tree_root> _ft { };
				Constructible<Tree_root> _mt { };
				Generatable_request<Helper, State, Virtual_block_device::Write_vba> _write_vba { };

				void _start_write_vba(Execute_attr const &, bool &);

			public:

				Write_vbas(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Write_vbas() { }

				void print(Output &out) const { Genode::print(out, "write vba"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Extend_free_tree : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr
				{
					Number_of_blocks const in_num_pbas;
					bool &out_extension_finished;
				};

				struct Execute_attr
				{
					Superblock_control &sb_control;
					Free_tree &free_tree;
					Meta_tree &meta_tree;
					Block_io &block_io;
					Trust_anchor &trust_anchor;
					Superblock &sb;
					Generation const &curr_gen;
				};

			private:

				enum State { INIT, COMPLETE, EXTEND_FREE_TREE, EXTEND_FREE_TREE_SUCCEEDED, SECURE_SB, SECURE_SB_SUCCEEDED };

				using Helper = Request_helper<Extend_free_tree, State>;

				Helper _helper;
				Attr const _attr;
				Number_of_blocks _num_pbas { };
				Physical_block_address _pba { };
				Number_of_blocks _nr_of_leaves { };
				Constructible<Tree_root> _ft { };
				Constructible<Tree_root> _mt { };
				Generatable_request<Helper, State, Secure_superblock> _secure_sb { };
				Generatable_request<Helper, State, Free_tree::Extend_tree> _extend_free_tree { };

			public:

				Extend_free_tree(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Extend_free_tree() { }

				void print(Output &out) const { Genode::print(out, "rekey vba"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Extend_vbd : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr
				{
					Number_of_blocks const in_num_pbas;
					bool &out_extension_finished;
				};

				struct Execute_attr
				{
					Superblock_control &sb_control;
					Virtual_block_device &vbd;
					Free_tree &free_tree;
					Meta_tree &meta_tree;
					Block_io &block_io;
					Trust_anchor &trust_anchor;
					Superblock &sb;
					Generation const &curr_gen;
				};

			private:

				enum State { INIT, COMPLETE, EXTEND_VBD, EXTEND_VBD_SUCCEEDED, SECURE_SB, SECURE_SB_SUCCEEDED };

				using Helper = Request_helper<Extend_vbd, State>;

				Helper _helper;
				Attr const _attr;
				Number_of_blocks _num_pbas { };
				Physical_block_address _pba { };
				Number_of_blocks _nr_of_leaves { };
				Constructible<Tree_root> _ft { };
				Constructible<Tree_root> _mt { };
				Generatable_request<Helper, State, Secure_superblock> _secure_sb { };
				Generatable_request<Helper, State, Virtual_block_device::Extend_tree> _extend_vbd { };

			public:

				Extend_vbd(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Extend_vbd() { }

				void print(Output &out) const { Genode::print(out, "extend vbd"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Rekey : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { bool &out_rekeying_finished; };

				struct Execute_attr
				{
					Superblock &sb;
					Generation const &curr_gen;
					Block_io &block_io;
					Crypto &crypto;
					Trust_anchor &trust_anchor;
					Free_tree &free_tree;
					Meta_tree &meta_tree;
					Virtual_block_device &vbd;
					Superblock_control &sb_control;
				};

			private:

				enum State {
					INIT, COMPLETE, REKEY_VBA, REKEY_VBA_SUCCEEDED, SECURE_SB, SECURE_SB_SUCCEEDED, REMOVE_KEY,
					REMOVE_KEY_SUCCEEDED, GENERATE_KEY, GENERATE_KEY_SUCCEEDED, ADD_KEY, ADD_KEY_SUCCEEDED };

				using Helper = Request_helper<Rekey, State>;

				Helper _helper;
				Attr const _attr;
				Constructible<Tree_root> _ft { };
				Constructible<Tree_root> _mt { };
				Generation _gen { };
				Generatable_request<Helper, State, Virtual_block_device::Rekey_vba> _rekey_vba { };
				Generatable_request<Helper, State, Crypto::Remove_key> _remove_key { };
				Generatable_request<Helper, State, Secure_superblock> _secure_sb { };
				Generatable_request<Helper, State, Crypto::Add_key> _add_key { };
				Generatable_request<Helper, State, Trust_anchor::Generate_key> _generate_key { };

			public:

				Rekey(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Rekey() { }

				void print(Output &out) const { Genode::print(out, "continue rekeying"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Read_vbas : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr
				{
					Virtual_block_address const in_first_vba;
					Number_of_blocks const in_num_vbas;
					Request_offset const in_client_req_offset;
					Request_tag const in_client_req_tag;
				};

				struct Execute_attr
				{
					Virtual_block_device &vbd;
					Client_data_interface &client_data;
					Block_io &block_io;
					Crypto &crypto;
					Superblock const &sb;
					Generation const &curr_gen;
				};

			private:

				enum State { INIT, COMPLETE, READ_VBA, READ_VBA_SUCCEEDED };

				using Helper = Request_helper<Read_vbas, State>;

				Helper _helper;
				Attr const _attr;
				Number_of_blocks _num_read_vbas { };
				Generatable_request<Helper, State, Virtual_block_device::Read_vba> _read_vba { };

				void _start_read_vba(Execute_attr const &, bool &progress);

			public:

				Read_vbas(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Read_vbas() { }

				void print(Output &out) const { Genode::print(out, "read vbas"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Create_snapshot : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { Generation &out_gen; };

				struct Execute_attr
				{
					Superblock &sb;
					Generation &curr_gen;
					Block_io &block_io;
					Trust_anchor &trust_anchor;
					Superblock_control &sb_control;
				};

			private:

				enum State { INIT, COMPLETE, SECURE_SB, SECURE_SB_SUCCEEDED };

				using Helper = Request_helper<Create_snapshot, State>;

				Helper _helper;
				Attr const _attr;
				Generation _gen { };
				Generatable_request<Helper, State, Secure_superblock> _secure_sb { };

			public:

				Create_snapshot(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Create_snapshot() { }

				void print(Output &out) const { Genode::print(out, "create snapshot"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Synchronize : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { };

				struct Execute_attr
				{
					Superblock &sb;
					Generation const &curr_gen;
					Block_io &block_io;
					Trust_anchor &trust_anchor;
					Superblock_control &sb_control;
				};

			private:

				enum State { INIT, COMPLETE, SECURE_SB, SECURE_SB_SUCCEEDED };

				using Helper = Request_helper<Synchronize, State>;

				Helper _helper;
				Attr const _attr;
				Generatable_request<Helper, State, Secure_superblock> _secure_sb { };

			public:

				Synchronize(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Synchronize() { }

				void print(Output &out) const { Genode::print(out, "sync"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Deinitialize : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { };

				struct Execute_attr
				{
					Superblock &sb;
					Generation const &curr_gen;
					Block_io &block_io;
					Crypto &crypto;
					Trust_anchor &trust_anchor;
					Superblock_control &sb_control;
				};

			private:

				enum State {
					INIT, COMPLETE, SECURE_SB, SECURE_SB_SUCCEEDED, REMOVE_KEY,
					REMOVE_CURR_KEY_SUCCEEDED, REMOVE_PREV_KEY_SUCCEEDED };

				using Helper = Request_helper<Deinitialize, State>;

				Helper _helper;
				Attr const _attr;
				Generatable_request<Helper, State, Secure_superblock> _secure_sb { };
				Generatable_request<Helper, State, Crypto::Remove_key> _remove_key { };

			public:

				Deinitialize(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Deinitialize() { }

				void print(Output &out) const { Genode::print(out, "deinitialize"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Initialize : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { Superblock::State &out_sb_state; };

				struct Execute_attr
				{
					Superblock &sb;
					Superblock_index &sb_idx;
					Generation &curr_gen;
					Block_io &block_io;
					Crypto &crypto;
					Trust_anchor &trust_anchor;
					Superblock_control &sb_control;
				};

			private:

				enum State {
					INIT, COMPLETE, READ_SB_HASH, READ_SB_HASH_SUCCEEDED, READ_BLOCK, READ_BLOCK_SUCCEEDED,
					DECRYPT_KEY, DECRYPT_CURR_KEY_SUCCEEDED, DECRYPT_PREV_KEY_SUCCEEDED, ADD_KEY,
					ADD_CURR_KEY_SUCCEEDED, ADD_PREV_KEY_SUCCEEDED };

				using Helper = Request_helper<Initialize, State>;

				Helper _helper;
				Attr const _attr;
				Generation _gen { };
				Hash _hash { };
				Block _blk { };
				Superblock _sb_ciphertext { };
				Generatable_request<Helper, State, Block_io::Read> _read_block { };
				Generatable_request<Helper, State, Trust_anchor::Read_hash> _read_sb_hash { };
				Generatable_request<Helper, State, Trust_anchor::Decrypt_key> _decrypt_key { };
				Generatable_request<Helper, State, Crypto::Add_key> _add_key { };

			public:

				Initialize(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Initialize() { }

				void print(Output &out) const { Genode::print(out, "initialize"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Discard_snapshot : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { Generation const in_gen; };

				struct Execute_attr
				{
					Superblock &sb;
					Generation &curr_gen;
					Block_io &block_io;
					Trust_anchor &trust_anchor;
					Superblock_control &sb_control;
				};

			private:

				enum State { INIT, COMPLETE, SECURE_SB, SECURE_SB_SUCCEEDED };

				using Helper = Request_helper<Discard_snapshot, State>;

				Helper _helper;
				Attr const _attr;
				Generatable_request<Helper, State, Secure_superblock> _secure_sb { };

			public:

				Discard_snapshot(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Discard_snapshot() { }

				void print(Output &out) const { Genode::print(out, "discard snapshot"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		Virtual_block_address max_vba() const { return _sb.valid() ? _sb.max_vba() : 0; };

		Virtual_block_address resizing_nr_of_pbas() const { return _sb.resizing_nr_of_pbas; }

		Virtual_block_address rekeying_vba() const { return _sb.rekeying_vba; }

		Snapshots_info snapshots_info() const;

		Superblock_info sb_info() const;

		bool execute(Extend_free_tree &req, Free_tree &free_tree, Meta_tree &meta_tree, Block_io &block_io, Trust_anchor &trust_anchor)
		{
			return req.execute({ *this, free_tree, meta_tree, block_io, trust_anchor, _sb, _curr_gen });
		}

		bool execute(Extend_vbd &req, Virtual_block_device &vbd, Free_tree &free_tree, Meta_tree &meta_tree, Block_io &block_io, Trust_anchor &trust_anchor)
		{
			return req.execute({ *this, vbd, free_tree, meta_tree, block_io, trust_anchor, _sb, _curr_gen });
		}

		bool execute(Rekey &req, Virtual_block_device &vbd, Free_tree &free_tree, Meta_tree &meta_tree, Block_io &block_io, Crypto &crypto, Trust_anchor &trust_anchor)
		{
			return req.execute({ _sb, _curr_gen, block_io, crypto, trust_anchor, free_tree, meta_tree, vbd, *this });
		}

		bool execute(Secure_superblock &req, Block_io &block_io, Trust_anchor &trust_anchor)
		{
			return req.execute({_sb, _sb_idx, _curr_gen, block_io, trust_anchor });
		}

		bool execute(Synchronize &req, Block_io &block_io, Trust_anchor &trust_anchor)
		{
			return req.execute({_sb, _curr_gen, block_io, trust_anchor, *this });
		}

		bool execute(Deinitialize &req, Block_io &block_io, Crypto &crypto, Trust_anchor &trust_anchor)
		{
			return req.execute({_sb, _curr_gen, block_io, crypto, trust_anchor, *this });
		}

		bool execute(Initialize &req, Block_io &block_io, Crypto &crypto, Trust_anchor &trust_anchor)
		{
			return req.execute({_sb, _sb_idx, _curr_gen, block_io, crypto, trust_anchor, *this });
		}

		bool execute(Create_snapshot &req, Block_io &block_io, Trust_anchor &trust_anchor)
		{
			return req.execute({_sb, _curr_gen, block_io, trust_anchor, *this });
		}

		bool execute(Discard_snapshot &req, Block_io &block_io, Trust_anchor &trust_anchor)
		{
			return req.execute({_sb, _curr_gen, block_io, trust_anchor, *this });
		}

		bool execute(Read_vbas &req, Virtual_block_device &vbd, Client_data_interface &client_data, Block_io &block_io, Crypto &crypto)
		{
			return req.execute({ vbd, client_data, block_io, crypto, _sb, _curr_gen });
		}

		bool execute(Write_vbas &req, Virtual_block_device &vbd, Client_data_interface &client_data, Block_io &block_io, Free_tree &free_tree, Meta_tree &meta_tree, Crypto &crypto)
		{
			return req.execute({ vbd, client_data, block_io, free_tree, meta_tree, crypto, _sb, _curr_gen });
		}

		static constexpr char const *name() { return "sb_control"; }
};

#endif /* _TRESOR__SUPERBLOCK_CONTROL_H_ */
