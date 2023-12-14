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

namespace Tresor { class Trust_anchor; }

class Tresor::Trust_anchor : Noncopyable
{
	public:

		struct Attr
		{
			Vfs::Vfs_handle &decrypt_file;
			Vfs::Vfs_handle &encrypt_file;
			Vfs::Vfs_handle &generate_key_file;
			Vfs::Vfs_handle &initialize_file;
			Vfs::Vfs_handle &hash_file;
		};

	private:

		Attr const _attr;
		addr_t _user { };

	public:

		class Encrypt_key;
		class Decrypt_key;
		class Generate_key;
		class Initialize;
		class Read_hash;
		class Write_hash;

		Trust_anchor(Attr const &attr) : _attr(attr) { }

		template <typename REQ>
		bool execute(REQ &req)
		{
			if (!_user)
				_user = (addr_t)&req;

			if (_user != (addr_t)&req)
				return false;

			bool progress = req.execute(_attr);
			if (req.complete())
				_user = 0;

			return progress;
		}

		static constexpr char const *name() { return "trust_anchor"; }
};

class Tresor::Trust_anchor::Encrypt_key : Noncopyable
{
	public:

		using Module = Trust_anchor;

		struct Attr
		{
			Key_value &out_key_ciphertext;
			Key_value const &in_key_plaintext;
		};

	private:

		enum State { INIT, COMPLETE, WRITE, WRITE_OK, READ_OK, FILE_ERR };

		Request_helper<Encrypt_key, State> _helper;
		Attr const _attr;
		Constructible<File<State> > _file { };

	public:

		Encrypt_key(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "encrypt key"); }

		bool execute(Trust_anchor::Attr const &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Trust_anchor::Decrypt_key : Noncopyable
{
	public:

		using Module = Trust_anchor;

		struct Attr
		{
			Key_value &out_key_plaintext;
			Key_value const &in_key_ciphertext;
		};

	private:

		enum State { INIT, COMPLETE, WRITE, WRITE_OK, READ_OK, FILE_ERR };

		Request_helper<Decrypt_key, State> _helper;
		Attr const _attr;
		Constructible<File<State> > _file { };

	public:

		Decrypt_key(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "decrypt key"); }

		bool execute(Trust_anchor::Attr const &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Trust_anchor::Initialize : Noncopyable
{
	public:

		using Module = Trust_anchor;

		struct Attr { Passphrase const &in_passphrase; };

	private:

		enum State { INIT, COMPLETE, WRITE, WRITE_OK, READ_OK, FILE_ERR };

		Request_helper<Initialize, State> _helper;
		Attr const _attr;
		Constructible<File<State> > _file { };
		char _result_buf[3];

	public:

		Initialize(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "initialize"); }

		bool execute(Trust_anchor::Attr const &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Trust_anchor::Generate_key : Noncopyable
{
	public:

		using Module = Trust_anchor;

		struct Attr { Key_value &out_key_plaintext; };

	private:

		enum State { INIT, COMPLETE, READ, READ_OK, FILE_ERR };

		Request_helper<Generate_key, State> _helper;
		Attr const _attr;
		Constructible<File<State> > _file { };

	public:

		Generate_key(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "generate key"); }

		bool execute(Trust_anchor::Attr const &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Trust_anchor::Write_hash : Noncopyable
{
	public:

		using Module = Trust_anchor;

		struct Attr { Hash const &in_hash; };

	private:

		enum State { INIT, COMPLETE, WRITE, WRITE_OK, READ_OK, FILE_ERR };

		Request_helper<Write_hash, State> _helper;
		Attr const _attr;
		Constructible<File<State> > _file { };
		char _result_buf[3];

	public:

		Write_hash(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "write hash"); }

		bool execute(Trust_anchor::Attr const &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Trust_anchor::Read_hash : Noncopyable
{
	public:

		using Module = Trust_anchor;

		struct Attr { Hash &out_hash; };

	private:

		enum State { INIT, COMPLETE, READ, READ_OK, FILE_ERR };

		Request_helper<Read_hash, State> _helper;
		Attr const _attr;
		Constructible<File<State> > _file { };

	public:

		Read_hash(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "read hash"); }

		bool execute(Trust_anchor::Attr const &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

#endif /* _TRESOR__TRUST_ANCHOR_H_ */
