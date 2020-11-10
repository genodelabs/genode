/*
 * \brief  Implementation of the Crypto module API using the Crypto VFS API
 * \author Martin Stein
 * \date   2020-10-29
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CBE_TESTER__CRYPTO_H_
#define _CBE_TESTER__CRYPTO_H_

/* CBE includes */
#include <cbe/types.h>

/* CBE tester includes */
#include <vfs_utilities.h>

class Crypto
{
	public:

		enum class Operation
		{
			INVALID,
			DECRYPT_BLOCK,
			ENCRYPT_BLOCK
		};

		enum class Result
		{
			SUCCEEDED,
			FAILED,
			RETRY_LATER
		};

	private:

		using Read_result = Vfs::File_io_service::Read_result;
		using Open_result = Vfs::Directory_service::Open_result;

		struct Key_directory
		{
			Vfs::Vfs_handle  *encrypt_handle { nullptr };
			Vfs::Vfs_handle  *decrypt_handle { nullptr };
			Genode::uint32_t  key_id         { 0 };
		};

		enum class Job_state
		{
			SUBMITTED,
			OP_WRITTEN_TO_VFS_HANDLE,
			READING_VFS_HANDLE_SUCCEEDED,
			COMPLETE
		};

		struct Job
		{
			Cbe::Request                      request        { };
			Vfs::Vfs_handle                  *handle         { nullptr };
			Job_state                         state          { Job_state::SUBMITTED };
			Operation                         op             { Operation::INVALID };
			Cbe::Crypto_cipher_buffer::Index  cipher_buf_idx { 0 };
			Cbe::Crypto_plain_buffer::Index   plain_buf_idx  { 0 };
		};

		Vfs::Env                 &_env;
		Genode::String<32> const  _path;
		Vfs::Vfs_handle          &_add_key_handle;
		Vfs::Vfs_handle          &_remove_key_handle;
		Vfs_io_response_handler   _vfs_io_response_handler;
		Key_directory             _key_dirs[2] { { }, { } };
		Job                       _job         { };

		Key_directory &_get_unused_key_dir();

		Key_directory &_lookup_key_dir(Genode::uint32_t key_id);

		void _execute_decrypt_block(Job                       &job,
		                            Cbe::Crypto_plain_buffer  &plain_buf,
		                            Cbe::Crypto_cipher_buffer &cipher_buf,
		                            bool                      &progress);

		void _execute_encrypt_block(Job                       &job,
		                            Cbe::Crypto_plain_buffer  &plain_buf,
		                            Cbe::Crypto_cipher_buffer &cipher_buf,
		                            bool                      &progress);

	public:

		Crypto(Vfs::Env                          &env,
		       Genode::Xml_node            const &crypto,
		       Genode::Signal_context_capability  sigh);

		bool request_acceptable() const;

		Result add_key(Cbe::Key const &key);

		Result remove_key(Cbe::Key::Id key_id);

		void submit_request(Cbe::Request               const &request,
		                    Operation                         op,
		                    Cbe::Crypto_plain_buffer::Index   plain_buf_idx,
		                    Cbe::Crypto_cipher_buffer::Index  cipher_buf_idx);

		Cbe::Request peek_completed_encryption_request() const;

		Cbe::Request peek_completed_decryption_request() const;

		void drop_completed_request();

		void execute(Cbe::Crypto_plain_buffer  &plain_buf,
		             Cbe::Crypto_cipher_buffer &cipher_buf,
		             bool                      &progress);
};

#endif /* _CBE_TESTER__CRYPTO_H_ */
