/*
 * \brief  I/O channel targeting Genode's terminal interface
 * \author Norman Feske
 * \date   2011-10-21
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__TERMINAL_IO_CHANNEL_H_
#define _NOUX__TERMINAL_IO_CHANNEL_H_

/* Genode includes */
#include <util/string.h>
#include <base/printf.h>
#include <terminal_session/connection.h>

/* Noux includes */
#include <io_channel.h>
#include <noux_session/sysio.h>

namespace Noux {

	struct Terminal_io_channel : Io_channel, Signal_dispatcher
	{
		Terminal::Session &terminal;
		Signal_receiver   &sig_rec;
		bool               eof;

		enum Type { STDIN, STDOUT, STDERR } type;

		Terminal_io_channel(Terminal::Session &terminal, Type type,
		                    Signal_receiver &sig_rec)
		: terminal(terminal), sig_rec(sig_rec), eof(false), type(type)
		{
			/*
			 * Enable wake up STDIN channel on the presence of new input
			 *
			 * By registering our I/O channel as signal handler, the Noux
			 * main loop will be unblocked on the arrival of new input.
			 * It will check if the received signal belongs to an I/O channel
			 * and invokes the 'handle_signal' function of the I/O channel.
			 *
			 * This gives us the opportunity to handle the unblocking of
			 * blocking system calls such as 'select'.
			 */
			if (type == STDIN) {
				terminal.read_avail_sigh(sig_rec.manage(this));
			}
		}

		~Terminal_io_channel() { sig_rec.dissolve(this); }

		bool write(Sysio *sysio, size_t &count)
		{
			terminal.write(sysio->write_in.chunk, sysio->write_in.count);
			count = sysio->write_in.count;
			return true;
		}

		bool read(Sysio *sysio)
		{
			if (type != STDIN) {
				PERR("attempt to read from terminal output channel");
				return false;
			}

			/* deliver EOF observed by the previous 'read' call */
			if (eof) {
				sysio->read_out.count = 0;
				eof = false;
				return true;
			}

			size_t const max_count =
				min(sysio->read_in.count,
				    sizeof(sysio->read_out.chunk));

			sysio->read_out.count =
				terminal.read(sysio->read_out.chunk, max_count);

			/* scan received characters for EOF */
			for (unsigned i = 0; i < sysio->read_out.count; i++) {

				enum { EOF = 4 };
				if (sysio->read_out.chunk[i] != EOF)
					continue;

				/* discard EOF character and everything that follows... */
				sysio->read_out.count = i;

				/*
				 * If EOF was the only character of the batch, the count has
				 * reached zero. In this case the read result indicates the EOF
				 * condition as is. However, if count is greater than zero, we
				 * deliver the previous characters of the batch and return the
				 * zero result from the subsequent 'read' call. This condition
				 * is tracked by the 'eof' variable.
				 */
				if (sysio->read_out.count > 0)
					eof = true;
			}

			return true;
		}

		bool fcntl(Sysio *sysio)
		{
			/**
			 * Actually it is "inappropiate" to use fcntl() directly on terminals
			 * (atleast according to the Open Group Specification). We do it anyway
			 * since in our case stdout/in/err is directly connected to the terminal.
			 *
			 * Some GNU programms check if stdout is open by calling fcntl(stdout, F_GETFL, ...).
			 */
			switch (sysio->fcntl_in.cmd) {

			case Sysio::FCNTL_CMD_GET_FILE_STATUS_FLAGS:
				sysio->fcntl_out.result = 0;
				return true;

			default:
				return false;
			}

			return false;
		}

		bool fstat(Sysio *sysio)
		{
			/*
			 * Supply stat values such that libc is happy. I.e., the libc
			 * is checking for the file descriptor 1 being a character
			 * device.
			 */
			sysio->fstat_out.st.mode = Sysio::STAT_MODE_CHARDEV;
			return true;
		}

		bool check_unblock(bool rd, bool wr, bool ex) const
		{
			/* never block for writing */
			if (wr) return true;

			/*
			 * Unblock I/O channel if the terminal has new user input. Channels
			 * otther than STDIN will never unblock.
			 */
			return (rd && (type == STDIN) && terminal.avail());
		}

		bool ioctl(Sysio *sysio)
		{
			switch (sysio->ioctl_in.request) {

			case Sysio::Ioctl_in::OP_TIOCGWINSZ:
				{
					Terminal::Session::Size size = terminal.size();
					sysio->ioctl_out.tiocgwinsz.rows    = size.lines();
					sysio->ioctl_out.tiocgwinsz.columns = size.columns();
					return true;
				}

			default:

				PDBG("invalid ioctl request %d", sysio->ioctl_in.request);
				return false;
			};
		}


		/*********************************
		 ** Signal_dispatcher interface **
		 *********************************/

		/**
		 * Called by Noux main loop on the occurrence of new STDIN input
		 */
		void dispatch()
		{
			Io_channel::invoke_all_notifiers();
		}
	};
}

#endif /* _NOUX__TERMINAL_IO_CHANNEL_H_ */
