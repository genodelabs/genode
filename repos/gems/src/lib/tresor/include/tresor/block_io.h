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
#include <tresor/file.h>

namespace Tresor {

	class Block_io;
	class Block_io_request;
	class Block_io_channel;
}

class Tresor::Block_io_request : public Module_request
{
	friend class Block_io_channel;

	public:

		enum Type { READ, WRITE, SYNC, READ_CLIENT_DATA, WRITE_CLIENT_DATA };

	private:

		Type const _type;
		Request_offset const _client_req_offset;
		Request_tag const _client_req_tag;
		Key_id const _key_id;
		Physical_block_address const _pba;
		Virtual_block_address const _vba;
		Block &_blk;
		Hash &_hash;
		bool &_success;

		NONCOPYABLE(Block_io_request);

	public:

		Block_io_request(Module_id, Module_channel_id, Type, Request_offset, Request_tag, Key_id,
		                 Physical_block_address, Virtual_block_address, Block &, Hash &, bool &);

		static char const *type_to_string(Type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type), " pba ", _pba); }
};

class Tresor::Block_io_channel : public Module_channel
{
	private:

		using Request = Block_io_request;

		enum State {
			REQ_SUBMITTED, REQ_COMPLETE, CIPHERTEXT_BLK_OBTAINED, PLAINTEXT_BLK_SUPPLIED, REQ_GENERATED,
			READ_OK, WRITE_OK, SYNC_OK, FILE_ERR };

		State _state { REQ_COMPLETE };
		Block _blk { };
		bool _generated_req_success { false };
		Block_io_request *_req_ptr { };
		Vfs::Env &_vfs_env;
		Tresor::Path const _path;
		Read_write_file<State> _file { _state, _vfs_env, _path };

		NONCOPYABLE(Block_io_channel);

		void _generated_req_completed(State_uint) override;

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _read(bool &);

		void _write(bool &);

		void _read_client_data(bool &);

		void _write_client_data(bool &);

		void _sync(bool &);

		void _mark_req_failed(bool &, Error_string);

		void _mark_req_successful(bool &);

	public:

		Block_io_channel(Module_channel_id, Vfs::Env &, Xml_node const &);

		void execute(bool &);
};

class Tresor::Block_io : public Module
{
	private:

		using Request = Block_io_request;
		using Channel = Block_io_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Block_io);

	public:

		struct Read : Request
		{
			Read(Module_id m, Module_channel_id c, Physical_block_address a, Block &b, bool &s)
			: Request(m, c, Request::READ, 0, 0, 0, a, 0, b, *(Hash*)0, s) { }
		};

		struct Write : Request
		{
			Write(Module_id m, Module_channel_id c, Physical_block_address a, Block const &b, bool &s)
			: Request(m, c, Request::WRITE, 0, 0, 0, a, 0, *const_cast<Block*>(&b), *(Hash*)0, s) { }
		};

		struct Sync : Request
		{
			Sync(Module_id m, Module_channel_id c, bool &s)
			: Request(m, c, Request::SYNC, 0, 0, 0, 0, 0, *(Block*)0, *(Hash*)0, s) { }
		};

		struct Write_client_data : Request
		{
			Write_client_data(Module_id m, Module_channel_id c, Physical_block_address p, Virtual_block_address v,
			                  Key_id k, Request_tag t, Request_offset o, Hash &h, bool &s)
			: Request(m, c, Request::WRITE_CLIENT_DATA, o, t, k, p, v, *(Block*)0, h, s) { }
		};

		struct Read_client_data : Request
		{
			Read_client_data(Module_id m, Module_channel_id c, Physical_block_address p, Virtual_block_address v,
			                  Key_id k, Request_tag t, Request_offset o, bool &s)
			: Request(m, c, Request::READ_CLIENT_DATA, o, t, k, p, v, *(Block*)0, *(Hash*)0, s) { }
		};

		Block_io(Vfs::Env &, Xml_node const &);

		void execute(bool &) override;
};

#endif /* _TRESOR__BLOCK_IO_H_ */
