/*
 * \brief  Module for accessing and managing trees of the virtual block device
 * \author Martin Stein
 * \date   2023-03-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__VIRTUAL_BLOCK_DEVICE_H_
#define _TRESOR__VIRTUAL_BLOCK_DEVICE_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/free_tree.h>
#include <tresor/block_io.h>
#include <tresor/client_data_interface.h>
#include <tresor/crypto.h>

namespace Tresor { class Virtual_block_device; }

class Tresor::Virtual_block_device : Noncopyable
{
	public:

		class Rekey_vba;
		class Read_vba;
		class Write_vba;
		class Extend_tree;

		template <typename REQUEST, typename... ARGS>
		bool execute(REQUEST &req, ARGS &&... args) { return req.execute(args...); }

		static constexpr char const *name() { return "vbd"; }
};

class Tresor::Virtual_block_device::Rekey_vba : Noncopyable
{
	public:

		using Module = Virtual_block_device;

		struct Attr
		{
			Snapshots &in_out_snapshots;
			Tree_root &in_out_ft;
			Tree_root &in_out_mt;
			Virtual_block_address const in_vba;
			Generation const in_curr_gen;
			Generation const in_last_secured_gen;
			Key_id const in_curr_key_id;
			Key_id const in_prev_key_id;
			Tree_degree const in_vbd_degree;
			Virtual_block_address const in_vbd_highest_vba;
		};

	private:

		enum State {
			INIT, COMPLETE, READ_BLK, READ_BLK_SUCCEEDED, WRITE_BLK, WRITE_BLK_SUCCEEDED,
			DECRYPT_BLOCK, DECRYPT_BLOCK_SUCCEEDED, ENCRYPT_BLOCK, ENCRYPT_BLOCK_SUCCEEDED,
			ALLOC_PBAS, ALLOC_PBAS_SUCCEEDED };

		using Helper = Request_helper<Rekey_vba, State>;

		Helper _helper;
		Attr const _attr;
		Tree_level_index _lvl { 0 };
		Type_1_node_block_walk _t1_blks { };
		Block _encoded_blk { };
		Block _data_blk { };
		Generation _free_gen { 0 };
		Tree_walk_pbas _old_pbas { };
		Tree_walk_pbas _new_pbas { };
		Snapshot_index _snap_idx { 0 };
		Type_1_node_walk _t1_nodes { };
		Number_of_blocks _num_blks { 0 };
		Hash _hash { };
		bool _first_snapshot { false };
		Generatable_request<Helper, State, Block_io::Read> _read_block { };
		Generatable_request<Helper, State, Block_io::Write> _write_block { };
		Generatable_request<Helper, State, Crypto::Encrypt> _encrypt_block { };
		Generatable_request<Helper, State, Crypto::Decrypt> _decrypt_block { };
		Generatable_request<Helper, State, Free_tree::Allocate_pbas> _alloc_pbas { };

		bool _check_and_decode_read_blk(bool &);

		void _start_alloc_pbas(bool &, Free_tree::Allocate_pbas::Application);

		void _generate_write_blk_req(bool &);

		bool _find_next_snap_to_rekey_vba_at(Snapshot_index &) const;

		void _generate_ft_alloc_req_for_rekeying(Tree_level_index, bool &);

	public:

		Rekey_vba(Attr const &attr) : _helper(*this), _attr(attr) { }

		~Rekey_vba() { }

		void print(Output &out) const { Genode::print(out, "rekey vba"); }

		bool execute(Block_io &, Crypto &, Free_tree &, Meta_tree &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Virtual_block_device::Read_vba : Noncopyable
{
	public:

		using Module = Virtual_block_device;

		struct Attr
		{
			Snapshot const &in_snap;
			Virtual_block_address const in_vba;
			Key_id const in_key_id;
			Tree_degree const in_vbd_degree;
			Request_offset const in_client_req_offset;
			Request_tag const in_client_req_tag;
		};

	private:

		enum State { INIT, COMPLETE, READ_BLK, READ_BLK_SUCCEEDED, DECRYPT_BLOCK, DECRYPT_BLOCK_SUCCEEDED };

		using Helper = Request_helper<Read_vba, State>;

		Helper _helper;
		Attr const _attr;
		Tree_level_index _lvl { 0 };
		Type_1_node_block_walk _t1_blks { };
		Hash _hash { };
		Block _blk { };
		Tree_walk_pbas _new_pbas { };
		Generatable_request<Helper, State, Block_io::Read> _read_block { };
		Generatable_request<Helper, State, Crypto::Decrypt> _decrypt_block { };

		bool _check_and_decode_read_blk(bool &);

	public:

		Read_vba(Attr const &attr) : _helper(*this), _attr(attr) { }

		~Read_vba() { }

		void print(Output &out) const { Genode::print(out, "read vba"); }

		bool execute(Client_data_interface &, Block_io &, Crypto &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Virtual_block_device::Write_vba : Noncopyable
{
	public:

		using Module = Virtual_block_device;

		struct Attr
		{
			Snapshot &in_out_snap;
			Snapshots const &in_snapshots;
			Tree_root &in_out_ft;
			Tree_root &in_out_mt;
			Virtual_block_address const in_vba;
			Key_id const in_curr_key_id;
			Key_id const in_prev_key_id;
			Tree_degree const in_vbd_degree;
			Virtual_block_address const in_vbd_highest_vba;
			Request_offset const in_client_req_offset;
			Request_tag const in_client_req_tag;
			Generation const in_curr_gen;
			Generation const in_last_secured_gen;
			bool in_rekeying;
			Virtual_block_address const in_rekeying_vba;
		};

	private:

		enum State {
			INIT, COMPLETE, READ_BLK, READ_BLK_SUCCEEDED, DECRYPT_BLOCK, DECRYPT_BLOCK_SUCCEEDED,
			WRITE_BLK, WRITE_BLK_SUCCEEDED, ENCRYPT_BLOCK, ENCRYPT_BLOCK_SUCCEEDED, ALLOC_PBAS, ALLOC_PBAS_SUCCEEDED };

		using Helper = Request_helper<Write_vba, State>;

		Helper _helper;
		Attr const _attr;
		Tree_level_index _lvl { 0 };
		Type_1_node_block_walk _t1_blks { };
		Hash _hash { };
		Type_1_node_walk _t1_nodes { };
		Block _data_blk { };
		Block _encoded_blk { };
		Tree_walk_pbas _new_pbas { };
		Number_of_blocks _num_blks { 0 };
		Generation _free_gen { 0 };
		Generatable_request<Helper, State, Block_io::Read> _read_block { };
		Generatable_request<Helper, State, Crypto::Decrypt> _decrypt_block { };
		Generatable_request<Helper, State, Crypto::Encrypt> _encrypt_block { };
		Generatable_request<Helper, State, Free_tree::Allocate_pbas> _alloc_pbas { };
		Generatable_request<Helper, State, Block_io::Write> _write_block { };

		bool _check_and_decode_read_blk(bool &);

		void _set_new_pbas_and_num_blks_for_alloc();

		void _update_nodes_of_branch_of_written_vba();

		void _generate_ft_alloc_req_for_write_vba(bool &);

		void _generate_write_blk_req(bool &);

	public:

		Write_vba(Attr const &attr) : _helper(*this), _attr(attr) { }

		~Write_vba() { }

		void print(Output &out) const { Genode::print(out, "write vba"); }

		bool execute(Client_data_interface &, Block_io &, Free_tree &, Meta_tree &, Crypto &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Virtual_block_device::Extend_tree : Noncopyable
{
	public:

		using Module = Virtual_block_device;

		struct Attr
		{
			Number_of_leaves &out_num_leaves;
			Snapshots &in_out_snapshots;
			Tree_degree const in_snap_degr;
			Generation const in_curr_gen;
			Generation const in_last_secured_gen;
			Physical_block_address &in_out_first_pba;
			Number_of_blocks &in_out_num_pbas;
			Tree_root &in_out_ft;
			Tree_root &in_out_mt;
			Tree_degree const in_vbd_degree;
			Virtual_block_address const in_vbd_highest_vba;
			Key_id const in_curr_key_id;
			Key_id const in_prev_key_id;
			bool in_rekeying;
			Virtual_block_address const in_rekeying_vba;
		};

	private:

		enum State {
			INIT, COMPLETE, READ_BLK, READ_BLK_SUCCEEDED, WRITE_BLK, WRITE_BLK_SUCCEEDED, ALLOC_PBAS, ALLOC_PBAS_SUCCEEDED };

		using Helper = Request_helper<Extend_tree, State>;

		Helper _helper;
		Attr const _attr;
		Tree_level_index _lvl { 0 };
		Snapshot_index _snap_idx { 0 };
		Virtual_block_address _vba { 0 };
		Tree_walk_pbas _old_pbas { };
		Block _encoded_blk { };
		Tree_walk_pbas _new_pbas { };
		Type_1_node_block_walk _t1_blks { };
		Number_of_blocks _num_blks { 0 };
		Type_1_node_walk _t1_nodes { };
		Block _data_blk { };
		Generation _free_gen { 0 };
		Generatable_request<Helper, State, Block_io::Read> _read_block { };
		Generatable_request<Helper, State, Block_io::Write> _write_block { };
		Generatable_request<Helper, State, Free_tree::Allocate_pbas> _alloc_pbas { };

		void _add_new_root_lvl_to_snap();

		void _add_new_branch_to_snap(Tree_level_index, Tree_node_index);

		void _set_new_pbas_identical_to_curr_pbas();

		void _generate_write_blk_req(bool &);

		void _generate_ft_alloc_req_for_resizing(Tree_level_index, bool &);

	public:

		Extend_tree(Attr const &attr) : _helper(*this), _attr(attr) { }

		~Extend_tree() { }

		void print(Output &out) const { Genode::print(out, "extend tree"); }

		bool execute(Block_io &, Free_tree &, Meta_tree &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

#endif /* _TRESOR__VIRTUAL_BLOCK_DEVICE_H_ */
