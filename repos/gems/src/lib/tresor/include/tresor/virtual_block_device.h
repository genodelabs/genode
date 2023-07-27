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
#include <tresor/module.h>
#include <tresor/types.h>
#include <tresor/vfs_utilities.h>

namespace Tresor {

	class Virtual_block_device;
	class Virtual_block_device_request;
	class Virtual_block_device_channel;
}

class Tresor::Virtual_block_device_request : public Module_request
{
	public:

		enum Type {
			INVALID = 0, REKEY_VBA = 3, READ_VBA = 1, WRITE_VBA = 2, VBD_EXTENSION_STEP = 4 };

	private:

		friend class Virtual_block_device;
		friend class Virtual_block_device_channel;

		Type                   _type                    { INVALID };
		Virtual_block_address  _vba                     { 0 };
		Snapshots              _snapshots               { };
		Tree_degree            _snapshots_degree        { 0 };
		Generation             _curr_gen                { INVALID_GENERATION };
		Key_id                 _new_key_id              { 0 };
		Key_id                 _old_key_id              { 0 };
		addr_t                 _ft_root_pba_ptr         { 0 };
		addr_t                 _ft_root_gen_ptr         { 0 };
		addr_t                 _ft_root_hash_ptr        { 0 };
		uint64_t               _ft_max_level            { 0 };
		uint64_t               _ft_degree               { 0 };
		uint64_t               _ft_leaves               { 0 };
		addr_t                 _mt_root_pba_ptr         { 0 };
		addr_t                 _mt_root_gen_ptr         { 0 };
		addr_t                 _mt_root_hash_ptr        { 0 };
		uint64_t               _mt_max_level            { 0 };
		uint64_t               _mt_degree               { 0 };
		uint64_t               _mt_leaves               { 0 };
		uint64_t               _vbd_degree              { 0 };
		uint64_t               _vbd_highest_vba         { 0 };
		bool                   _rekeying                { 0 };
		uint64_t               _client_req_offset       { 0 };
		uint64_t               _client_req_tag          { 0 };
		Generation             _last_secured_generation { INVALID_GENERATION };
		Physical_block_address _pba                     { 0 };
		Number_of_blocks       _nr_of_pbas              { 0 };
		Number_of_leaves       _nr_of_leaves            { 0 };
		bool                   _success                 { false };

	public:

		Virtual_block_device_request() { }

		Virtual_block_device_request(Module_id         src_module_id,
		                             Module_request_id src_request_id);

		static void create(void                   *buf_ptr,
		                   size_t                  buf_size,
		                   uint64_t                src_module_id,
		                   uint64_t                src_request_id,
		                   size_t                  req_type,
		                   uint64_t                client_req_offset,
		                   uint64_t                client_req_tag,
		                   Generation              last_secured_generation,
		                   addr_t                  ft_root_pba_ptr,
		                   addr_t                  ft_root_gen_ptr,
		                   addr_t                  ft_root_hash_ptr,
		                   uint64_t                ft_max_level,
		                   uint64_t                ft_degree,
		                   uint64_t                ft_leaves,
		                   addr_t                  mt_root_pba_ptr,
		                   addr_t                  mt_root_gen_ptr,
		                   addr_t                  mt_root_hash_ptr,
		                   uint64_t                mt_max_level,
		                   uint64_t                mt_degree,
		                   uint64_t                mt_leaves,
		                   uint64_t                vbd_degree,
		                   uint64_t                vbd_highest_vba,
		                   bool                    rekeying,
		                   Virtual_block_address   vba,
		                   Snapshot  const        *snapshot_ptr,
		                   Snapshots const        *snapshots_ptr,
		                   Tree_degree             snapshots_degree,
		                   Key_id                  old_key_id,
		                   Key_id                  new_key_id,
		                   Generation              current_gen,
		                   Key_id                  key_id,
		                   Physical_block_address  first_pba,
		                   Number_of_blocks        nr_of_pbas);

		bool success() const { return _success; }

		Physical_block_address pba() const { return _pba; }

		Number_of_blocks nr_of_pbas() const { return _nr_of_pbas; }

		Number_of_leaves nr_of_leaves() const { return _nr_of_leaves; }

		Snapshot *snapshot_ptr() { return &_snapshots.items[0]; }

		Snapshots *snapshots_ptr() { return &_snapshots; }

		static char const *type_to_string(Type type);

		char const *type_name() const { return type_to_string(_type); }


		/********************
		 ** Module_request **
		 ********************/

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};

class Tresor::Virtual_block_device_channel
{
	private:

		friend class Virtual_block_device;

		using Request = Virtual_block_device_request;

		enum State {
			SUBMITTED,
			READ_ROOT_NODE_PENDING,
			READ_ROOT_NODE_IN_PROGRESS,
			READ_ROOT_NODE_COMPLETED,
			READ_INNER_NODE_PENDING,
			READ_INNER_NODE_IN_PROGRESS,
			READ_INNER_NODE_COMPLETED,
			READ_LEAF_NODE_PENDING,
			READ_LEAF_NODE_IN_PROGRESS,
			READ_LEAF_NODE_COMPLETED,
			READ_CLIENT_DATA_FROM_LEAF_NODE_PENDING,
			READ_CLIENT_DATA_FROM_LEAF_NODE_IN_PROGRESS,
			READ_CLIENT_DATA_FROM_LEAF_NODE_COMPLETED,
			WRITE_CLIENT_DATA_TO_LEAF_NODE_PENDING,
			WRITE_CLIENT_DATA_TO_LEAF_NODE_IN_PROGRESS,
			WRITE_CLIENT_DATA_TO_LEAF_NODE_COMPLETED,
			DECRYPT_LEAF_NODE_PENDING,
			DECRYPT_LEAF_NODE_IN_PROGRESS,
			DECRYPT_LEAF_NODE_COMPLETED,
			ALLOC_PBAS_AT_LEAF_LVL_PENDING,
			ALLOC_PBAS_AT_LEAF_LVL_IN_PROGRESS,
			ALLOC_PBAS_AT_LEAF_LVL_COMPLETED,
			ALLOC_PBAS_AT_LOWEST_INNER_LVL_PENDING,
			ALLOC_PBAS_AT_LOWEST_INNER_LVL_IN_PROGRESS,
			ALLOC_PBAS_AT_LOWEST_INNER_LVL_COMPLETED,
			ALLOC_PBAS_AT_HIGHER_INNER_LVL_PENDING,
			ALLOC_PBAS_AT_HIGHER_INNER_LVL_IN_PROGRESS,
			ALLOC_PBAS_AT_HIGHER_INNER_LVL_COMPLETED,
			ENCRYPT_LEAF_NODE_PENDING,
			ENCRYPT_LEAF_NODE_IN_PROGRESS,
			ENCRYPT_LEAF_NODE_COMPLETED,
			WRITE_LEAF_NODE_PENDING,
			WRITE_LEAF_NODE_IN_PROGRESS,
			WRITE_LEAF_NODE_COMPLETED,
			WRITE_INNER_NODE_PENDING,
			WRITE_INNER_NODE_IN_PROGRESS,
			WRITE_INNER_NODE_COMPLETED,
			WRITE_ROOT_NODE_PENDING,
			WRITE_ROOT_NODE_IN_PROGRESS,
			WRITE_ROOT_NODE_COMPLETED,
			COMPLETED
		};

		struct Type_1_node_blocks
		{
			Type_1_node_block items[TREE_MAX_LEVEL] { };
		};

		struct Type_1_node_blocks_pbas
		{
			Physical_block_address items[TREE_MAX_LEVEL] { 0 };
		};

		enum Tag_type
		{
			TAG_INVALID,
			TAG_VBD_CACHE,
			TAG_VBD_BLK_IO_WRITE_CLIENT_DATA,
			TAG_VBD_BLK_IO_READ_CLIENT_DATA,
			TAG_VBD_BLK_IO,
			TAG_VBD_FT_ALLOC_FOR_NON_RKG,
			TAG_VBD_FT_ALLOC_FOR_RKG_CURR_GEN_BLKS,
			TAG_VBD_FT_ALLOC_FOR_RKG_OLD_GEN_BLKS,
			TAG_VBD_CRYPTO_ENCRYPT,
			TAG_VBD_CRYPTO_DECRYPT,
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

		Snapshot &snapshots(Snapshot_index idx)
		{
			if (idx < MAX_NR_OF_SNAPSHOTS)
				return _request._snapshots.items[idx];

			class Snapshot_idx_too_large { };
			throw Snapshot_idx_too_large { };
		}

		Snapshot &snap();

		void _log_rekeying_pba_alloc() const;

		Virtual_block_device_request _request          { };
		State                        _state            { SUBMITTED };
		Generated_prim               _generated_prim   { };
		Snapshot_index               _snapshot_idx     { 0 };
		Type_1_node_blocks           _t1_blks          { };
		Type_1_node_blocks_pbas      _t1_blks_old_pbas { };
		Tree_level_index             _t1_blk_idx       { 0 };
		Virtual_block_address        _vba              { 0 };
		Type_1_node_walk             _t1_node_walk     { };
		Tree_walk_pbas               _new_pbas         { };
		Hash                         _hash             { };
		Number_of_blocks             _nr_of_blks       { 0 };
		Generation                   _last_secured_gen { 0 };
		Generation                   _free_gen         { 0 };
		Block                        _encoded_blk      { };
		Block                        _data_blk         { };
		Physical_block_address       _data_blk_old_pba { 0 };
		bool                         _first_snapshot   { false };
};

class Tresor::Virtual_block_device : public Module
{
	private:

		using Channel = Virtual_block_device_channel;
		using Request = Virtual_block_device_request;
		using Generated_prim = Channel::Generated_prim;
		using Type_1_node_blocks = Channel::Type_1_node_blocks;

		enum { NR_OF_CHANNELS = 1 };
		enum { FIRST_T1_NODE_BLKS_IDX = 1 };
		enum { MAX_T1_NODE_BLKS_IDX = 6 };

		Channel _channels[NR_OF_CHANNELS] { };

		static char const *_state_to_step_label(Channel::State state);

		bool _handle_failed_generated_req(Channel &chan,
		                                  bool    &progress);

		bool _find_next_snap_to_rekey_vba_at(Channel const   &chan,
		                                     Snapshot_index  &next_snap_idx);

		void _execute_read_vba           (Channel &, uint64_t, bool &);
		void _execute_write_vba          (Channel &, uint64_t, bool &);
		void _execute_rekey_vba          (Channel &, uint64_t, bool &);
		void _execute_vbd_extension_step (Channel &, uint64_t, bool &);

		void _mark_req_failed(Channel    &chan,
		                      bool       &progress,
		                      char const *str);

		void _mark_req_successful(Channel &chan,
		                          bool    &progress);

		void _check_that_primitive_was_successful(Channel::Generated_prim const &);

		void _execute_read_vba_read_inner_node_completed(Channel &channel,
		                                                 uint64_t const  job_idx,
		                                                 bool &progress);

		Virtual_block_address _tree_max_max_vba(Tree_degree     snap_degree,
		                                        Snapshot const &snap);

		void _update_nodes_of_branch_of_written_vba(Snapshot &snapshot,
		                                            uint64_t const snapshot_degree,
		                                            uint64_t const vba,
		                                            Tree_walk_pbas const &new_pbas,
		                                            Hash const & leaf_hash,
		                                            uint64_t curr_gen,
		                                            Channel::Type_1_node_blocks &t1_blks);

		void
		_alloc_pba_from_resizing_contingent(Physical_block_address &first_pba,
		                                    Number_of_blocks       &nr_of_pbas,
		                                    Physical_block_address &allocated_pba);

		void _set_args_in_order_to_write_client_data_to_leaf_node(Tree_walk_pbas          const &,
		                                                          uint64_t                const  job_idx,
		                                                          Channel::State                &,
		                                                          Channel::Generated_prim       &,
		                                                          bool                          &progress);

		void _set_new_pbas_identical_to_current_pbas(Channel &chan);

		void
		_add_new_branch_to_snap_using_pba_contingent(Channel           &chan,
		                                             Tree_level_index   mount_at_lvl,
		                                             Tree_node_index    mount_at_child_idx);

		void
		_set_args_for_alloc_of_new_pbas_for_resizing(Channel          &chan,
		                                             uint64_t          chan_idx,
		                                             Tree_level_index  min_lvl,
		                                             bool             &progress);

		void _add_new_root_lvl_to_snap_using_pba_contingent(Channel &chan);

		void _check_hash_of_read_type_1_node(Channel &chan,
		                                     Snapshot const &snapshot,
		                                     uint64_t const snapshots_degree,
		                                     uint64_t const t1_blk_idx,
		                                     Channel::Type_1_node_blocks const &t1_blks,
		                                     uint64_t const vba);

		void _initialize_new_pbas_and_determine_nr_of_pbas_to_allocate(uint64_t const curr_gen,
		                                                               Snapshot const &snapshot,
		                                                               uint64_t const snapshots_degree,
		                                                               uint64_t const vba,
		                                                               Channel::Type_1_node_blocks const &t1_blks,
		                                                               Tree_walk_pbas &new_pbas,
		                                                               uint64_t &nr_of_blks);

		void _set_args_for_alloc_of_new_pbas_for_branch_of_written_vba(uint64_t curr_gen,
		                                                               Snapshot const &snapshot,
		                                                               uint64_t const snapshots_degree,
		                                                               uint64_t const vba,
		                                                               Channel::Type_1_node_blocks const &t1_blks,
		                                                               uint64_t const prim_idx,
		                                                               uint64_t                 &free_gen,
		                                                               Type_1_node_walk         &t1_walk,
		                                                               Channel::State           &state,
		                                                               Channel::Generated_prim  &prim,
		                                                               bool                     &progress);

		void _set_args_for_alloc_of_new_pbas_for_rekeying(Channel          &chan,
		                                                  uint64_t  chan_idx,
		                                                  Tree_level_index  min_lvl);

		void _set_args_in_order_to_read_type_1_node(Snapshot const &snapshot,
		                                            uint64_t const  snapshots_degree,
		                                            uint64_t const  t1_blk_idx,
		                                            Channel::Type_1_node_blocks const &t1_blks,
		                                            uint64_t const  vba,
		                                            uint64_t const  job_idx,
		                                            Channel::State &state,
		                                            Channel::Generated_prim &prim,
		                                            bool &progress);

		void _set_args_for_write_back_of_t1_lvl(Tree_level_index const max_lvl_idx,
		                                        uint64_t const  t1_lvl_idx,
		                                        uint64_t const  pba,
		                                        uint64_t const  prim_idx,
		                                        Channel::State &state,
		                                        bool &progress,
		                                        Channel::Generated_prim &prim);

		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &mod_req) override;

		bool _peek_completed_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		void execute(bool &) override;

		bool _peek_generated_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

		void generated_request_complete(Module_request &req) override;
};

#endif /* _TRESOR__VIRTUAL_BLOCK_DEVICE_H_ */
