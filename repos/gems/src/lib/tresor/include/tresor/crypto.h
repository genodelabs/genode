/*
 * \brief  Module for encrypting/decrypting single data blocks
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__CRYPTO_H_
#define _TRESOR__CRYPTO_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/file.h>

namespace Tresor {

	class Crypto;
	class Crypto_request;
	class Crypto_channel;
}

class Tresor::Crypto_request : public Module_request
{
	friend class Crypto_channel;

	public:

		enum Type { ADD_KEY, REMOVE_KEY, DECRYPT, ENCRYPT, DECRYPT_CLIENT_DATA, ENCRYPT_CLIENT_DATA };

	private:

		Type const _type;
		Request_offset const _client_req_offset;
		Request_tag const _client_req_tag;
		Physical_block_address const _pba;
		Virtual_block_address const _vba;
		Key_id const _key_id;
		Key_value const &_key_plaintext;
		Block &_blk;
		bool &_success;

		NONCOPYABLE(Crypto_request);

	public:

		Crypto_request(Module_id, Module_channel_id, Type, Request_offset, Request_tag, Key_id,
		               Key_value const &, Physical_block_address, Virtual_block_address, Block &, bool &);

		static const char *type_to_string(Type);

		void print(Output &) const override;
};

class Tresor::Crypto_channel : public Module_channel
{
	private:

		using Request = Crypto_request;

		enum State {
			REQ_SUBMITTED, REQ_COMPLETE, PLAINTEXT_BLK_OBTAINED, PLAINTEXT_BLK_SUPPLIED, REQ_GENERATED,
			READ_OK, WRITE_OK, FILE_ERR };

		struct Key_directory
		{
			Crypto_channel &chan;
			Key_id key_id;
			Read_write_file<State> encrypt_file { chan._state, chan._vfs_env, { chan._path, "/keys/", key_id, "/encrypt" } };
			Read_write_file<State> decrypt_file { chan._state, chan._vfs_env, { chan._path, "/keys/", key_id, "/decrypt" } };

			NONCOPYABLE(Key_directory);

			Key_directory(Crypto_channel &chan, Key_id key_id) : chan { chan }, key_id { key_id } { }
		};

		Vfs::Env &_vfs_env;
		Tresor::Path const _path;
		char _add_key_buf[sizeof(Key_id) + KEY_SIZE] { };
		Write_only_file<State> _add_key_file { _state, _vfs_env, { _path, "/add_key" } };
		Write_only_file<State> _remove_key_file { _state, _vfs_env, { _path, "/remove_key" } };
		Constructible<Key_directory> _key_dirs[2] { };
		State _state { REQ_COMPLETE };
		bool _generated_req_success { false };
		Block _blk { };
		Request *_req_ptr { };

		NONCOPYABLE(Crypto_channel);

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

		void _add_key(bool &);

		void _remove_key(bool &);

		void _decrypt(bool &);

		void _encrypt(bool &);

		void _encrypt_client_data(bool &);

		void _decrypt_client_data(bool &);

		void _mark_req_failed(bool &, char const *);

		void _mark_req_successful(bool &);

		Constructible<Key_directory> &_key_dir(Key_id key_id);

	public:

		Crypto_channel(Module_channel_id, Vfs::Env &, Xml_node const &);

		void execute(bool &);
};

class Tresor::Crypto : public Module
{
	private:

		using Request = Crypto_request;
		using Channel = Crypto_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Crypto);

	public:

		struct Add_key : Request
		{
			Add_key(Module_id src_mod, Module_channel_id src_chan, Key &key, bool &succ)
			: Request(src_mod, src_chan, Request::ADD_KEY, 0, 0, key.id, key.value, 0, 0, *(Block*)0, succ) { }
		};

		struct Remove_key : Request
		{
			Remove_key(Module_id src_mod, Module_channel_id src_chan, Key_id key, bool &succ)
			: Request(src_mod, src_chan, Request::REMOVE_KEY, 0, 0, key, *(Key_value*)0, 0, 0, *(Block*)0, succ) { }
		};

		struct Decrypt : Request
		{
			Decrypt(Module_id src_mod, Module_channel_id src_chan, Key_id key, Physical_block_address pba, Block &blk, bool &succ)
			: Request(src_mod, src_chan, Request::DECRYPT, 0, 0, key, *(Key_value*)0, pba, 0, blk, succ) { }
		};

		struct Encrypt : Request
		{
			Encrypt(Module_id src_mod, Module_channel_id src_chan, Key_id key, Physical_block_address pba, Block &blk, bool &succ)
			: Request(src_mod, src_chan, Request::ENCRYPT, 0, 0, key, *(Key_value*)0, pba, 0, blk, succ) { }
		};

		Crypto(Vfs::Env &, Xml_node const &);

		void execute(bool &) override;
};

#endif /* _TRESOR__CRYPTO_H_ */
