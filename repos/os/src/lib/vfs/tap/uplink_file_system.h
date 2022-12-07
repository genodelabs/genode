/*
 * \brief  Vfs handle for an uplink client.
 * \author Johannes Schlatow
 * \date   2022-01-24
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__VFS__TAP__UPLINK_FILE_SYSTEM_H_
#define _SRC__LIB__VFS__TAP__UPLINK_FILE_SYSTEM_H_

#include <vfs/single_file_system.h>

/* local Uplink_client_base with added custom handler */
#include "uplink_client_base.h"

namespace Vfs {
	using namespace Genode;

	class Uplink_file_system;
}


class Vfs::Uplink_file_system : public Vfs::Single_file_system
{
	public:

		class Uplink_vfs_handle;

		using Vfs_handle = Uplink_vfs_handle;

		Uplink_file_system(char const *name)
		: Single_file_system(Node_type::TRANSACTIONAL_FILE, name,
		                     Node_rwx::rw(), Genode::Xml_node("<data/>"))
		{ }
};


class Vfs::Uplink_file_system::Uplink_vfs_handle : public Single_vfs_handle,
                                                   public Genode::Uplink_client_base
{
	private:

		using Read_result  = File_io_service::Read_result;
		using Write_result = File_io_service::Write_result;

		bool _notifying = false;
		bool _blocked   = false;

		void _handle_read_avail()
		{
			if (!read_ready())
				return;

			if (_blocked) {
				_blocked = false;
				io_progress_response();
			}

			if (_notifying) {
				_notifying = false;
				read_ready_response();
			}
		}

		/************************
		 ** Uplink_client_base **
		 ************************/

		bool _custom_conn_rx_ready_to_ack_handler()    override { return true; }
		bool _custom_conn_rx_packet_avail_handler()    override { return true; }

		void _custom_conn_rx_handle_packet_avail()     override { _handle_read_avail(); }
		void _custom_conn_rx_handle_ready_to_ack()     override { _handle_read_avail(); }

		Transmit_result _drv_transmit_pkt(const char *, size_t) override
		{
			class Unexpected_call { };
			throw Unexpected_call { };
		}

	public:

		Uplink_vfs_handle(Genode::Env            &env,
		                  Allocator              &alloc,
		                  Label            const &label,
		                  Net::Mac_address const &mac,
		                  Directory_service      &ds,
		                  File_io_service        &fs,
		                  int                     flags)
		: Single_vfs_handle  { ds, fs, alloc, flags },
		  Uplink_client_base { env, alloc, mac, label }
		{ _drv_handle_link_state(true); }

		bool notify_read_ready() override
		{
			_notifying = true;
			return true;
		}

		void mac_address(Net::Mac_address const & mac)
		{
			if (_drv_mac_addr_used) {
				_drv_handle_link_state(false);
				Uplink_client_base::mac_address(mac);
				_drv_handle_link_state(true);
			}
			else
				Uplink_client_base::mac_address(mac);
		}

		Net::Mac_address mac_address() const {
			return _drv_mac_addr; }

		/************************
		 * Vfs_handle interface *
		 ************************/

		bool read_ready() const override
		{
			auto &nonconst_this = const_cast<Uplink_vfs_handle &>(*this);
			auto &rx = *nonconst_this._conn->rx();

			return _drv_link_state && rx.packet_avail() && rx.ready_to_ack();
		}

		bool write_ready() const override
		{
			/* wakeup from WRITE_ERR_WOULD_BLOCK not supported */
			return _drv_link_state;
		}

		Read_result read(char *dst, file_size count,
		                 file_size &out_count) override
		{
			if (!_conn.constructed())
				return Read_result::READ_ERR_INVALID;

			if (!read_ready()) {
				_blocked = true;
				return Read_result::READ_QUEUED;
			}

			out_count = 0;

			/* process a single packet from rx stream */
			Packet_descriptor const conn_rx_pkt { _conn->rx()->get_packet() };

			if (conn_rx_pkt.size() > 0 &&
				 _conn->rx()->packet_valid(conn_rx_pkt)) {

				const char *const conn_rx_pkt_base {
					_conn->rx()->packet_content(conn_rx_pkt) };

				out_count = static_cast<file_size>(min(conn_rx_pkt.size(), static_cast<size_t>(count)));
				memcpy(dst, conn_rx_pkt_base, static_cast<size_t>(out_count));

				_conn->rx()->acknowledge_packet(conn_rx_pkt);
			}

			return Read_result::READ_OK;
		}

		Write_result write(char const *src, file_size count,
		                   file_size &out_count) override
		{
			if (!_conn.constructed())
				return Write_result::WRITE_ERR_INVALID;

			out_count = 0;
			_drv_rx_handle_pkt(static_cast<size_t>(count), [&] (void * dst, size_t dst_size) {
				out_count = dst_size;
				memcpy(dst, src, dst_size);
				return Uplink_client_base::Write_result::WRITE_SUCCEEDED;
			});

			if (out_count == count)
				return Write_result::WRITE_OK;
			else
				return Write_result::WRITE_ERR_WOULD_BLOCK;
		}
};

#endif /* _SRC__LIB__VFS__TAP__UPLINK_FILE_SYSTEM_H_ */
