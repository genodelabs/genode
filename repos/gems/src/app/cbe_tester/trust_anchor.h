/*
 * \brief  Implementation of the TA module API using the TA VFS API
 * \author Martin Stein
 * \date   2020-10-29
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CBE_TESTER__TRUST_ANCHOR_H_
#define _CBE_TESTER__TRUST_ANCHOR_H_

/* CBE includes */
#include <cbe/types.h>

/* CBE tester includes */
#include <vfs_utilities.h>

class Trust_anchor
{
	private:

		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;
		using Operation = Cbe::Trust_anchor_request::Operation;

		enum Job_state
		{
			WRITE_PENDING,
			WRITE_IN_PROGRESS,
			READ_PENDING,
			READ_IN_PROGRESS,
			COMPLETE
		};

		struct Job
		{
			Cbe::Trust_anchor_request request              { };
			Job_state                 state                { Job_state::COMPLETE };
			Genode::String<64>        passphrase           { };
			Cbe::Hash                 hash                 { };
			Cbe::Key_plaintext_value  key_plaintext_value  { };
			Cbe::Key_ciphertext_value key_ciphertext_value { };
			Vfs::file_offset          fl_offset            { 0 };
			Vfs::file_size            fl_size              { 0 };
		};

		Vfs::Env                  &_vfs_env;
		char                       _read_buf[64];
		Vfs_io_response_handler    _handler;
		Genode::String<128> const  _path;
		Genode::String<128> const  _decrypt_path      { _path, "/decrypt" };
		Vfs::Vfs_handle           &_decrypt_file      { vfs_open_rw(_vfs_env, { _decrypt_path }) };
		Genode::String<128> const  _encrypt_path      { _path, "/encrypt" };
		Vfs::Vfs_handle           &_encrypt_file      { vfs_open_rw(_vfs_env, { _encrypt_path }) };
		Genode::String<128> const  _generate_key_path { _path, "/generate_key" };
		Vfs::Vfs_handle           &_generate_key_file { vfs_open_rw(_vfs_env, { _generate_key_path }) };
		Genode::String<128> const  _initialize_path   { _path, "/initialize" };
		Vfs::Vfs_handle           &_initialize_file   { vfs_open_rw(_vfs_env, { _initialize_path }) };
		Genode::String<128> const  _hashsum_path      { _path, "/hashsum" };
		Vfs::Vfs_handle           &_hashsum_file      { vfs_open_rw(_vfs_env, { _hashsum_path }) };
		Job                        _job               { };

		void _execute_write_read_operation(Vfs::Vfs_handle           &file,
		                                   Genode::String<128> const &file_path,
		                                   char                const *write_buf,
		                                   char                      *read_buf,
		                                   Vfs::file_size             read_size,
		                                   bool                      &progress);

		void _execute_write_operation(Vfs::Vfs_handle           &file,
		                              Genode::String<128> const &file_path,
		                              char                const *write_buf,
		                              bool                      &progress);

		void _execute_read_operation(Vfs::Vfs_handle           &file,
		                             Genode::String<128> const &file_path,
		                             char                      *read_buf,
		                             bool                      &progress);

	public:

		Trust_anchor(Vfs::Env                          &vfs_env,
		             Genode::Xml_node            const &xml_node,
		             Genode::Signal_context_capability  sigh);

		bool request_acceptable() const;

		void submit_request_passphrase(Cbe::Trust_anchor_request const &request,
		                               Genode::String<64>        const &passphrase);

		void
		submit_request_key_plaintext_value(Cbe::Trust_anchor_request const &request,
		                                   Cbe::Key_plaintext_value  const &key_plaintext_value);

		void
		submit_request_key_ciphertext_value(Cbe::Trust_anchor_request const &request,
		                                    Cbe::Key_ciphertext_value const &key_ciphertext_value);

		void submit_request_hash(Cbe::Trust_anchor_request const &request,
		                         Cbe::Hash                 const &hash);

		void submit_request(Cbe::Trust_anchor_request const &request);

		void execute(bool &progress);

		Cbe::Trust_anchor_request peek_completed_request() const;

		Cbe::Hash const &peek_completed_hash() const;

		Cbe::Key_plaintext_value const &
		peek_completed_key_plaintext_value() const;

		Cbe::Key_ciphertext_value const &
		peek_completed_key_ciphertext_value() const;

		void drop_completed_request();
};

#endif /* _CBE_TESTER__TRUST_ANCHOR_H_ */
