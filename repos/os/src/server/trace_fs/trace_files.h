/*
 * \brief  Trace files
 * \author Josef Soentgen
 * \date   2014-01-22
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TRACE_FILES_H_
#define _TRACE_FILES_H_

/* Genode includes */
#include <util/string.h>

/* local includes */
#include <file.h>


namespace File_system {

	/**
	 * The State_file is a stateful file that is used to implement
	 * files in the file system, which may trigger a action in the
	 * file system backend.
	 */

	class State_file : public File,
	                   public Changeable_content
	{
		protected:

			bool _state;


		public:

			State_file(char const *name)
			: File(name), _state(false) { }

			bool state() const { return _state; }


			/********************
			 ** Node interface **
			 ********************/

			virtual size_t read(char *dst, size_t len, seek_off_t seek_offset)
			{
				/* limit len */
				if (len > 2)
					len = 2;

				switch (len) {
				case 2:
					dst[1] = '\n';
				case 1:
					dst[0] = 0x30 + (char)_state;
					break;
				default:
					/* zero length is useless */
					break;
				}

				return len;
			}

			virtual size_t write(char const *src, size_t len, seek_off_t seek_offset)
			{
				char buf[32];
				if (len >= sizeof buf)
					return 0;

				using namespace Genode;

				strncpy(buf, src, min(len + 1, sizeof (buf)));

				/**
				 * For now, we only check the leading digit and do not care
				 * about the rest.
				 */
				if (!strcmp(buf, "1", 1)) {
					_state = true;
				}
				else if (!strcmp(buf, "0", 1)) {
					_state = false;
				} else {
					/* silently ignore bogus writes */
					return 0;
				}

				Changeable_content::_changed = true;

				return len;
			}

			Status status() const
			{
				Status s;

				s.inode = inode();
				s.size  = 2;
				s.mode  = File_system::Status::MODE_FILE;

				return s;
			}

			/********************
			 ** File interface **
			 ********************/

			file_size_t length() const { return 2; }
			void truncate(file_size_t size) { }
	};


	/**
	 * The Active_file node shows the state of the tracing
	 */

	class Active_file : public State_file
	{
		private:

			Genode::Trace::Subject_id &_id;
			bool                       _active;


		public:

			Active_file(Genode::Trace::Subject_id &id)
			: State_file("active"), _id(id) { }

			Genode::Trace::Subject_id& id() const { return _id; }

			bool active() const { return State_file::state(); }

			void set_active() { _state = true; }
			void set_inactive() { _state = false; }
	};


	/**
	 * The Cleanup_file is used to trigger the removal of files used by
	 * the traced subject and to free utilized memory.
	 */

	class Cleanup_file : public State_file
	{
		private:

			Genode::Trace::Subject_id &_id;


		public:

			Cleanup_file(Genode::Trace::Subject_id &id)
			: State_file("cleanup"), _id(id) { }

			Genode::Trace::Subject_id& id() const { return _id; }

			bool cleanup() const { return State_file::state(); }
	};


	/**
	 * The Enable_file is used to initiate the tracing process
	 */

	class Enable_file : public State_file
	{
		private:

			Genode::Trace::Subject_id &_id;


		public:

			Enable_file(Genode::Trace::Subject_id &id)
			: State_file("enable"), _id(id) { }

			Genode::Trace::Subject_id& id() const { return _id; }

			bool enabled() const { return State_file::state(); }
	};


	/**
	 * The Events_file encapsulates the trace buffer of traced thread
	 */

	class Events_file : public Buffered_file
	{
		private:

			Genode::Trace::Subject_id &_id;


		public:

			Events_file(Genode::Trace::Subject_id &id,
			            Allocator &md_alloc)
			: Buffered_file(md_alloc, "events"), _id(id) { }

			Genode::Trace::Subject_id id() const { return _id; }

			size_t append(char const *src, size_t len)
			{
				Buffered_file::write(src, len, length());

				return len;
			}

			/********************
			 ** File interface **
			 ********************/

			/* override to prevent the user from overriding the file */
			size_t write(char const *src, size_t len, seek_off_t seek_offset) { return 0; }
			void truncate(file_size_t size) { }
	};


	/**
	 * This file contains the size of the trace buffer
	 */

	class Buffer_size_file : public File,
	                         public Changeable_content
	{
		private:

			file_size_t    _length;
			unsigned long  _size_limit;
			unsigned long  _size;

			char           _content[32];
			Genode::size_t _content_filled;

			/**
			 * Check if new size honors the size limit
			 *
			 * \param size new size of the buffer
			 * \return size limit if new size is greater, otherwise new size
			 */
			size_t _check_size_limit(size_t size)
			{
				if (size > _size_limit)
					return _size_limit;
				else
					return size;
			}

			/**
			 * Evalute the current content of the buffer
			 */
			void _refresh_content()
			{
				unsigned long tmp = 0;

				_content[_content_filled - 1] = '\0';
				_content_filled = 0;

				_length = Genode::strlen(_content);

				/* account for \n when reading from the file */
				_length += 1;

				ascii_to(_content, tmp);

				_size = _check_size_limit(tmp);
			}


		public:

			/**
			 * Constructor
			 */
			Buffer_size_file() : File("buffer_size"), _size_limit(0), _size(0) { }

			/**
			 * Return current size of the trace buffer
			 */
			unsigned long size() const { return _size; }

			/**
			 * Set current size of the trace buffer
			 */
			void size(unsigned long size)
			{
				_size = _check_size_limit(size);

				/* update file content */
				_length = Genode::snprintf(_content, sizeof (_content), "%lu", _size);

				/* account for \n when reading from the file */
				_length += 1;
			}

			/**
			 * Set max size of a trace buffer
			 */
			void size_limit(unsigned long limit) { _size_limit = limit; }

			/**
			 * Return maximal size of the trace buffer
			 */
			unsigned long size_limit() const { return _size_limit; }


			/********************
			 ** Node interface **
			 ********************/

			/**
			 * Read current maximal size of the trace buffer
			 */
			size_t read(char *dst, size_t len, seek_off_t seek_offset)
			{
				if (len > 32) {
					Genode::error("len:'", len, "' to small");
					return 0;
				}

				char buf[32];
				Genode::snprintf(buf, sizeof (buf), "%lu\n", _size);
				memcpy(dst, buf, len);

				return len;
			}

			/**
			 * Write new current maximal size of the trace buffer
			 */
			size_t write(char const *src, size_t len, seek_off_t seek_offset)
			{
				if ((_content_filled + len) >  sizeof (_content))
					return 0;

				Genode::memcpy(_content + _content_filled, src, len);
				_content_filled += len;

				Changeable_content::_changed = true;

				return len;
			}

			Status status() const
			{
				Status s;

				s.inode = inode();
				s.size  = _length;
				s.mode  = File_system::Status::MODE_FILE;

				return s;
			}


			/********************
			 ** File interface **
			 ********************/

			file_size_t length() const { return _length; }

			void truncate(file_size_t size) { }
	};


	/**
	 * Policy file
	 */

	class Policy_file : public Buffered_file,
	                    public Changeable_content
	{
		private:

			Genode::Trace::Subject_id &_id;
			file_size_t                _length;


		public:

			Policy_file(Genode::Trace::Subject_id &id,
			            Genode::Allocator &md_alloc)
			: Buffered_file(md_alloc, "policy"), _id(id) { }

			Genode::Trace::Subject_id& id() const { return _id; }


			/********************
			 ** Node interface **
			 ********************/

			size_t read(char *dst, size_t len, seek_off_t seek_offset)
			{
				return Buffered_file::read(dst, len, seek_offset);
			}

			size_t write(char const *src, size_t len, seek_off_t seek_offset)
			{
				size_t written = Buffered_file::write(src, len, seek_offset);

				if (written > 0)
					_changed = true;

				return written;
			}


			/********************
			 ** File interface **
			 ********************/

			void truncate(file_size_t size)
			{
				Buffered_file::truncate(size);

				_changed = true;
			}

	};

}

#endif /* _TRACE_FILES_H_ */
