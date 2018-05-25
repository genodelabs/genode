/*
 * \brief  Libc plugin providing lwIP's DNS server address in the
 *         '/socket/nameserver' file
 * \author Christian Prochaska
 * \date   2013-05-02
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* lwip includes */
#include <lwip/err.h>
#include <lwip/ip_addr.h>
#include <lwip/dns.h>

/* fix redefinition warnings */
#undef LITTLE_ENDIAN
#undef BIG_ENDIAN
#undef BYTE_ORDER

/* libc plugin interface */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* libc includes */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/env.h>
#include <base/heap.h>
#include <util/misc_math.h>

#include <base/log.h>
#include <parent/parent.h>

#include <nic/packet_allocator.h>
#include <util/string.h>

#include <lwip_legacy/genode.h>

#undef AF_INET6
#undef MSG_PEEK
#undef MSG_WAITALL
#undef MSG_OOB
#undef MSG_DONTWAIT

namespace Lwip {
extern "C" {
#include <lwip/sockets.h>
#include <lwip/api.h>
}
}

extern void create_lwip_plugin();


namespace {

	class Plugin_context : public Libc::Plugin_context
	{
		private:

			int _status_flags;
			off_t _seek_offset;

		public:

			Plugin_context() : _status_flags(0), _seek_offset(0) { }

			/**
			 * Set/get file status status flags
			 */
			void status_flags(int flags) { _status_flags = flags; }
			int status_flags() { return _status_flags; }

			/**
			 * Set seek offset
			 */
			void seek_offset(size_t seek_offset) { _seek_offset = seek_offset; }

			/**
			 * Return seek offset
			 */
			off_t seek_offset() const { return _seek_offset; }

			/**
			 * Advance current seek position by 'incr' number of bytes
			 */
			void advance_seek_offset(size_t incr)
			{
				_seek_offset += incr;
			}

			void infinite_seek_offset()
			{
				_seek_offset = ~0;
			}

	};


	static inline Plugin_context *context(Libc::File_descriptor *fd)
	{
		return static_cast<Plugin_context *>(fd->context);
	}


	class Plugin : public Libc::Plugin
	{
		private:

			Genode::Constructible<Genode::Heap> _heap;

			/**
			 * File name this plugin feels responsible for
			 */
			static char const *_file_name() { return "/socket/nameserver"; }

			const char *_file_content()
			{
				static char result[32];
				ip_addr_t nameserver_ip = dns_getserver(0);
				snprintf(result, sizeof(result), "%s\n",
				         ipaddr_ntoa(&nameserver_ip));
				return result;
			}

			::off_t _file_size(Libc::File_descriptor *fd)
			{
				struct stat stat_buf;
				if (fstat(fd, &stat_buf) == -1)
					return -1;
				return stat_buf.st_size;
			}

		public:

			/**
			 * Constructor
			 */
			Plugin() { }

			bool supports_stat(const char *path)
			{
				return (Genode::strcmp(path, "/socket") == 0) ||
				       (Genode::strcmp(path, _file_name()) == 0);
			}

			bool supports_open(const char *path, int flags)
			{
				return (Genode::strcmp(path, _file_name()) == 0);
			}

			Libc::File_descriptor *open(const char *pathname, int flags)
			{
				Plugin_context *context = new (*_heap) Plugin_context;
				context->status_flags(flags);
				return Libc::file_descriptor_allocator()->alloc(this, context);
			}

			int close(Libc::File_descriptor *fd)
			{
				Genode::destroy(*_heap, context(fd));
				Libc::file_descriptor_allocator()->free(fd);
				return 0;
			}

			int stat(const char *path, struct stat *buf)
			{
				if (buf) {
					Genode::memset(buf, 0, sizeof(struct stat));
					if (Genode::strcmp(path, "/socket") == 0)
						buf->st_mode = S_IFDIR;
					else if (Genode::strcmp(path, _file_name()) == 0) {
						buf->st_mode = S_IFREG;
						buf->st_size = strlen(_file_content()) + 1;
					} else {
						errno = ENOENT;
						return -1;
					}
				}
				return 0;
			}

			int fstat(Libc::File_descriptor *fd, struct stat *buf)
			{
				if (buf) {
					Genode::memset(buf, 0, sizeof(struct stat));
					buf->st_mode = S_IFREG;
					buf->st_size = strlen(_file_content()) + 1;
				}
				return 0;
			}

			::off_t lseek(Libc::File_descriptor *fd, ::off_t offset, int whence)
			{
				switch (whence) {

				case SEEK_SET:
					context(fd)->seek_offset(offset);
					return offset;

				case SEEK_CUR:
					context(fd)->advance_seek_offset(offset);
					return context(fd)->seek_offset();

				case SEEK_END:
					if (offset != 0) {
						errno = EINVAL;
						return -1;
					}
					context(fd)->infinite_seek_offset();
					return _file_size(fd);

				default:
					errno = EINVAL;
					return -1;
				}
			}

			ssize_t read(Libc::File_descriptor *fd, void *buf, ::size_t count)
			{
				::off_t seek_offset = context(fd)->seek_offset();

				if (seek_offset >= _file_size(fd))
					return 0;

				const char *content = _file_content();
				count = Genode::min((::off_t)count, _file_size(fd) - seek_offset);

				memcpy(buf, &content[seek_offset], count);

				context(fd)->advance_seek_offset(count);

				return count;
			}

			int fcntl(Libc::File_descriptor *fd, int cmd, long arg)
			{
				switch (cmd) {
					case F_GETFL: return context(fd)->status_flags();
					default: Genode::error("fcntl(): command ", cmd, " not supported", cmd); return -1;
				}
			}

			void init(Libc::Env &env) override
			{
				_heap.construct(env.ram(), env.rm());

				enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

				Genode::log(__func__);

				char ip_addr_str[16] = {0};
				char netmask_str[16] = {0};
				char gateway_str[16] = {0};

				genode_int32_t ip_addr = 0;
				genode_int32_t netmask = 0;
				genode_int32_t gateway = 0;
				Genode::Number_of_bytes tx_buf_size(BUF_SIZE);
				Genode::Number_of_bytes rx_buf_size(BUF_SIZE);

				try {
					Genode::Attached_rom_dataspace config(env, "config");
					Genode::Xml_node libc_node = config.xml().sub_node("libc");

					try {
						libc_node.attribute("ip_addr").value(ip_addr_str, sizeof(ip_addr_str));
					} catch(...) { }

					try {
						libc_node.attribute("netmask").value(netmask_str, sizeof(netmask_str));
					} catch(...) { }

					try {
						libc_node.attribute("gateway").value(gateway_str, sizeof(gateway_str));
					} catch(...) { }

					try {
						libc_node.attribute("tx_buf_size").value(&tx_buf_size);
					} catch(...) { }

					try {
						libc_node.attribute("rx_buf_size").value(&rx_buf_size);
					} catch(...) { }

					/* either none or all 3 interface attributes must exist */
					if ((strlen(ip_addr_str) != 0) ||
						(strlen(netmask_str) != 0) ||
						(strlen(gateway_str) != 0)) {
						if (strlen(ip_addr_str) == 0) {
							Genode::error("missing \"ip_addr\" attribute. Ignoring network interface config.");
							throw Genode::Xml_node::Nonexistent_attribute();
						} else if (strlen(netmask_str) == 0) {
							Genode::error("missing \"netmask\" attribute. Ignoring network interface config.");
							throw Genode::Xml_node::Nonexistent_attribute();
						} else if (strlen(gateway_str) == 0) {
							Genode::error("missing \"gateway\" attribute. Ignoring network interface config.");
							throw Genode::Xml_node::Nonexistent_attribute();
						}
					} else
						throw -1;

					Genode::log("static network interface: "
								"ip_addr=", Genode::Cstring(ip_addr_str), " "
								"netmask=", Genode::Cstring(netmask_str), " "
								"gateway=", Genode::Cstring(gateway_str));

					genode_uint32_t ip, nm, gw;

					ip = inet_addr(ip_addr_str);
					nm = inet_addr(netmask_str);
					gw = inet_addr(gateway_str);

					if (ip == INADDR_NONE || nm == INADDR_NONE || gw == INADDR_NONE) {
						Genode::error("invalid network interface config");
						throw -1;
					} else {
						ip_addr = ip;
						netmask = nm;
						gateway = gw;
					}
				}
				catch (...) {
					Genode::log("Using DHCP for interface configuration.");
				}

				/* make sure the libc_lwip plugin has been created */
				create_lwip_plugin();

				try {
					lwip_nic_init(ip_addr, netmask, gateway,
								  (Genode::size_t)tx_buf_size, (Genode::size_t)rx_buf_size);
				}
				catch (Genode::Service_denied) { /* ignore for now */ }
			}
	};
} /* unnamed namespace */


void __attribute__((constructor)) init_libc_lwip_dhcp(void)
{
	static Plugin plugin;
}
