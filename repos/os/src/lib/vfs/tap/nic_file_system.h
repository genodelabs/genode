/*
 * \brief  Vfs handle for a Nic client.
 * \author Johannes Schlatow
 * \date   2022-01-26
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__VFS__TAP__NIC_FILE_SYSTEM_H_
#define _SRC__LIB__VFS__TAP__NIC_FILE_SYSTEM_H_

#include <net/mac_address.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>
#include <vfs/single_file_system.h>


namespace Vfs {
	using namespace Genode;

	class Nic_file_system;
}


class Vfs::Nic_file_system : public Vfs::Single_file_system
{
	public:

		class Nic_vfs_handle;

		using Vfs_handle = Nic_vfs_handle;

		Nic_file_system(char const *name)
		: Single_file_system(Node_type::TRANSACTIONAL_FILE, name,
		                     Node_rwx::rw(), Genode::Xml_node("<data/>"))
		{ }
};


class Vfs::Nic_file_system::Nic_vfs_handle : public Single_vfs_handle
{
	public:

		using Label = String<64>;

	private:

		using Read_result  = File_io_service::Read_result;
		using Write_result = File_io_service::Write_result;

		enum { PKT_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE };
		enum { BUF_SIZE = Uplink::Session::QUEUE_SIZE * PKT_SIZE };

		Genode::Env          &_env;
		Nic::Packet_allocator _pkt_alloc;
		Nic::Connection       _nic;
		bool                  _link_state;

		bool _notifying = false;
		bool _blocked   = false;

		Io_signal_handler<Nic_vfs_handle> _link_state_handler { _env.ep(), *this, &Nic_vfs_handle::_handle_link_state};
		Io_signal_handler<Nic_vfs_handle> _read_avail_handler { _env.ep(), *this, &Nic_vfs_handle::_handle_read_avail };
		Io_signal_handler<Nic_vfs_handle> _ack_avail_handler  { _env.ep(), *this, &Nic_vfs_handle::_handle_ack_avail };

		void _handle_ack_avail()
		{
			while (_nic.tx()->ack_avail()) {
				_nic.tx()->release_packet(_nic.tx()->get_acked_packet()); }
		}

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

		void _handle_link_state()
		{
			_link_state = _nic.link_state();
			_handle_read_avail();
		}

	public:

		Nic_vfs_handle(Genode::Env            &env,
		               Allocator              &alloc,
		               Label            const &label,
		               Net::Mac_address const &,
		               Directory_service      &ds,
		               File_io_service        &fs,
		               int                     flags)
		: Single_vfs_handle  { ds, fs, alloc, flags },
		  _env(env),
		  _pkt_alloc(&alloc),
		  _nic(_env, &_pkt_alloc, BUF_SIZE, BUF_SIZE, label.string()),
		  _link_state(_nic.link_state())
		{
			_nic.link_state_sigh(_link_state_handler);
			_nic.tx_channel()->sigh_ack_avail      (_ack_avail_handler);
			_nic.rx_channel()->sigh_ready_to_ack   (_read_avail_handler);
			_nic.rx_channel()->sigh_packet_avail   (_read_avail_handler);
		}

		bool notify_read_ready() override {
			_notifying = true;
			return true;
		}

		void mac_address(Net::Mac_address const &) { }

		Net::Mac_address mac_address() {
			return _nic.mac_address(); }

		/************************
		 * Vfs_handle interface *
		 ************************/

		bool read_ready() override {
			return _link_state && _nic.rx()->packet_avail() && _nic.rx()->ready_to_ack(); }

		Read_result read(char *dst, file_size count,
		                 file_size &out_count) override
		{
			if (!read_ready()) {
				_blocked = true;
				return Read_result::READ_QUEUED;
			}

			out_count = 0;

			/* process a single packet from rx stream */
			Packet_descriptor const rx_pkt { _nic.rx()->get_packet() };

			if (rx_pkt.size() > 0 &&
				 _nic.rx()->packet_valid(rx_pkt)) {

				const char *const rx_pkt_base {
					_nic.rx()->packet_content(rx_pkt) };

				out_count = static_cast<file_size>(min(rx_pkt.size(), static_cast<size_t>(count)));
				memcpy(dst, rx_pkt_base, static_cast<size_t>(out_count));

				_nic.rx()->acknowledge_packet(rx_pkt);
			}

			return Read_result::READ_OK;
		}

		Write_result write(char const *src, file_size count,
		                   file_size &out_count) override
		{
			out_count = 0;

			_handle_ack_avail();

			if (!_nic.tx()->ready_to_submit()) {
				return Write_result::WRITE_ERR_WOULD_BLOCK;
			}
			try {
				size_t tx_pkt_size { static_cast<size_t>(count) };

				Packet_descriptor tx_pkt {
					_nic.tx()->alloc_packet(tx_pkt_size) };

				void *tx_pkt_base {
					_nic.tx()->packet_content(tx_pkt) };

				memcpy(tx_pkt_base, src, tx_pkt_size);

				_nic.tx()->submit_packet(tx_pkt);
				out_count = tx_pkt_size;

				return Write_result::WRITE_OK;
			} catch (...) {

				warning("exception while trying to forward packet from driver "
				        "to Nic connection TX");

				return Write_result::WRITE_ERR_INVALID;
			}
		}
};

#endif /* _SRC__LIB__VFS__TAP__NIC_FILE_SYSTEM_H_ */
