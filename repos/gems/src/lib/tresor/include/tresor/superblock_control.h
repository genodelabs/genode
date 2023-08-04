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

namespace Tresor {

	class Superblock_control;
	class Superblock_control_request;
	class Superblock_control_channel;
}

class Tresor::Superblock_control_request : Module_request, Noncopyable
{
	friend class Superblock_control_channel;

	public:

		enum Type {
			READ_VBA, WRITE_VBA, SYNC, INITIALIZE, DEINITIALIZE, VBD_EXTENSION_STEP,
			FT_EXTENSION_STEP, CREATE_SNAPSHOT, DISCARD_SNAPSHOT, INITIALIZE_REKEYING,
			REKEY_VBA };

	private:

		Type const _type;
		Request_offset const _client_req_offset;
		Request_tag const _client_req_tag;
		Number_of_blocks _nr_of_blks;
		Virtual_block_address const _vba;
		bool &_success;
		bool &_client_req_finished;
		Superblock::State &_sb_state;
		Generation &_gen;

	public:

		Superblock_control_request(Module_id, Module_channel_id, Type, Request_offset,
		                           Request_tag, Number_of_blocks, Virtual_block_address,
		                           bool &, bool &, Superblock::State &, Generation &);

		static char const *type_to_string(Type);

		void print(Output &) const override;
};


class Tresor::Superblock_control_channel : public Module_channel
{
	private:

		using Request = Superblock_control_request;

		enum State : State_uint {
			INACTIVE, REQ_SUBMITTED, ACCESS_VBA_AT_VBD_SUCCEEDED,
			REKEY_VBA_AT_VBD_SUCCEEDED, CREATE_KEY_SUCCEEDED,
			TREE_EXT_STEP_IN_TREE_SUCCEEDED, DECRYPT_CURR_KEY_SUCCEEDED,
			DECRYPT_PREV_KEY_SUCCEEDED, READ_SB_HASH_SUCCEEDED, ADD_PREV_KEY_SUCCEEDED,
			ADD_CURR_KEY_SUCCEEDED, REMOVE_PREV_KEY_SUCCEEDED, REMOVE_CURR_KEY_SUCCEEDED,
			READ_SB_SUCCEEDED, REQ_COMPLETE, REQ_GENERATED, SECURE_SB, SECURE_SB_SUCCEEDED };

		enum Secure_sb_state : State_uint {
			SECURE_SB_INACTIVE, STARTED, ENCRYPT_CURR_KEY_SUCCEEDED,
			SECURE_SB_REQ_GENERATED, ENCRYPT_PREV_KEY_SUCCEEDED, SYNC_CACHE_SUCCEEDED,
			WRITE_SB_SUCCEEDED, SYNC_BLK_IO_SUCCEEDED, WRITE_SB_HASH_SUCCEEDED };

		State _state { INACTIVE };
		Constructible<Tree_root> _ft { };
		Constructible<Tree_root> _mt { };
		Secure_sb_state _secure_sb_state { SECURE_SB_INACTIVE };
		Superblock _sb_ciphertext { };
		Block _blk { };
		Generation _gen { INVALID_GENERATION };
		Hash _hash { };
		Physical_block_address _pba { INVALID_PBA };
		Number_of_blocks _nr_of_leaves { 0 };
		Request *_req_ptr { nullptr };
		bool _gen_req_success { false };
		Superblock &_sb;
		Superblock_index &_sb_idx;
		Generation &_curr_gen;

		NONCOPYABLE(Superblock_control_channel);

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _mark_req_successful(bool &);

		void _mark_req_failed(bool &, char const *);

		void _access_vba(Virtual_block_device_request::Type, bool &);

		void _generate_vbd_req(Virtual_block_device_request::Type, State_uint, bool &, Key_id, Virtual_block_address);

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint complete_state, bool &progress, ARGS &&... args)
		{
			generate_req<REQUEST>(complete_state, progress, args..., _gen_req_success);
			if (_state == SECURE_SB)
				_secure_sb_state = SECURE_SB_REQ_GENERATED;
			else
				_state = REQ_GENERATED;
		}

		void _start_secure_sb(bool &);

		void _secure_sb(bool &);

		void _tree_ext_step(Superblock::State, bool, String<4>, bool &);

		void _rekey_vba(bool &);

		void _init_rekeying(bool &);

		void _discard_snap(bool &);

		void _create_snap(bool &);

		void _sync(bool &);

		void _initialize(bool &);

		void _deinitialize(bool &);

	public:

		void execute(bool &);

		Superblock_control_channel(Module_channel_id, Superblock &, Superblock_index &, Generation &);
};

class Tresor::Superblock_control : public Module
{
	private:

		using Channel = Superblock_control_channel;

		enum { NUM_CHANNELS = 1 };

		Superblock _sb { };
		Superblock_index _sb_idx { INVALID_SB_IDX };
		Generation _curr_gen { INVALID_GENERATION };
		Constructible<Channel> _channels[NUM_CHANNELS] { };

		void execute(bool &) override;

	public:

		Virtual_block_address max_vba() const { return _sb.valid() ? _sb.max_vba() : 0; };

		Virtual_block_address resizing_nr_of_pbas() const { return _sb.resizing_nr_of_pbas; }

		Virtual_block_address rekeying_vba() const { return _sb.rekeying_vba; }

		Snapshots_info snapshots_info() const;

		Superblock_info sb_info() const;

		Superblock_control();
};

#endif /* _TRESOR__SUPERBLOCK_CONTROL_H_ */
