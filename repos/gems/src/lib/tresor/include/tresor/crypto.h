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
	class Crypto_key_files_interface;
}

struct Tresor::Crypto_key_files_interface : Interface
{
	virtual void add_crypto_key(Key_id) = 0;

	virtual void remove_crypto_key(Key_id) = 0;

	virtual Vfs::Vfs_handle &encrypt_file(Key_id) = 0;

	virtual Vfs::Vfs_handle &decrypt_file(Key_id) = 0;
};

class Tresor::Crypto : Noncopyable
{
	public:

		struct Attr
		{
			Crypto_key_files_interface &key_files;
			Vfs::Vfs_handle &add_key_file;
			Vfs::Vfs_handle &remove_key_file;
		};

	private:

		Attr const _attr;
		addr_t _user { };

	public:

		class Encrypt;
		class Decrypt;
		class Add_key;
		class Remove_key;

		Crypto(Attr const &attr) : _attr(attr) { }

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

		static constexpr char const *name() { return "crypto"; }
};

class Tresor::Crypto::Encrypt : Noncopyable
{
	public:

		using Module = Crypto;

		struct Attr
		{
			Key_id const in_key_id;
			Physical_block_address const in_pba;
			Block &in_out_blk;
		};

	private:

		enum State { INIT, COMPLETE, WRITE, WRITE_OK, READ_OK, FILE_ERR };

		Request_helper<Encrypt, State> _helper;
		Attr const _attr;
		off_t _offset { };
		Constructible<File<State> > _file { };

	public:

		Encrypt(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "encrypt pba ", _attr.in_pba); }

		bool execute(Crypto::Attr const &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Crypto::Decrypt : Noncopyable
{
	public:

		using Module = Crypto;

		struct Attr
		{
			Key_id const in_key_id;
			Physical_block_address const in_pba;
			Block &in_out_blk;
		};

	private:

		enum State { INIT, COMPLETE, WRITE, WRITE_OK, READ_OK, FILE_ERR };

		Request_helper<Decrypt, State> _helper;
		Attr const _attr;
		off_t _offset { };
		Constructible<File<State> > _file { };

	public:

		Decrypt(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "decrypt pba ", _attr.in_pba); }

		bool execute(Crypto::Attr const &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Crypto::Add_key : Noncopyable
{
	public:

		using Module = Crypto;

		struct Attr { Key const &in_key; };

	private:

		enum State { INIT, COMPLETE, WRITE, WRITE_OK, FILE_ERR };

		Request_helper<Add_key, State> _helper;
		Attr const _attr;
		char _write_buf[sizeof(Key_id) + sizeof(Key_value)] { };
		Constructible<File<State> > _file { };

	public:

		Add_key(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "add key id ", _attr.in_key.id); }

		bool execute(Crypto::Attr const &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Crypto::Remove_key : Noncopyable
{
	public:

		using Module = Crypto;

		struct Attr { Key_id const in_key_id; };

	private:

		enum State { INIT, COMPLETE, WRITE, WRITE_OK, FILE_ERR };

		Request_helper<Remove_key, State> _helper;
		Attr const _attr;
		Constructible<File<State> > _file { };

	public:

		Remove_key(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "remove key id ", _attr.in_key_id); }

		bool execute(Crypto::Attr const &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

#endif /* _TRESOR__CRYPTO_H_ */
