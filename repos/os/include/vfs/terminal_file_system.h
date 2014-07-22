/*
 * \brief  Terminal file system
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2012-05-23
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_
#define _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_

#include <terminal_session/connection.h>
#include <vfs/single_file_system.h>

namespace Vfs { class Terminal_file_system; }


class Vfs::Terminal_file_system : public Single_file_system
{
	private:

		struct Label
		{
			enum { LABEL_MAX_LEN = 64 };
			char string[LABEL_MAX_LEN];

			Label(Xml_node config)
			{
				string[0] = 0;
				try { config.attribute("label").value(string, sizeof(string)); }
				catch (...) { }
			}
		} _label;

		Terminal::Connection _terminal;

	public:

		Terminal_file_system(Xml_node config)
		:
			Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config),
			_label(config),
			_terminal(_label.string)
		{
			/*
			 * Wait for connection-established signal
			 */

			/* create signal receiver, just for the single signal */
			Genode::Signal_context    sig_ctx;
			Genode::Signal_receiver   sig_rec;
			Signal_context_capability sig_cap = sig_rec.manage(&sig_ctx);

			/* register signal handler */
			_terminal.connected_sigh(sig_cap);

			/* wati for signal */
			sig_rec.wait_for_signal();
			sig_rec.dissolve(&sig_ctx);
		}

		static const char *name() { return "terminal"; }


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *, char const *buf, size_t buf_size, size_t &out_count) override
		{
			out_count = _terminal.write(buf, buf_size);
			return WRITE_OK;
		}

		Read_result read(Vfs_handle *, char *dst, size_t count, size_t &out_count) override
		{
			out_count = _terminal.read(dst, count);
			return READ_OK;
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, size_t) override
		{
			return FTRUNCATE_OK;
		}

		bool check_unblock(Vfs_handle *vfs_handle, bool rd, bool wr, bool ex) override
		{
			if (rd && (_terminal.avail() > 0))
				return true;

			if (wr)
				return true;

			return false;
		}

		void register_read_ready_sigh(Vfs_handle *vfs_handle,
		                              Signal_context_capability sigh) override
		{
			_terminal.read_avail_sigh(sigh);
		}
};

#endif /* _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_ */
