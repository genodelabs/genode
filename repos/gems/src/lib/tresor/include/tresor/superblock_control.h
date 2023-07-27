/*
 * \brief  Module for management of the superblocks
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
#include <tresor/module.h>

namespace Tresor {

	class Superblock_control;
	class Superblock_control_request;
	class Superblock_control_channel;
}

class Tresor::Superblock_control_request : public Module_request
{
	public:

		enum Type {
			INVALID = 0, READ_VBA = 1, WRITE_VBA = 2, SYNC = 3, INITIALIZE = 4,
			DEINITIALIZE = 5,
			VBD_EXTENSION_STEP = 6,
			FT_EXTENSION_STEP = 7,
			CREATE_SNAPSHOT = 8,
			DISCARD_SNAPSHOT = 9,
			INITIALIZE_REKEYING = 10,
			REKEY_VBA = 11
		};

	private:

		friend class Superblock_control;
		friend class Superblock_control_channel;

		Type                  _type              { INVALID };
		uint64_t              _client_req_offset { 0 };
		uint64_t              _client_req_tag    { 0 };
		Virtual_block_address _vba               { 0 };
		Superblock::State     _sb_state          { Superblock::INVALID };
		Number_of_blocks      _nr_of_blks        { 0 };
		bool                  _success           { false };
		bool                  _request_finished  { false };

	public:

		Superblock_control_request() { }

		Type type() const { return _type; }

		Superblock_control_request(Module_id         src_module_id,
		                           Module_request_id src_request_id);

		static void create(void             *buf_ptr,
		                   size_t            buf_size,
		                   uint64_t          src_module_id,
		                   uint64_t          src_request_id,
		                   size_t            req_type,
		                   uint64_t          client_req_offset,
		                   uint64_t          client_req_tag,
		                   Number_of_blocks  nr_of_blks,
		                   uint64_t          vba);

		Superblock::State sb_state() { return _sb_state; }

		bool success() const { return _success; }

		bool request_finished() const { return _request_finished; }

		static char const *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		void print(Output &out) const override
		{
			Genode::print(out, type_to_string(_type));
			switch (_type) {
			case REKEY_VBA:
			case READ_VBA:
			case WRITE_VBA:
				Genode::print(out, " ", _vba);
				break;
			default:
				break;
			}
		}
};

class Tresor::Superblock_control_channel
{
	private:

		friend class Superblock_control;

		enum State {
			SUBMITTED,
			READ_VBA_AT_VBD_PENDING,
			READ_VBA_AT_VBD_IN_PROGRESS,
			READ_VBA_AT_VBD_COMPLETED,
			WRITE_VBA_AT_VBD_PENDING,
			WRITE_VBA_AT_VBD_IN_PROGRESS,
			WRITE_VBA_AT_VBD_COMPLETED,
			READ_SB_PENDING,
			READ_SB_IN_PROGRESS,
			READ_SB_COMPLETED,
			READ_CURRENT_SB_PENDING,
			READ_CURRENT_SB_IN_PROGRESS,
			READ_CURRENT_SB_COMPLETED,
			REKEY_VBA_IN_VBD_PENDING,
			REKEY_VBA_IN_VBD_IN_PROGRESS,
			REKEY_VBA_IN_VBD_COMPLETED,
			VBD_EXT_STEP_IN_VBD_PENDING,
			VBD_EXT_STEP_IN_VBD_IN_PROGRESS,
			FT_EXT_STEP_IN_FT_PENDING,
			FT_EXT_STEP_IN_FT_IN_PROGRESS,
			TREE_EXT_STEP_IN_TREE_COMPLETED,
			CREATE_KEY_PENDING,
			CREATE_KEY_IN_PROGRESS,
			CREATE_KEY_COMPLETED,
			ENCRYPT_CURRENT_KEY_PENDING,
			ENCRYPT_CURRENT_KEY_IN_PROGRESS,
			ENCRYPT_CURRENT_KEY_COMPLETED,
			ENCRYPT_PREVIOUS_KEY_PENDING,
			ENCRYPT_PREVIOUS_KEY_IN_PROGRESS,
			ENCRYPT_PREVIOUS_KEY_COMPLETED,
			DECRYPT_CURRENT_KEY_PENDING,
			DECRYPT_CURRENT_KEY_IN_PROGRESS,
			DECRYPT_CURRENT_KEY_COMPLETED,
			DECRYPT_PREVIOUS_KEY_PENDING,
			DECRYPT_PREVIOUS_KEY_IN_PROGRESS,
			DECRYPT_PREVIOUS_KEY_COMPLETED,
			SYNC_CACHE_PENDING,
			SYNC_CACHE_IN_PROGRESS,
			SYNC_CACHE_COMPLETED,
			ADD_KEY_AT_CRYPTO_MODULE_PENDING,
			ADD_KEY_AT_CRYPTO_MODULE_IN_PROGRESS,
			ADD_KEY_AT_CRYPTO_MODULE_COMPLETED,
			ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING,
			ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_IN_PROGRESS,
			ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_COMPLETED,
			ADD_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING,
			ADD_CURRENT_KEY_AT_CRYPTO_MODULE_IN_PROGRESS,
			ADD_CURRENT_KEY_AT_CRYPTO_MODULE_COMPLETED,
			REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING,
			REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_IN_PROGRESS,
			REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_COMPLETED,
			REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING,
			REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_IN_PROGRESS,
			REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_COMPLETED,
			WRITE_SB_PENDING,
			WRITE_SB_IN_PROGRESS,
			WRITE_SB_COMPLETED,
			SYNC_BLK_IO_PENDING,
			SYNC_BLK_IO_IN_PROGRESS,
			SYNC_BLK_IO_COMPLETED,
			SECURE_SB_PENDING,
			SECURE_SB_IN_PROGRESS,
			SECURE_SB_COMPLETED,
			MAX_SB_HASH_PENDING,
			MAX_SB_HASH_IN_PROGRESS,
			MAX_SB_HASH_COMPLETED,
			COMPLETED
		};

		enum Tag_type {
			TAG_SB_CTRL_VBD_VBD_EXT_STEP,
			TAG_SB_CTRL_FT_FT_EXT_STEP,
			TAG_SB_CTRL_VBD_RKG_REKEY_VBA,
			TAG_SB_CTRL_VBD_RKG_READ_VBA,
			TAG_SB_CTRL_VBD_RKG_WRITE_VBA,
			TAG_SB_CTRL_TA_ENCRYPT_KEY,
			TAG_SB_CTRL_CACHE,
			TAG_SB_CTRL_BLK_IO_READ_SB,
			TAG_SB_CTRL_BLK_IO_WRITE_SB,
			TAG_SB_CTRL_BLK_IO_SYNC,
			TAG_SB_CTRL_TA_SECURE_SB,
			TAG_SB_CTRL_TA_LAST_SB_HASH,
			TAG_SB_CTRL_TA_DECRYPT_KEY,
			TAG_SB_CTRL_TA_CREATE_KEY,
			TAG_SB_CTRL_CRYPTO_ADD_KEY,
			TAG_SB_CTRL_CRYPTO_REMOVE_KEY,
		};

		struct Generated_prim
		{
			enum Type { READ, WRITE, SYNC };

			Type     op     { READ };
			bool     succ   { false };
			Tag_type tg     { };
			uint64_t blk_nr { 0 };
			uint64_t idx    { 0 };
		};

		State                      _state              { SUBMITTED };
		Superblock_control_request _request            { };
		Generated_prim             _generated_prim     { };
		Key                        _key_plaintext      { };
		Superblock                 _sb_ciphertext      { };
		Block                      _encoded_blk        { };
		Superblock_index           _sb_idx             { 0 };
		bool                       _sb_found           { false };
		Superblock_index           _read_sb_idx        { 0 };
		Generation                 _generation         { 0 };
		Snapshots                  _snapshots          { };
		Hash                       _hash               { };
		Key                        _curr_key_plaintext { };
		Key                        _prev_key_plaintext { };
		Physical_block_address     _pba                { 0 };
		Number_of_blocks           _nr_of_leaves       { 0 };
		Type_1_node                _ft_root            { };
		Tree_level_index           _ft_max_lvl         { 0 };
		Number_of_leaves           _ft_nr_of_leaves    { 0 };

	public:

		Superblock_control_request const &request() const { return _request; }
};

class Tresor::Superblock_control : public Module
{
	private:

		using Request = Superblock_control_request;
		using Channel = Superblock_control_channel;
		using Generated_prim = Channel::Generated_prim;
		using Tag = Channel::Tag_type;

		enum { NR_OF_CHANNELS = 1 };

		Superblock        _sb                       { };
		Superblock_index _sb_idx                   { 0 };
		Generation        _curr_gen                 { 0 };
		Channel           _channels[NR_OF_CHANNELS] { };

		void _mark_req_failed(Channel    &chan,
		                      bool       &progress,
		                      char const *str);

		void _mark_req_successful(Channel &chan,
		                          bool    &progress);

		static char const *_state_to_step_label(Channel::State state);

		bool _handle_failed_generated_req(Channel &chan,
		                                  bool    &progress);

		void _secure_sb_init(Channel  &chan,
		                     uint64_t  chan_idx,
		                     bool     &progress);

		void _secure_sb_encr_curr_key_compl(Channel  &chan,
		                                    uint64_t  chan_idx,
		                                    bool     &progress);

		void _secure_sb_encr_prev_key_compl(Channel  &chan,
		                                    uint64_t  chan_idx,
		                                    bool     &progress);

		void _secure_sb_sync_cache_compl(Channel  &chan,
		                                 uint64_t  chan_idx,
		                                 bool     &progress);

		void _secure_sb_write_sb_compl(Channel  &chan,
		                               uint64_t  chan_idx,
		                               bool     &progress);

		void _secure_sb_sync_blk_io_compl(Channel  &chan,
		                                  uint64_t  chan_idx,
		                                  bool     &progress);

		void _init_sb_without_key_values(Superblock const &, Superblock &);

		void _execute_sync(Channel &, uint64_t const job_idx, Superblock &,
                           Superblock_index &, Generation &, bool &progress);

		void _execute_tree_ext_step(Channel           &chan,
		                            uint64_t           chan_idx,
		                            Superblock::State  tree_ext_sb_state,
		                            bool               tree_ext_verbose,
		                            Tag                tree_ext_tag,
		                            Channel::State     tree_ext_pending_state,
		                            String<4>  tree_name,
		                            bool              &progress);

		void _execute_rekey_vba(Channel  &chan,
		                        uint64_t  chan_idx,
		                        bool     &progress);

		void _execute_initialize_rekeying(Channel  &chan,
		                                  uint64_t  chan_idx,
		                                  bool     &progress);

		void _execute_read_vba(Channel &, uint64_t const job_idx,
		                       Superblock const &, bool &progress);

		void _execute_write_vba(Channel &, uint64_t const job_idx,
                              Superblock &, Generation const &, bool &progress);

		void _execute_initialize(Channel &, uint64_t const job_idx,
		                         Superblock &, Superblock_index &,
		                         Generation &, bool &progress);

		void _execute_deinitialize(Channel &, uint64_t const job_idx,
		                           Superblock &, Superblock_index &,
		                           Generation &, bool &progress);


		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		bool _peek_completed_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		void execute(bool &) override;

		bool _peek_generated_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

		void generated_request_complete(Module_request &req) override;

	public:

		Virtual_block_address max_vba() const;

		Virtual_block_address resizing_nr_of_pbas() const;

		Virtual_block_address rekeying_vba() const;

		void snapshot_generations(Snapshot_generations &generations) const
		{
			if (_sb.valid()) {

				for (Snapshot_index idx { 0 };
				     idx < MAX_NR_OF_SNAPSHOTS;
				     idx++) {

					Snapshot const &snap { _sb.snapshots.items[idx] };
					if (snap.valid && snap.keep)
						generations.items[idx] = snap.gen;
					else
						generations.items[idx] = INVALID_GENERATION;
				}
			} else {

				generations = Snapshot_generations { };
			}
		}

		Superblock_info sb_info() const
		{
			if (_sb.valid())

				return Superblock_info {
					true, _sb.state == Superblock::REKEYING,
					_sb.state == Superblock::EXTENDING_FT,
					_sb.state == Superblock::EXTENDING_VBD };

			else

				return Superblock_info { };
		}
};

#endif /* _TRESOR__SUPERBLOCK_CONTROL_H_ */
