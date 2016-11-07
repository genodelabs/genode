/*
 * \brief  ISO 9660 file system support
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2010-07-26
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/exception.h>
#include <base/log.h>
#include <base/stdint.h>
#include <block_session/connection.h>
#include <util/misc_math.h>
#include <util/token.h>

#include "iso9660.h"

using namespace Genode;

namespace Iso {

	/*
	 *  Sector reads one or more packets from the block interface
	 */
	class Sector {

		public:

			enum {
				MAX_SECTORS = 32, /* max. number sectors that can be read in one
				                     transaction */
			};

			static Block::Connection          *_blk;
			static Block::Session::Tx::Source *_source;
			static size_t                      _blk_size;
			static Lock                        _lock;

		private:

			Block::Packet_descriptor _p;

		public:

			Sector(unsigned long blk_nr, unsigned long count)
			{
				Lock::Guard lock_guard(_lock);

				try {
					_p = Block::Packet_descriptor(
						_blk->dma_alloc_packet(blk_size() * count),
						Block::Packet_descriptor::READ,
						blk_nr * ((float)blk_size() / _blk_size),
						count * ((float)blk_size() / _blk_size));
					_source->submit_packet(_p);
					_p = _source->get_acked_packet();

					if (!_p.succeeded()) {
						Genode::error("Could not read block ", blk_nr);
						throw Io_error();
					}

				} catch (Block::Session::Tx::Source::Packet_alloc_failed) {
					Genode::error("packet overrun!");
					_p = _source->get_acked_packet();
					throw Io_error();
				}
			}

			~Sector() { 
				Lock::Guard lock_guard(_lock);
				_source->release_packet(_p);
			}

			/**
			 * Return address of packet content
			 */
			template <typename T>
			T addr() { return reinterpret_cast<T>(_source->packet_content(_p)); }

			static size_t blk_size() { return 2048; }

			static unsigned long to_blk(unsigned long bytes) {
				return ((bytes + blk_size() - 1) & ~(blk_size() - 1)) / blk_size(); }
	};

	/**
	 * Rock ridge extension (see IEEE P1282)
	 */
	class Rock_ridge
	{
		public:

			enum {
				NM = 0x4d4e, /* POSIX name system use entry (little endian) */
			};

		private:

			uint16_t _signature;
			uint8_t  _length;
			uint8_t  _version;
			uint8_t  _flags;
			char _name;

		public:

			char   *name()   { return &_name; }
			uint8_t length() { return _length - 5; }

			static Rock_ridge* scan_name(uint8_t *ptr, uint8_t size)
			{
				Rock_ridge *rr = (Rock_ridge *)ptr;

				while (rr->_length && ((uint8_t *)rr < ptr + size - 4)) {
					if (rr->_signature == NM)
						return rr;
					rr = rr->next();
				}

				return 0;
			}

			Rock_ridge *next() { return (Rock_ridge *)((uint8_t *)this + _length); }
	};


	/*********************
	 ** ISO9660 classes **
	 *********************/

	/**
	 * Access memory using offsets
	 */
	class Iso_base
	{
		protected:

			template <typename T>
			T value(int offset) { return *((T *)(this + offset)); }

			template <typename T>
			T ptr(int offset) { return (T)(this + offset); }
	};


	/**
	 * Class representing a directory descriptions (see ECMA 119)
	 */
	class Directory_record : public Iso_base
	{
		enum {
			TABLE_LENGTH = 33,  /* fixed length of directory record */

			ROOT_DIR     = 0x0, /* special names of root and parent directories */
			PARENT_DIR   = 0x1,

			DIR_FLAG     = 0x2, /* directory flag in attributes */
		};

		public:

			/* total length of this record */
			uint8_t  record_length() { return value<uint8_t>(0); }

			/* starting block of extent */
			uint32_t blk_nr() { return value<uint32_t>(2); }

			/* length in bytes of extent */
			uint32_t data_length() { return value<uint32_t>(10); }

			/* attributes */
			uint8_t  file_flags() { return value<uint8_t>(25); }

			/* length of file name */
			uint8_t   file_name_length() { return value<uint8_t>(32); }

			/* retrieve the file name */
			void file_name(char *buf)
			{
				buf[0] = 0;

				/* try Rock Ridge name */
				Rock_ridge *rr = Rock_ridge::scan_name(system_use(),
				                                       system_use_size());

				if (rr) {
					memcpy(buf, rr->name(), rr->length());
					buf[rr->length()] = 0;
					return;
				}

				/* retrieve iso name */
				char *name = ptr<char*>(33);

				/*
				 * Check for root and parent directory names and modify them
				 * to '.' and '..' respectively.
				 */
				if (file_name_length() == 1)

					switch (name[0]) {

					case PARENT_DIR:

						buf[2] = 0;
						buf[1] = '.';
						buf[0] = '.';
						return;

					case ROOT_DIR:

						buf[1] = 0;
						buf[0] = '.';
						return;
					}

				memcpy(buf, name, file_name_length());
				buf[file_name_length()] = 0;
			}

			/* pad byte after file name (if file name length is even, only) */
			uint8_t pad_byte() { return !(file_name_length() % 2) ? 1 : 0; }

			/* system use area */
			uint8_t *system_use()
			{
				return (uint8_t*)this + TABLE_LENGTH
				        + file_name_length() + pad_byte();
			}

			/* length in bytes of system use area */
			uint8_t system_use_size()
			{
				return record_length() - file_name_length()
				       - TABLE_LENGTH - pad_byte();
			}

			/* retrieve next record */
			Directory_record *next()
			{
				Directory_record *_next = this + record_length();

				if (_next->record_length())
					return _next;

				return 0;
			}

			/* find a directory record with file name matching 'level' */
			Directory_record *locate(char *level)
			{
				Directory_record *dir = this;

				while (dir) {

					char name[LEVEL_LENGTH];
					dir->file_name(name);

					if (!strcmp(name, level))
						return dir;

					dir = dir->next();
				}

				return 0;
			}

			/* describes this record a directory */
			bool directory() { return file_flags() & DIR_FLAG; }
	};


	/**
	 * Volume descriptor (see ECMA 119)
	 */
	class Volume_descriptor : public Iso_base
	{
		enum {
			/* volume types */
			PRIMARY    = 0x01, /* type of primary volume descriptor */
			TERMINATOR = 0xff, /* type of terminating descriptor */

			/* constants */
			ROOT_SIZE  = 34, /* the root directory record has a fixed length */
		};

		public:

			/* descriptor type */
			uint8_t type() { return value<uint8_t>(0); }

			/* root directory record */
			Directory_record * root_record() { return ptr<Directory_record *>(156); }

			/* check for primary descriptor */
			bool primary() { return type() == PRIMARY; }

			/* check for terminating descriptor */
			bool terminator() { return type() == TERMINATOR; }

			/* copy the root record */
			Directory_record *copy_root_record()
			{
				Directory_record *buf;

				if (!(env()->heap()->alloc(ROOT_SIZE, &buf)))
					throw Root::Quota_exceeded();

				memcpy(buf, root_record(), ROOT_SIZE);

				return buf;
			}
	};


	/**
	 * Locate the root-directory record in the primary volume descriptor
	 */
	Directory_record *locate_root()
	{
		/* volume descriptors in ISO9660 start at block 16 */
		for (unsigned long blk_nr = 16;; blk_nr++) {
			Sector sec(blk_nr, 1);
			Volume_descriptor *vol = sec.addr<Volume_descriptor *>();

			if (vol->primary())
				return vol->copy_root_record();

			if (vol->terminator())
				return 0;
		}
	}


	/**
	 * Return root directory record
	 */
	Directory_record *root_dir()
	{
		static Directory_record *root = locate_root();

		if (!root)
			throw Non_data_disc();

		return root;
	}


	unsigned long read_file(File_info *info, off_t file_offset, uint32_t length, void *buf_ptr)
	{
		uint8_t *buf = (uint8_t *)buf_ptr;
		if (info->size() <= (size_t)(length + file_offset))
			length = info->size() - file_offset - 1;

		unsigned long total_blk_count = ((length + (Sector::blk_size() - 1)) &
		                                 ~((Sector::blk_size()) - 1)) / Sector::blk_size();
		unsigned long ret = total_blk_count;
		unsigned long blk_count;
		unsigned long blk_nr = info->blk_nr() + (file_offset / Sector::blk_size());

		while ((blk_count = min<unsigned long>(Sector::MAX_SECTORS, total_blk_count))) {
			Sector sec(blk_nr, blk_count);

			total_blk_count -= blk_count;
			blk_nr          += blk_count;

			unsigned long copy_length = blk_count * Sector::blk_size();
			memcpy(buf, sec.addr<void *>(), copy_length);

			length -= copy_length;
			buf    += copy_length;

			/* zero out rest of page */
			if (!total_blk_count && (blk_count % 2))
				memset(buf, 0, Sector::blk_size());
		}
	
		return ret * Sector::blk_size();
	}


	struct Scanner_policy_file
	{
		static bool identifier_char(char c, unsigned /* i */)
		{
			return c != '/' && c != 0;
		}
	};

	typedef ::Genode::Token<Scanner_policy_file> Token;


	/**
	 * ISO interface
	 */
	File_info *file_info(char *path)
	{
		char level[PATH_LENGTH];

		Token t(path);
		Directory_record *dir = root_dir();
		uint32_t blk_nr = 0, data_length = 0;

		/* determine block nr and file length on disk, parse directory records */
		while (t) {

			if (t.type() != Token::IDENT) {
				t = t.next();
				continue;
			}

			t.string(level, PATH_LENGTH);

			/*
			 * Save current block number in a variable because successive
			 * iterations might override the memory location where dir points
			 * to when a directory entry spans several sectors.
			 */
			uint32_t current_blk_nr = dir->blk_nr();

			/* load extent of directory record and search for level */
			for (unsigned long i = 0; i < Sector::to_blk(dir->data_length()); i++) {
				Sector sec(current_blk_nr + i, 1);
				Directory_record *tmp = sec.addr<Directory_record *>()->locate(level);

				if (!tmp && i == Sector::to_blk(dir->data_length()) - 1) {
					Genode::error("file not found: ", Genode::Cstring(path));
					throw File_not_found();
				}

				if (!tmp) continue;

				dir = tmp;
				current_blk_nr = dir->blk_nr();

				if (!dir->directory()) {
					blk_nr      = current_blk_nr;
					data_length = dir->data_length();
				}

				break;
			}

			t = t.next();
		}

		/* Warning: Don't access 'dir' after this point, since the sector is gone */

		if (!blk_nr && !data_length) {
			Genode::error("file not found: ", Genode::Cstring(path));
			throw File_not_found();
		}

		return new(env()->heap()) File_info(blk_nr, data_length);
	}


	/*
	 * Static members of Sector
	 */
	Block::Connection *Sector::_blk;
	Block::Session::Tx::Source *Sector::_source;
	size_t Sector::_blk_size;
	Lock Sector::_lock;


	/**
	 * Initialize globals
	 */
	void __attribute__((constructor)) init()
	{
		static Allocator_avl block_alloc(env()->heap());
		static Block::Connection _blk(&block_alloc);

		Sector::_blk    = &_blk;
		Sector::_source  = _blk.tx();

		Block::sector_t blk_cnt = 0;
		Block::Session::Operations ops;
		_blk.info(&blk_cnt, &Sector::_blk_size, &ops);
	}
} /* end of namespace Iso */
