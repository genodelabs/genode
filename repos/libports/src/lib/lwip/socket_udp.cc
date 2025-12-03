/*
 * \brief  UDP protocol to lwIP mapping
 * \author Sebastian Sumpf
 * \date   2025-01-28
 *
 * Based on the VFS-plugin by Emery Hemingway
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include<socket_lwip.h>

namespace Lwip {
#include <lwip/udp.h>
}

namespace Socket {
	class Udp;
}


class Socket::Udp : public Protocol
{
	private:

		class Packet : public  Fifo<Packet>::Element,
		               private Genode::Noncopyable
		{
			public:

				Lwip::ip_addr_t const addr;
				Lwip::u16_t     const port;

			private:

				Lwip::u16_t  _offset = 0;
				Lwip::pbuf  &_pbuf;

			public:

				Packet(Lwip::ip_addr_t const *addr, Lwip::u16_t port, Lwip::pbuf &pbuf)
				: addr(*addr), port(port), _pbuf(pbuf) { }

				~Packet() { Lwip::pbuf_free(&_pbuf); }

				Lwip::u16_t read(void *dst, size_t count)
				{
					count = min(size_t(_pbuf.tot_len), count);
					Lwip::u16_t n = Lwip::pbuf_copy_partial(&_pbuf, dst,
					                                        Lwip::u16_t(count),
					                                        _offset);
					_offset += n;
					return n;
				}

				Lwip::u16_t peek(void *dst, size_t count)  const
				{
					count = min(size_t(_pbuf.tot_len), count);
					return Lwip::pbuf_copy_partial(&_pbuf, dst, Lwip::u16_t(count),
					                               _offset);
				}

				bool empty() { return _offset >= _pbuf.tot_len; }
		};

		Allocator &_alloc;

		Lwip::udp_pcb &_pcb { *Lwip::udp_new() };

		Tslab<Packet, sizeof(Packet)*64> _packet_slab { &_alloc };

		/* Queue of received UDP packets */
		Fifo<Packet> _packet_queue { };

		bool _ip_addr_equal(Lwip::ip_addr_t const &addr1,
		                    Lwip::ip_addr_t const &addr2)
		{
			using namespace Lwip;
			/* is macro */
			return ip_addr_cmp(&addr1, &addr2);
		}

		void _queue(Lwip::ip_addr_t const *addr, Lwip::u16_t port, Lwip::pbuf *pbuf)
		{
			try {
				Packet *packet = new (_packet_slab) Packet(addr, port, *pbuf);
				_packet_queue.enqueue(*packet);
			}
			catch (...) {
				/* drop */
				Lwip::pbuf_free(pbuf);
			}
		}

		static void
		_udp_recv_callback(void *arg, Lwip::udp_pcb *, Lwip::pbuf *pbuf,
		                   Lwip::ip_addr_t const *addr, Lwip::u16_t port)
		{
			if (!arg) {
				pbuf_free(pbuf);
				return;
			}

			static_cast<Udp *>(arg)->_queue(addr, port, pbuf);
		}

	public:

		Udp(Allocator &alloc)
		:
			Protocol(NEW),
			_alloc(alloc)
		{
			/* 'this' will be the argument to the LwIP recv callback */
			Lwip::udp_recv(&_pcb, _udp_recv_callback, this);
		}

		~Udp()
		{
			_packet_queue.dequeue_all([&] (Packet &packet) {
				destroy(_packet_slab, &packet);
			});

			Lwip::udp_remove(&_pcb);
		}

		Errno bind(genode_sockaddr const &addr) override
		{
			Lwip::ip_addr_t ip   = lwip_ip_addr(addr);
			Lwip::u16_t     port = Lwip::ntohs(addr.in.port);

			return lwip_error(Lwip::udp_bind(&_pcb, &ip, port));
		}

		Errno listen(uint8_t) override
		{
			return lwip_error(GENODE_ENOTSUPP);
		}

		Protocol *accept(genode_sockaddr *, Errno &errno) override
		{
			errno = GENODE_ENOTSUPP;
			return nullptr;
		}

		Errno connect(genode_sockaddr const &addr, bool disconnect) override
		{
			if (disconnect) {
				Lwip::udp_disconnect(&_pcb);
				return lwip_error(GENODE_ENONE);
			}

			Lwip::ip_addr_t ip   = lwip_ip_addr(addr);
			Lwip::u16_t     port = Lwip::ntohs(addr.in.port);

			return lwip_error(Lwip::udp_connect(&_pcb, &ip, port));
		}

		Errno sendmsg(genode_msghdr &hdr, unsigned long &bytes_send) override
		{
			Lwip::ip_addr_t ip { };
			Lwip::u16_t     port = 0;

			if (hdr.name) {
				if (hdr.name->family != AF_INET)
					return lwip_error(GENODE_EOPNOTSUPP);

				ip   = lwip_ip_addr(*hdr.name);
				port = Lwip::lwip_ntohs(hdr.name->in.port);
			}

			bytes_send = 0;

			Errno error = GENODE_ENONE;
			for_each_iovec(hdr, [&] (void const *base, unsigned long size,
			                         unsigned long) {

				if (error != GENODE_ENONE) return;

				char const *src = (char const*)base;
				while (size) {
					/* only C functions in loop */
					using namespace Lwip;

					Lwip::u16_t const alloc_size = min(uint16_t(~0u), uint16_t(size));
					pbuf * const pbuf = pbuf_alloc(PBUF_RAW, alloc_size, PBUF_RAM);
					pbuf_take(pbuf, src, pbuf->tot_len);

					err_t err;
					if (hdr.name)
						err = udp_sendto(&_pcb, pbuf, &ip, port);
					else /* connected socket */
						err = udp_send(&_pcb, pbuf);

					unsigned long send = pbuf->tot_len;
					pbuf_free(pbuf);

					if (err != ERR_OK) {
						error = genode_errno(err);
						break;
					}

					size -= send; src += send; bytes_send += send;
				}
			});

			return lwip_error(error);
		}

		Errno recvmsg(genode_msghdr &msg, unsigned long &bytes_recv,
		              bool msg_peek) override
		{
			/* retrieve remote peer */
			Lwip::ip_addr_t ip;
			_packet_queue.head([&] (Packet &packet) {
				ip = packet.addr;
				if (msg.name) {
					msg.name->family  = AF_INET;
					msg.name->in.addr = ip.u_addr.ip4.addr;
					msg.name->in.port = Lwip::htons(packet.port);
				}
			});

			bool done  = false;
			bytes_recv = 0;
			for_each_iovec(msg, [&](void *base, unsigned long size,
			               unsigned long &used) {
				if (done) return;

				_packet_queue.head([&] (Packet &packet) {

					/* msghdr only supports one addr */
					if (!_ip_addr_equal(ip, packet.addr)) {
						done = true;
						return;
					}

					/* peek only one packet */
					if (msg_peek) {
						used = bytes_recv = packet.peek(base, size);
						done = true;
						return;
					}

					bytes_recv += (used = packet.read(base, size));
					if (packet.empty()) {
						_packet_queue.remove(packet);
						destroy(_packet_slab, &packet);
					}
				});
			});

			return lwip_error(bytes_recv > 0 ? GENODE_ENONE : GENODE_EAGAIN);
		}

		Errno peername(genode_sockaddr &) override
		{
			Genode::error(__func__, " not implemented");
			return lwip_error(GENODE_ENOTSUPP);
		}

		Errno name(genode_sockaddr &addr) override
		{
			addr.in.addr = _pcb.local_ip.u_addr.ip4.addr;
			addr.in.port = Lwip::htons(_pcb.local_port);

			return lwip_error(GENODE_ENONE);
		}

		unsigned poll() override
		{
			/* always writable */
			unsigned value = Poll::WRITE;
			if (_packet_queue.empty() == false)
				value |= Poll::READ;

			return value;
		}

		Errno shutdown() override
		{
			Genode::error(__func__, " not implemented");
			return lwip_error(GENODE_ENOTSUPP);
		}

		Errno getsockopt(Sock_level level, Sock_opt opt,
		                 void *optval, unsigned *optlen) override
		{
			if (!optval || *optlen < 1) return lwip_error(GENODE_EFAULT);

			if (level == GENODE_SOL_SOCKET) {
				unsigned   lwip_opt;
				switch (opt) {
				case GENODE_SO_REUSEADDR: lwip_opt = SOF_REUSEADDR; break;
				default: return lwip_error(GENODE_ENOPROTOOPT);
				}


				/* ip_* are macros */
				using namespace Lwip;
				Lwip::u8_t *lwip_optval = (Lwip::u8_t*)optval;
				*lwip_optval = !!ip_get_option(&_pcb, lwip_opt);

				return lwip_error(GENODE_ENONE);
			}

			return lwip_error(GENODE_ENOPROTOOPT);
		}

		Errno setsockopt(Sock_level level, Sock_opt opt,
		                 void const *optval, unsigned optlen) override
		{
			if (!optval) return lwip_error(GENODE_EFAULT);
			if (optlen < sizeof(Lwip::u8_t)) return lwip_error(GENODE_EINVAL);

			if (level == GENODE_SOL_SOCKET) {

				unsigned   lwip_opt;
				switch (opt) {
				case GENODE_SO_REUSEADDR: lwip_opt = SOF_REUSEADDR; break;
				default: return lwip_error(GENODE_ENOPROTOOPT);
				}

				/* ip_* are macros */
				using namespace Lwip;
				Lwip::u8_t lwip_optval = *(Lwip::u8_t*)optval;
				if (lwip_optval)
					ip_set_option(&_pcb, lwip_opt);
				else
					ip_reset_option(&_pcb, lwip_opt);

				return lwip_error(GENODE_ENONE);
			}

			return lwip_error(GENODE_ENOPROTOOPT);
		}
};


Socket::Protocol *Socket::create_udp(Genode::Allocator &alloc)
{
	return new (alloc) Udp(alloc);
}
