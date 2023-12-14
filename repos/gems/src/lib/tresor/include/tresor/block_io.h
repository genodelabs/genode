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

namespace Tresor { class Block_io; }

class Tresor::Block_io : Noncopyable
{
	private:

		Vfs::Vfs_handle &_file;
		addr_t _user { };

	public:

		class Read;
		class Write;
		class Sync;

		Block_io(Vfs::Vfs_handle &file) : _file(file) { }

		template <typename REQ>
		bool execute(REQ &req)
		{
			if (!_user)
				_user = (addr_t)&req;

			if (_user != (addr_t)&req)
				return false;

			bool progress = req.execute(_file);
			if (req.complete())
				_user = 0;

			return progress;
		}

		static constexpr char const *name() { return "block_io"; }
};

class Tresor::Block_io::Read : Noncopyable
{
	public:

		using Module = Block_io;

		struct Attr
		{
			Physical_block_address const in_pba;
			Block &out_block;
		};

	private:

		enum State { INIT, COMPLETE, READ, READ_OK, FILE_ERR };

		Request_helper<Read, State> _helper;
		Attr const _attr;
		Constructible<File<State> > _file { };

	public:

		Read(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "read pba ", _attr.in_pba); }

		bool execute(Vfs::Vfs_handle &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Block_io::Write : Noncopyable
{
	public:

		using Module = Block_io;

		struct Attr
		{
			Physical_block_address const in_pba;
			Block const &in_block;
		};

	private:

		enum State { INIT, COMPLETE, WRITE, WRITE_OK, FILE_ERR };

		Request_helper<Write, State> _helper;
		Attr const _attr;
		Constructible<File<State> > _file { };

	public:

		Write(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "write pba ", _attr.in_pba); }

		bool execute(Vfs::Vfs_handle &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Block_io::Sync : Noncopyable
{
	public:

		using Module = Block_io;

		struct Attr { };

	private:

		enum State { INIT, COMPLETE, SYNC, SYNC_OK, FILE_ERR };

		Request_helper<Sync, State> _helper;
		Attr const _attr;
		Constructible<File<State> > _file { };

	public:

		Sync(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "sync"); }

		bool execute(Vfs::Vfs_handle &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

#endif /* _TRESOR__BLOCK_IO_H_ */
