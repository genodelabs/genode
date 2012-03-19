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
		Terminal::Session       &terminal;
		Genode::Signal_receiver &sig_rec;

		enum Type { STDIN, STDOUT, STDERR } type;

		Terminal_io_channel(Terminal::Session &terminal, Type type,
		                    Genode::Signal_receiver &sig_rec)
		: terminal(terminal), sig_rec(sig_rec), type(type)
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

			Genode::size_t const max_count =
				Genode::min(sysio->read_in.count,
				            sizeof(sysio->read_out.chunk));

			sysio->read_out.count =
				terminal.read(sysio->read_out.chunk, max_count);
			return true;
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
					PDBG("OP_TIOCGWINSZ requested");
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
