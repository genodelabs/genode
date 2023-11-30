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

namespace Tresor {

	class Virtual_block_device;
	class Virtual_block_device_request;
	class Virtual_block_device_channel;
}

class Tresor::Virtual_block_device_request : public Module_request
{
	friend class Virtual_block_device_channel;

	public:

		enum Type { REKEY_VBA, READ_VBA, WRITE_VBA, EXTENSION_STEP };

	private:

		Type const _type;
		Virtual_block_address const _vba;
		Snapshots &_snapshots;
		Snapshot_index const _curr_snap_idx;
		Tree_degree const _snap_degr;
		Generation const _curr_gen;
		Key_id const _curr_key_id;
		Key_id const _prev_key_id;
		Tree_root &_ft;
		Tree_root &_mt;
		Tree_degree const _vbd_degree;
		Virtual_block_address const _vbd_highest_vba;
		bool const _rekeying;
		Request_offset const _client_req_offset;
		Request_tag const _client_req_tag;
		Generation const _last_secured_gen;
		Physical_block_address &_pba;
		Number_of_blocks &_num_pbas;
		Number_of_leaves &_num_leaves;
		bool &_success;

		NONCOPYABLE(Virtual_block_device_request);

	public:

		Virtual_block_device_request(Module_id, Module_channel_id, Type, Request_offset, Request_tag, Generation,
		                             Tree_root &, Tree_root &, Tree_degree, Virtual_block_address, bool,
		                             Virtual_block_address, Snapshot_index, Snapshots &, Tree_degree, Key_id,
		                             Key_id, Generation, Physical_block_address &, bool &, Number_of_leaves &,
		                             Number_of_blocks &);

		static char const *type_to_string(Type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};

class Tresor::Virtual_block_device_channel : public Module_channel
{
	private:

		using Request = Virtual_block_device_request;

		enum State {
			SUBMITTED, REQ_GENERATED, REQ_COMPLETE, READ_BLK_SUCCEEDED, WRITE_BLK_SUCCEEDED,
			DECRYPT_LEAF_DATA_SUCCEEDED, ENCRYPT_LEAF_DATA_SUCCEEDED, ALLOC_PBAS_SUCCEEDED };

		Request *_req_ptr { nullptr };
		State _state { REQ_COMPLETE };
		Snapshot_index _snap_idx { 0 };
		Type_1_node_block_walk _t1_blks { };
		Type_1_node_walk _t1_nodes { };
		Tree_level_index _lvl { 0 };
		Virtual_block_address _vba { 0 };
		Tree_walk_pbas _old_pbas { };
		Tree_walk_pbas _new_pbas { };
		Hash _hash { };
		Number_of_blocks _num_blks { 0 };
		Generation _free_gen { 0 };
		Block _encoded_blk { };
		Block _data_blk { };
		bool _first_snapshot { false };
		bool _gen_req_success { false };

		NONCOPYABLE(Virtual_block_device_channel);

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint complete_state, bool &progress, ARGS &&... args)
		{
			generate_req<REQUEST>(complete_state, progress, args..., _gen_req_success);
			_state = REQ_GENERATED;
		}

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _generated_req_completed(State_uint) override;

		void _generate_ft_req(State, bool, Free_tree_request::Type);

		Snapshot &snap() { return _req_ptr->_snapshots.items[_snap_idx]; }

		void _generate_write_blk_req(bool &);

		bool _find_next_snap_to_rekey_vba_at(Snapshot_index &) const;

		void _read_vba(bool &);

		bool _check_and_decode_read_blk(bool &);

		Tree_node_index _node_idx(Tree_level_index, Virtual_block_address) const;

		Type_1_node &_node(Tree_level_index, Virtual_block_address);

		void _mark_req_successful(bool &);

		void _mark_req_failed(bool &, char const *);

		void _set_new_pbas_and_num_blks_for_alloc();

		void _generate_ft_alloc_req_for_write_vba(bool &);

		void _write_vba(bool &);

		void _update_nodes_of_branch_of_written_vba();

		void _rekey_vba(bool &);

		void _generate_ft_alloc_req_for_rekeying(Tree_level_index, bool &);

		void _add_new_root_lvl_to_snap();

		void _add_new_branch_to_snap(Tree_level_index, Tree_node_index);

		void _set_new_pbas_identical_to_curr_pbas();

		void _generate_ft_alloc_req_for_resizing(Tree_level_index, bool &);

		void _extension_step(bool &);

	public:

		Virtual_block_device_channel(Module_channel_id id) : Module_channel { VIRTUAL_BLOCK_DEVICE, id } { }

		void execute(bool &);
};

class Tresor::Virtual_block_device : public Module
{
	private:

		using Channel = Virtual_block_device_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Virtual_block_device);

		void execute(bool &) override;

	public:

		Virtual_block_device();
};

#endif /* _TRESOR__VIRTUAL_BLOCK_DEVICE_H_ */
