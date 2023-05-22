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
#include <tresor/module.h>
#include <tresor/vfs_utilities.h>

namespace Tresor {

	class Crypto;
	class Crypto_request;
	class Crypto_channel;
}

class Tresor::Crypto_request : public Module_request
{
	public:

		enum Type {
			INVALID = 0, ADD_KEY = 1, REMOVE_KEY = 2, DECRYPT = 3, ENCRYPT = 4,
			DECRYPT_CLIENT_DATA = 5, ENCRYPT_CLIENT_DATA = 6 };

	private:

		friend class Crypto;
		friend class Crypto_channel;

		Type     _type                { INVALID };
		uint64_t _client_req_offset   { 0 };
		uint64_t _client_req_tag      { 0 };
		uint64_t _pba                 { 0 };
		uint64_t _vba                 { 0 };
		uint32_t _key_id              { 0 };
		addr_t   _key_plaintext_ptr   { 0 };
		addr_t   _plaintext_blk_ptr   { 0 };
		addr_t   _ciphertext_blk_ptr  { 0 };
		bool     _success             { false };

	public:

		Crypto_request() { }

		Type type() const { return _type; }

		Crypto_request(uint64_t  src_module_id,
		               uint64_t  src_request_id,
		               size_t    req_type,
		               uint64_t  client_req_offset,
		               uint64_t  client_req_tag,
		               uint32_t  key_id,
		               void     *key_plaintext_ptr,
		               uint64_t  pba,
		               uint64_t  vba,
		               void     *plaintext_blk_ptr,
		               void     *ciphertext_blk_ptr);

		bool success() const { return _success; }

		static const char *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		void print(Output &out) const override
		{
			Genode::print(out, type_to_string(_type));
			switch (_type) {
			case ADD_KEY:
			case REMOVE_KEY:
				Genode::print(out, " ", _key_id);
				break;
			case DECRYPT:
			case ENCRYPT:
			case DECRYPT_CLIENT_DATA:
			case ENCRYPT_CLIENT_DATA:
				Genode::print(out, " pba ", _pba);
				break;
			default:
				break;
			}
		}
};

class Tresor::Crypto_channel
{
	private:

		friend class Crypto;

		enum State {
			INACTIVE, SUBMITTED, COMPLETE, OBTAIN_PLAINTEXT_BLK_PENDING,
			OBTAIN_PLAINTEXT_BLK_IN_PROGRESS, OBTAIN_PLAINTEXT_BLK_COMPLETE,
			SUPPLY_PLAINTEXT_BLK_PENDING, SUPPLY_PLAINTEXT_BLK_IN_PROGRESS,
			SUPPLY_PLAINTEXT_BLK_COMPLETE, OP_WRITTEN_TO_VFS_HANDLE,
			QUEUE_READ_SUCCEEDED };

		State            _state                 { INACTIVE };
		Crypto_request   _request               { };
		bool             _generated_req_success { false };
		Vfs::Vfs_handle *_vfs_handle            { nullptr };
		char             _blk_buf[BLOCK_SIZE]   { 0 };

	public:

		Crypto_request const &request() const { return _request; }
};

class Tresor::Crypto : public Module
{
	private:

		using Request = Crypto_request;
		using Channel = Crypto_channel;
		using Write_result = Vfs::File_io_service::Write_result;
		using Read_result = Vfs::File_io_service::Read_result;

		enum { NR_OF_CHANNELS = 4 };

		struct Key_directory
		{
			Vfs::Vfs_handle  *encrypt_handle { nullptr };
			Vfs::Vfs_handle  *decrypt_handle { nullptr };
			uint32_t          key_id         { 0 };
		};

		Vfs::Env         &_vfs_env;
		String<32> const  _path;
		Vfs::Vfs_handle  &_add_key_handle;
		Vfs::Vfs_handle  &_remove_key_handle;
		Channel           _channels[NR_OF_CHANNELS] { };
		Key_directory     _key_dirs[2]              { };

		Key_directory &_lookup_key_dir(uint32_t key_id);

		void _execute_add_key(Channel &channel,
		                      bool    &progress);

		void _execute_remove_key(Channel &channel,
		                         bool    &progress);

		void _execute_decrypt(Channel &channel,
		                      bool    &progress);

		void _execute_encrypt(Channel &channel,
		                      bool    &progress);

		void _execute_encrypt_client_data(Channel &channel,
		                                  bool    &progress);

		void _execute_decrypt_client_data(Channel &channel,
		                                  bool    &progress);

		void _mark_req_failed(Channel    &channel,
		                      bool       &progress,
		                      char const *str);

		void _mark_req_successful(Channel &channel,
		                          bool    &progress);


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

		Crypto(Vfs::Env       &vfs_env,
		       Xml_node const &xml_node);
};

#endif /* _TRESOR__CRYPTO_H_ */
