/*
 * \brief  I/O channels for pipe input/output
 * \author Norman Feske
 * \date   2012-03-19
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__PIPE_IO_CHANNEL_H_
#define _NOUX__PIPE_IO_CHANNEL_H_

/* Noux includes */
#include <io_channel.h>

namespace Noux {
	class Pipe;
	class Pipe_sink_io_channel;
	class Pipe_source_io_channel;
}


class Noux::Pipe : public Reference_counter
{
	private:

		Lock mutable _lock;

		enum { BUFFER_SIZE = 4096 };
		char _buffer[BUFFER_SIZE];

		unsigned _read_offset;
		unsigned _write_offset;

		Signal_context_capability _read_ready_sigh;
		Signal_context_capability _write_ready_sigh;

		bool _writer_is_gone;

		/**
		 * Return space available in the buffer for writing, in bytes
		 */
		size_t _avail_buffer_space() const
		{
			if (_read_offset < _write_offset)
				return (BUFFER_SIZE - _write_offset) + _read_offset - 1;

			if (_read_offset > _write_offset)
				return _read_offset - _write_offset - 1;

			/* _read_offset == _write_offset */
			return BUFFER_SIZE - 1;
		}

		bool _any_space_avail_for_writing() const
		{
			return _avail_buffer_space() > 0;;
		}

		void _wake_up_reader()
		{
			if (_read_ready_sigh.valid())
				Signal_transmitter(_read_ready_sigh).submit();
		}

		void _wake_up_writer()
		{
			if (_write_ready_sigh.valid())
				Signal_transmitter(_write_ready_sigh).submit();
		}

	public:

		Pipe()
		: _read_offset(0), _write_offset(0), _writer_is_gone(false) { }

		~Pipe()
		{
			Lock::Guard guard(_lock);
		}

		void writer_close()
		{
			Lock::Guard guard(_lock);

			_writer_is_gone = true;
			_write_ready_sigh = Signal_context_capability();
			_wake_up_reader();
		}

		void reader_close()
		{
			Lock::Guard guard(_lock);
			_read_ready_sigh = Signal_context_capability();
		}

		bool writer_is_gone() const
		{
			Lock::Guard guard(_lock);
			return _writer_is_gone;
		}

		bool any_space_avail_for_writing() const
		{
			Lock::Guard guard(_lock);
			return _any_space_avail_for_writing();
		}

		bool data_avail_for_reading() const
		{
			Lock::Guard guard(_lock);

			return _read_offset != _write_offset;
		}

		size_t read(char *dst, size_t dst_len)
		{
			Lock::Guard guard(_lock);

			if (_read_offset < _write_offset) {

				size_t len = min(dst_len, _write_offset - _read_offset);
				memcpy(dst, &_buffer[_read_offset], len);

				_read_offset += len;
				_wake_up_writer();

				return len;
			}

			if (_read_offset > _write_offset) {

				size_t const upper_len = min(dst_len, BUFFER_SIZE - _read_offset);
				memcpy(dst, &_buffer[_read_offset], upper_len);

				size_t const lower_len = min(dst_len - upper_len, _write_offset);
				if (lower_len) {
					memcpy(dst + upper_len, &_buffer[0], lower_len);
					_read_offset = lower_len;
				} else {
					_read_offset += upper_len;
				}
				_wake_up_writer();

				return upper_len + lower_len;
			}

			/* _read_offset == _write_offset */
			return 0;
		}

		/**
		 * Write to pipe buffer
		 *
		 * \return number of written bytes (may be less than 'len')
		 */
		size_t write(char *src, size_t len)
		{
			Lock::Guard guard(_lock);

			/* trim write request to the available buffer space */
			size_t const trimmed_len = min(len, _avail_buffer_space());

			/*
			 * Remember pipe state prior writing to see whether a reader
			 * must be unblocked after writing.
			 */
			bool const pipe_was_empty = (_read_offset == _write_offset);

			/* write data up to the upper boundary of the pipe buffer */
			size_t const upper_len = min(BUFFER_SIZE - _write_offset, trimmed_len);
			memcpy(&_buffer[_write_offset], src, upper_len);

			_write_offset += upper_len;

			/*
			 * Determine number of remaining bytes beyond the buffer boundary.
			 * The buffer wraps. So this data will end up in the lower part
			 * of the pipe buffer.
			 */
			size_t const lower_len = trimmed_len - upper_len;

			if (lower_len > 0) {

				/* pipe buffer wrap-around, write remaining data to the lower part */
				memcpy(&_buffer[0], src + upper_len, lower_len);
				_write_offset = lower_len;
			}

			/*
			 * Wake up reader who may block for incoming data.
			 */
			if (pipe_was_empty || !_any_space_avail_for_writing())
				_wake_up_reader();

			/* return number of written bytes */
			return trimmed_len;
		}

		void register_write_ready_sigh(Signal_context_capability sigh)
		{
			Lock::Guard guard(_lock);
			_write_ready_sigh = sigh;
		}

		void register_read_ready_sigh(Signal_context_capability sigh)
		{
			Lock::Guard guard(_lock);
			_read_ready_sigh = sigh;
		}
};


class Noux::Pipe_sink_io_channel : public Io_channel
{
	private:

		Signal_handler<Pipe_sink_io_channel> _write_ready_handler;

		void _handle_write_ready() { Io_channel::invoke_all_notifiers(); }

		Shared_pointer<Pipe> _pipe;

	public:

		Pipe_sink_io_channel(Shared_pointer<Pipe> pipe, Entrypoint &ep)
		:
			_write_ready_handler(ep, *this, &Pipe_sink_io_channel::_handle_write_ready),
			_pipe(pipe)
		{
			pipe->register_write_ready_sigh(_write_ready_handler);
		}

		~Pipe_sink_io_channel() { _pipe->writer_close(); }

		bool check_unblock(bool rd, bool wr, bool ex) const override
		{
			return wr && _pipe->any_space_avail_for_writing();
		}

		bool write(Sysio &sysio, size_t &offset) override
		{
			/*
			 * If the write operation is larger than the space available in
			 * the pipe buffer, the write function is successively called
			 * for different portions of original write request. The
			 * current read pointer of the request is tracked via the
			 * 'count' in/out argument. If completed, 'count' equals
			 * 'write_in.count'.
			 */

			/* dimension the pipe write operation to the not yet written data */
			size_t curr_count = _pipe->write(sysio.write_in.chunk + offset,
			                                 sysio.write_in.count - offset);
			offset += curr_count;
			return true;
		}

		bool fcntl(Sysio &sysio) override
		{
			switch (sysio.fcntl_in.cmd) {

				case Sysio::FCNTL_CMD_GET_FILE_STATUS_FLAGS:
					sysio.fcntl_out.result = Sysio::OPEN_MODE_WRONLY;
					return true;

				default:
					return false;
			}
		}

		bool fstat(Sysio &sysio) override
		{
			sysio.fstat_out.st.mode = Sysio::STAT_MODE_CHARDEV;
			return true;
		}
};


class Noux::Pipe_source_io_channel : public Io_channel
{
	private:

		Signal_handler<Pipe_source_io_channel> _read_avail_handler;

		void _handle_read_avail() { Io_channel::invoke_all_notifiers(); }

		Shared_pointer<Pipe> _pipe;

	public:

		Pipe_source_io_channel(Shared_pointer<Pipe> pipe, Entrypoint &ep)
		:
			_read_avail_handler(ep, *this, &Pipe_source_io_channel::_handle_read_avail),
			_pipe(pipe)
		{
			_pipe->register_read_ready_sigh(_read_avail_handler);
		}

		~Pipe_source_io_channel() { _pipe->reader_close(); }

		bool check_unblock(bool rd, bool wr, bool ex) const override
		{
			/* unblock if the writer has already closed its pipe end */
			if (_pipe->writer_is_gone())
				return true;

			return (rd && _pipe->data_avail_for_reading());
		}

		bool read(Sysio &sysio) override
		{
			size_t const max_count =
				min(sysio.read_in.count,
				    sizeof(sysio.read_out.chunk));

			sysio.read_out.count =
				_pipe->read(sysio.read_out.chunk, max_count);

			return true;
		}

		bool fcntl(Sysio &sysio) override
		{
			switch (sysio.fcntl_in.cmd) {

				case Sysio::FCNTL_CMD_GET_FILE_STATUS_FLAGS:
					sysio.fcntl_out.result = Sysio::OPEN_MODE_RDONLY;
					return true;

				default:
					return false;
			}
		}

		bool fstat(Sysio &sysio) override
		{
			sysio.fstat_out.st.mode = Sysio::STAT_MODE_CHARDEV;
			return true;
		}
};

#endif /* _NOUX__PIPE_IO_CHANNEL_H_ */
