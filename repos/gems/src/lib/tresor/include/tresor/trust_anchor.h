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
#include <tresor/file.h>

namespace Tresor {

	class Trust_anchor;
	class Trust_anchor_request;
	class Trust_anchor_channel;
}

class Tresor::Trust_anchor_request : public Module_request
{
	friend class Trust_anchor_channel;

	public:

		enum Type { CREATE_KEY, ENCRYPT_KEY, DECRYPT_KEY, WRITE_HASH, READ_HASH, INITIALIZE };

	private:

		Type const _type;
		Key_value &_key_plaintext;
		Key_value &_key_ciphertext;
		Hash &_hash;
		Passphrase const _pass;
		bool &_success;

		NONCOPYABLE(Trust_anchor_request);

	public:

		Trust_anchor_request(Module_id src, Module_channel_id, Type, Key_value &, Key_value &, Hash &, Passphrase, bool &);

		static char const *type_to_string(Type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};

class Tresor::Trust_anchor_channel : public Module_channel
{
	private:

		using Request = Trust_anchor_request;

		enum State { REQ_SUBMITTED, REQ_COMPLETE, READ_OK, WRITE_OK, FILE_ERR  };

		State _state { REQ_COMPLETE };
		Vfs::Env &_vfs_env;
		char _result_buf[3];
		Tresor::Path const _path;
		Read_write_file<State> _decrypt_file { _state, _vfs_env, { _path, "/decrypt" } };
		Read_write_file<State> _encrypt_file { _state, _vfs_env, { _path, "/encrypt" } };
		Read_write_file<State> _generate_key_file { _state, _vfs_env, { _path, "/generate_key" } };
		Read_write_file<State> _initialize_file { _state, _vfs_env, { _path, "/initialize" } };
		Read_write_file<State> _hashsum_file { _state, _vfs_env, { _path, "/hashsum" } };
		Trust_anchor_request *_req_ptr { nullptr };

		NONCOPYABLE(Trust_anchor_channel);

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _create_key(bool &);

		void _read_hash(bool &);

		void _initialize(bool &);

		void _write_hash(bool &);

		void _encrypt_key(bool &);

		void _decrypt_key(bool &);

		void _mark_req_failed(bool &, Error_string);

		void _mark_req_successful(bool &);

	public:

		void execute(bool &);

		Trust_anchor_channel(Module_channel_id, Vfs::Env &, Xml_node const &);
};

class Tresor::Trust_anchor : public Module
{
	private:

		using Request = Trust_anchor_request;
		using Channel = Trust_anchor_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Trust_anchor);

	public:

		struct Create_key : Request
		{
			Create_key(Module_id m, Module_channel_id c, Key_value &k, bool &s)
			: Request(m, c, Request::CREATE_KEY, k, *(Key_value*)0, *(Hash*)0, Passphrase(), s) { }
		};

		struct Encrypt_key : Request
		{
			Encrypt_key(Module_id m, Module_channel_id c, Key_value const &kp, Key_value &kc, bool &s)
			: Request(m, c, Request::ENCRYPT_KEY, *const_cast<Key_value*>(&kp), kc, *(Hash*)0, Passphrase(), s) { }
		};

		struct Decrypt_key : Request
		{
			Decrypt_key(Module_id m, Module_channel_id c, Key_value &kp, Key_value const &kc, bool &s)
			: Request(m, c, Request::DECRYPT_KEY, kp, *const_cast<Key_value*>(&kc), *(Hash*)0, Passphrase(), s) { }
		};

		struct Write_hash : Request
		{
			Write_hash(Module_id m, Module_channel_id c, Hash const &h, bool &s)
			: Request(m, c, Request::WRITE_HASH, *(Key_value*)0, *(Key_value*)0, *const_cast<Hash*>(&h), Passphrase(), s) { }
		};

		struct Read_hash : Request
		{
			Read_hash(Module_id m, Module_channel_id c, Hash &h, bool &s)
			: Request(m, c, Request::READ_HASH, *(Key_value*)0, *(Key_value*)0, h, Passphrase(), s) { }
		};

		struct Initialize : Request
		{
			Initialize(Module_id src_mod, Module_channel_id src_chan, Passphrase pass, bool &succ)
			: Request(src_mod, src_chan, Request::INITIALIZE, *(Key_value*)0, *(Key_value*)0, *(Hash*)0, pass, succ) { }
		};

		Trust_anchor(Vfs::Env &, Xml_node const &);

		void execute(bool &) override;
};

#endif /* _TRESOR__TRUST_ANCHOR_H_ */
