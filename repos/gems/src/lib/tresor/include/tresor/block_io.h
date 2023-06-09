/*
 * \brief  Module for accessing the back-end block device
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__BLOCK_IO_H_
#define _TRESOR__BLOCK_IO_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/module.h>
#include <tresor/vfs_utilities.h>

namespace Tresor {

	class Block_io;
	class Block_io_request;
	class Block_io_channel;
}

class Tresor::Block_io_request : public Module_request
{
	public:

		enum Type {
			INVALID = 0, READ = 1, WRITE = 2, SYNC = 3, READ_CLIENT_DATA = 4,
			WRITE_CLIENT_DATA = 5 };

	private:

		friend class Block_io;
		friend class Block_io_channel;

		Type     _type              { INVALID };
		uint64_t _client_req_offset { 0 };
		uint64_t _client_req_tag    { 0 };
		uint32_t _key_id            { 0 };
		uint64_t _pba               { 0 };
		uint64_t _vba               { 0 };
		uint64_t _blk_count         { 0 };
		addr_t   _blk_ptr           { 0 };
		addr_t   _hash_ptr          { 0 };
		bool     _success           { false };

	public:

		Block_io_request() { }

		Block_io_request(uint64_t  src_module_id,
		                 uint64_t  src_request_id,
		                 size_t    req_type,
		                 uint64_t  client_req_offset,
		                 uint64_t  client_req_tag,
		                 uint32_t  key_id,
		                 uint64_t  pba,
		                 uint64_t  vba,
		                 uint64_t  blk_count,
		                 void     *blk_ptr,
		                 void     *hash_ptr);

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);

		char const *type_name() const { return type_to_string(_type); }


		/********************
		 ** Module_request **
		 ********************/

		void print(Output &out) const override;
};

class Tresor::Block_io_channel
{
	private:

		friend class Block_io;

		enum State {
			INACTIVE, SUBMITTED, PENDING, IN_PROGRESS, COMPLETE,
			ENCRYPT_CLIENT_DATA_PENDING,
			ENCRYPT_CLIENT_DATA_IN_PROGRESS,
			ENCRYPT_CLIENT_DATA_COMPLETE,
			DECRYPT_CLIENT_DATA_PENDING,
			DECRYPT_CLIENT_DATA_IN_PROGRESS,
			DECRYPT_CLIENT_DATA_COMPLETE
		};

		State            _state                 { INACTIVE };
		Block_io_request _request               { };
		Vfs::file_offset _nr_of_processed_bytes { 0 };
		size_t           _nr_of_remaining_bytes { 0 };
		Block            _blk_buf               { };
		bool             _generated_req_success { false };
};

class Tresor::Block_io : public Module
{
	private:

		using Request = Block_io_request;
		using Channel = Block_io_channel;
		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;
		using file_size = Vfs::file_size;
		using file_offset = Vfs::file_offset;

		enum { NR_OF_CHANNELS = 1 };

		String<32> const  _path;
		Vfs::Env         &_vfs_env;
		Vfs::Vfs_handle  &_vfs_handle               { vfs_open_rw(_vfs_env, _path) };
		Channel           _channels[NR_OF_CHANNELS] { };

		void _execute_read(Channel &channel,
		                   bool    &progress);

		void _execute_write(Channel &channel,
		                    bool    &progress);

		void _execute_read_client_data(Channel &channel,
		                               bool    &progress);

		void _execute_write_client_data(Channel &channel,
		                                bool    &progress);

		void _execute_sync(Channel &channel,
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

		Block_io(Vfs::Env       &vfs_env,
		         Xml_node const &xml_node);
};

#endif /* _TRESOR__BLOCK_IO_H_ */
