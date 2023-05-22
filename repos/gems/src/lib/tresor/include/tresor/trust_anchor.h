/*
 * \brief  Module for accessing the systems trust anchor
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__TRUST_ANCHOR_H_
#define _TRESOR__TRUST_ANCHOR_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/module.h>
#include <tresor/vfs_utilities.h>

namespace Tresor {

	class Trust_anchor;
	class Trust_anchor_request;
	class Trust_anchor_channel;
}

class Tresor::Trust_anchor_request : public Module_request
{
	public:

		enum Type {
			INVALID = 0, CREATE_KEY = 1, ENCRYPT_KEY = 2, DECRYPT_KEY = 3,
			SECURE_SUPERBLOCK = 4, GET_LAST_SB_HASH = 5, INITIALIZE = 6 };

	private:

		friend class Trust_anchor;
		friend class Trust_anchor_channel;

		Type    _type                     { INVALID };
		uint8_t _key_plaintext[KEY_SIZE]  { 0 };
		uint8_t _key_ciphertext[KEY_SIZE] { 0 };
		Hash    _hash                     { };
		addr_t  _passphrase_ptr           { 0 };
		bool    _success                  { false };

	public:

		Trust_anchor_request() { }

		Trust_anchor_request(Module_id         src_module_id,
		                     Module_request_id src_request_id);

		static void create(void       *buf_ptr,
		                   size_t      buf_size,
		                   uint64_t    src_module_id,
		                   uint64_t    src_request_id,
		                   size_t      req_type,
		                   void       *key_plaintext_ptr,
		                   void       *key_ciphertext_ptr,
		                   char const *passphrase_ptr,
		                   void       *hash_ptr);

		void *hash_ptr() { return (void *)&_hash; }
		void *key_plaintext_ptr() { return (void *)&_key_plaintext; }
		void *key_ciphertext_ptr() { return (void *)&_key_ciphertext; }

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};

class Tresor::Trust_anchor_channel
{
	private:

		friend class Trust_anchor;

		enum State {
			INACTIVE, SUBMITTED, WRITE_PENDING, WRITE_IN_PROGRESS,
			READ_PENDING, READ_IN_PROGRESS, COMPLETE };

		State                _state       { INACTIVE };
		Trust_anchor_request _request     { };
		Vfs::file_offset     _file_offset { 0 };
		size_t               _file_size   { 0 };
};

class Tresor::Trust_anchor : public Module
{
	private:

		using Request = Trust_anchor_request;
		using Channel = Trust_anchor_channel;
		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;

		enum { NR_OF_CHANNELS = 1 };

		Vfs::Env          &_vfs_env;
		char               _read_buf[64];
		String<128> const  _path;
		String<128> const  _decrypt_path             { _path, "/decrypt" };
		Vfs::Vfs_handle   &_decrypt_file             { vfs_open_rw(_vfs_env, { _decrypt_path }) };
		String<128> const  _encrypt_path             { _path, "/encrypt" };
		Vfs::Vfs_handle   &_encrypt_file             { vfs_open_rw(_vfs_env, { _encrypt_path }) };
		String<128> const  _generate_key_path        { _path, "/generate_key" };
		Vfs::Vfs_handle   &_generate_key_file        { vfs_open_rw(_vfs_env, { _generate_key_path }) };
		String<128> const  _initialize_path          { _path, "/initialize" };
		Vfs::Vfs_handle   &_initialize_file          { vfs_open_rw(_vfs_env, { _initialize_path }) };
		String<128> const  _hashsum_path             { _path, "/hashsum" };
		Vfs::Vfs_handle   &_hashsum_file             { vfs_open_rw(_vfs_env, { _hashsum_path }) };
		Channel            _channels[NR_OF_CHANNELS] { };

		void
		_execute_write_read_operation(Vfs::Vfs_handle   &file,
		                              String<128> const &file_path,
		                              Channel           &channel,
		                              char const        *write_buf,
		                              char              *read_buf,
		                              size_t             read_size,
		                              bool              &progress);

		void _execute_write_operation(Vfs::Vfs_handle   &file,
		                              String<128> const &file_path,
		                              Channel           &channel,
		                              char const        *write_buf,
		                              bool              &progress,
		                              bool               result_via_read);

		void _execute_read_operation(Vfs::Vfs_handle   &file,
		                             String<128> const &file_path,
		                             Channel           &channel,
		                             char              *read_buf,
		                             bool              &progress);


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

	public:

		Trust_anchor(Vfs::Env       &vfs_env,
		             Xml_node const &xml_node);


		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		void execute(bool &) override;
};

#endif /* _TRESOR__TRUST_ANCHOR_H_ */
