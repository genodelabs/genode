/*
 * \brief  TCP protocol to lwIP mapping
 * \author Sebastian Sumpf
 * \date   2025-01-25
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
#include <lwip/tcp.h>
}

namespace Socket {
	class Tcp;
}

class Socket::Tcp : public Protocol
{
	private:

		Allocator &_alloc;

		struct Accept_pending : Fifo<Accept_pending>::Element
		{
			Lwip::tcp_pcb *pcb;
			Lwip::pbuf    *pbuf   { nullptr };
			size_t         length { 0 };

			Accept_pending(Lwip::tcp_pcb *pcb) : pcb(pcb) { }
		};

		Lwip::tcp_pcb *_pcb;

		/* queue of received data */
		Lwip::pbuf *_recv_pbuf   { nullptr };

		/* used for poll in */
		size_t _recv_length { 0 };

		Fifo<Accept_pending> _backlog { };

		size_t _sendbuf_avail()
		{
			/* tcp_sndbuf is a macro */
			using namespace Lwip;
			if (!_pcb) return 0;
			return tcp_sndbuf(_pcb);
		}

		bool _recvbuf_avail() const { return _recv_length > 0; }

		void _recvbuf_consume(size_t length)
		{
			_recv_length -= length > _recv_length ? _recv_length : length;
		}

		Lwip::err_t _sendmsg_queue(void * const base, Lwip::u16_t size)
		{
			char const *src = static_cast<char const *>(base);

			/*
			 * Write in a loop to account for lwIP chunking
			 */
			while(size) {
				Lwip::u16_t n = min(size, _sendbuf_avail());

				Lwip::err_t err  = Lwip::tcp_write(_pcb, src, n, TCP_WRITE_FLAG_COPY);
				if (err != Lwip::ERR_OK) {
					error("lwIP: tcp_write failed, error ", (int)err);
					return err;
				}
				size -= n; src += n;
			}

			return Lwip::ERR_OK;
		}

		Errno _name(genode_sockaddr &addr, bool local)
		{
			Lwip::ip_addr_t ip;
			Lwip::u16_t     port;

			Lwip::err_t err = Lwip::tcp_tcp_get_tcp_addrinfo(_pcb, local, &ip, &port);
			if (err == Lwip::ERR_OK) {
				/* avoid macros */
				addr.in.addr = ip.u_addr.ip4.addr;
				addr.in.port = Lwip::htons(port);
			}
			return genode_errno(err);
		}

		/*
		 * Callbacks & callback helpers
		 */

		Lwip::err_t _accept(Lwip::tcp_pcb *newpcb)
		{
			Accept_pending *pending = new (_alloc) Accept_pending(newpcb);
			_backlog.enqueue(*pending);

			/*
			 * Delay accepting a connection in respect to the listen backlog:
			 * the number of outstanding connections is increased until
			 * tcp_backlog_accepted() is called.
			 */
			Lwip::tcp_backlog_delayed(newpcb);

			Lwip::tcp_arg(newpcb, pending);
			Lwip::tcp_recv(newpcb, _tcp_delayed_recv_callback);

			return Lwip::ERR_OK;
		}

		/*
		 * Chain a buffer to the queue
		 */
		Lwip::err_t _recv(Lwip::pbuf *pbuf)
		{
			if (!pbuf)
				return Lwip::ERR_ARG;

			if (_recv_pbuf)
				Lwip::pbuf_cat(_recv_pbuf, pbuf);
			else
				_recv_pbuf = pbuf;

			_recv_length += pbuf->tot_len;

			return Lwip::ERR_OK;
		}

		/*
		 * Close the connection by error
		 *
		 * Triggered by error callback, usually just by an aborted connection. The
		 * corresponding pcb is already freed when this callback is called!
		 */
		void _error()
		{
			_state = CLOSED;
			_pcb = NULL;
		}

		static Lwip::err_t
		_tcp_accept_callback(void *arg, Lwip::tcp_pcb *newpcb, Lwip::err_t err)
		{
			if (!arg) {
				Lwip::tcp_abort(newpcb);
				return Lwip::ERR_ABRT;
			}

			Tcp *tcp = static_cast<Tcp *>(arg);
			return tcp->_accept(newpcb);
		}

		static Lwip::err_t
		_tcp_recv_callback(void *arg, Lwip::tcp_pcb *pcb, Lwip::pbuf *pbuf,
		                   Lwip::err_t)
		{
			if (!arg) {
				Lwip::tcp_abort(pcb);
				return Lwip::ERR_ABRT;
			}

			Lwip::err_t err = Lwip::ERR_OK;
			Tcp        *tcp = static_cast<Tcp *>(arg);

			if (pbuf == NULL) tcp->shutdown();
			else err = tcp->_recv(pbuf);

			return err;
		}

		static Lwip::err_t
		_tcp_delayed_recv_callback(void *arg, Lwip::tcp_pcb *pcb, Lwip::pbuf *pbuf,
		                           Lwip::err_t)
		{
			if (!arg) {
				Lwip::tcp_abort(pcb);
				return Lwip::ERR_ABRT;
			}

			if (!pbuf) return Lwip::ERR_CONN;

			Accept_pending *pending = static_cast<Accept_pending *>(arg);

			if (pending->pbuf) Lwip::pbuf_cat(pending->pbuf, pbuf);
			else pending->pbuf = pbuf;

			pending->length += pbuf->tot_len;

			return Lwip::ERR_OK;
		}

		static Lwip::err_t
		_tcp_connected_callback(void *arg, struct Lwip::tcp_pcb *pcb, Lwip::err_t err)
		{
			Tcp *tcp = static_cast<Tcp *>(arg);
			tcp->_state = Protocol::State::READY;
			tcp->_so_error = genode_errno(err);

			return Lwip::ERR_OK;
		}

		static void
		_tcp_err_callback(void *arg, Lwip::err_t)
		{
			if (!arg) return;

			Tcp *tcp = static_cast<Tcp *>(arg);
			/* the error is ERR_ABRT or ERR_RST, both end the session */
			tcp->_error();
		}

	public:

		Tcp(Allocator &alloc, Lwip::tcp_pcb *pcb = nullptr)
		:
			Protocol(pcb ? READY : NEW),
			_alloc(alloc),
			_pcb(pcb ? pcb : Lwip::tcp_new())
		{
			/* 'this' will be the argument to lwip callbacks */
			Lwip::tcp_arg(_pcb, this);
			Lwip::tcp_recv(_pcb, _tcp_recv_callback);
			Lwip::tcp_err(_pcb, _tcp_err_callback);
		}

		~Tcp()
		{
			if (_recv_pbuf) {
				Lwip::pbuf_free(_recv_pbuf);
				_recv_pbuf = nullptr;
			}

			Lwip::tcp_arg(_pcb, NULL);

			_backlog.dequeue_all([&] (Accept_pending &pending) {
				if (pending.pbuf) Lwip::pbuf_free(pending.pbuf);
				destroy(_alloc, &pending);
			});

			if (_pcb != NULL) {
				Lwip::tcp_arg(_pcb, NULL);
				Lwip::tcp_close(_pcb);
			}
		}

		Errno bind(genode_sockaddr const &addr) override
		{
			if (_state != NEW) return GENODE_EINVAL;

			Lwip::ip_addr_t ip   = lwip_ip_addr(addr);
			Lwip::u16_t     port = Lwip::lwip_ntohs(addr.in.port);
			Lwip::err_t     err  = Lwip::tcp_bind(_pcb, &ip, port);

			if (err == Lwip::ERR_OK) _state = BOUND;

			return genode_errno(err);
		}

		Errno listen(uint8_t backlog) override
		{
			if (_state != BOUND) return GENODE_EOPNOTSUPP;

			/*
			 * tcp_listen deallocates the _pcb and returns a new one in order to save
			 * memory + it can return null on memory exhaustion
			 */
			_pcb = Lwip::tcp_listen_with_backlog(_pcb, backlog);
			if (!_pcb) return GENODE_ENOMEM;

			Lwip::tcp_arg(_pcb, this);

			/*
			 * Register function that is called when listening connection has been
			 * connected to other host
			 */
			Lwip::tcp_accept(_pcb, _tcp_accept_callback);
			_state = LISTEN;

			return GENODE_ENONE;
		}

		Protocol *accept(genode_sockaddr *addr, Errno &errno) override
		{
			Tcp *proto = nullptr;
			errno      = GENODE_EAGAIN;

			_backlog.dequeue([&](Accept_pending &pending) {

				proto = new (_alloc) Tcp(_alloc, pending.pcb);
				proto->_recv_pbuf   = pending.pbuf;
				proto->_recv_length = pending.length;

				if (addr) proto->peername(*addr);

				Lwip::tcp_backlog_accepted(pending.pcb);

				destroy(_alloc, &pending);
				errno = GENODE_ENONE;
			});

			return proto;
		}

		Errno connect(genode_sockaddr const &addr, bool) override
		{
			using namespace Lwip;

			if ((_state != NEW) && (_state != BOUND)) return GENODE_EISCONN;

			_so_error = GENODE_ECONNREFUSED;

			Lwip::ip_addr_t ip   = lwip_ip_addr(addr);
			Lwip::u16_t     port = Lwip::lwip_ntohs(addr.in.port);

			/*
			 * Sends SYN and returns immediately, connection is establed when callback
			 * is invoked
			 */
			err_t err = Lwip::tcp_connect(_pcb, &ip, port,
			                              _tcp_connected_callback);

			/* we are non-blocking */
			if (err == Lwip::ERR_OK) {
				_state = CONNECT;
				return GENODE_EINPROGRESS;
			}

			return genode_errno(err);
		}

		Errno sendmsg(genode_msghdr &hdr, unsigned long &bytes_send) override
		{
			bytes_send = 0;

			/* socket is closed */
			if (_pcb == NULL)    return GENODE_EPIPE;
			if (_state != READY) return GENODE_EINVAL;

			Lwip::err_t err = Lwip::ERR_OK;
			for_each_iovec(hdr, [&](void * const base, unsigned long size,
			                        unsigned long) {

				if (err != Lwip::ERR_OK) return;

				/* limit size to available sendbuf */
				Lwip::u16_t limit_size = min(_sendbuf_avail(), size);
				if (limit_size == 0) {
					err = Lwip::ERR_WOULDBLOCK;
					return;
				}

				err = _sendmsg_queue(base, limit_size);
				if (err == Lwip::ERR_OK) bytes_send += limit_size;
			});

			if (err != Lwip::ERR_OK && err != Lwip::ERR_WOULDBLOCK)
				bytes_send = 0;

			/* send queued data */
			if (bytes_send > 0 && (err = Lwip::tcp_output(_pcb) != Lwip::ERR_OK))
				bytes_send = 0;

			return genode_errno(err);
		}

		Errno recvmsg(genode_msghdr &msg, unsigned long &bytes_recv,
		              bool msg_peek) override
		{
			bytes_recv = 0;

			if (!_recv_pbuf) {
				if (!_pcb || _pcb->state == Lwip::CLOSE_WAIT) {
					shutdown();
					return GENODE_ENONE;
				}

				/*
				 * EAGAIN if the PCB is active and there is nothing to read, otherwise
				 * return ENONE to indicate the connection is closed
				 */
				return (_state == READY) ? GENODE_EAGAIN : GENODE_ENONE;
			}

			bool        done   = false;
			Lwip::u16_t offset = 0;
			for_each_iovec(msg, [&](void *base, size_t size,
			                        unsigned long &used) {
				if (done) return;

				for_each_64k_chunk(base, size, [&] (void *chunk_base, uint16_t chunk_size) {

					if (done) return;

					Lwip::u16_t copied = Lwip::pbuf_copy_partial(_recv_pbuf, chunk_base,
					                                             chunk_size, offset);

					if (msg_peek)
						offset += copied;
					else
						_recv_pbuf = Lwip::pbuf_free_header(_recv_pbuf, copied);

					if (copied < chunk_size) done = true;

					used        = copied;
					bytes_recv += copied;
				});
			});

			/* ack remote */
			if (!msg_peek && _pcb)  {
					Lwip::tcp_recved(_pcb, bytes_recv);
					_recvbuf_consume(bytes_recv);
			}

			if (_state == CLOSING) shutdown();

			return bytes_recv == 0 ? GENODE_EAGAIN : GENODE_ENONE;
		}

		Errno peername(genode_sockaddr &addr) override {
			return _name(addr, false); }

		Errno name(genode_sockaddr &addr) override {
			return _name(addr, true); }

		unsigned poll() override
		{
			unsigned value = Poll::NONE;

			if ((_state == READY && _sendbuf_avail()) || _state == CLOSED)
				value |= Poll::WRITE;

			if (_recv_length > 0)
				value |= Poll::READ;

			if (_state == LISTEN && !_backlog.empty())
				value |= Poll::READ;

			if (_state == CLOSED || _state == CLOSING)
				value |= Poll::READ;

			return value;
		}

		Errno shutdown() override
		{
			_state = CLOSING;
			if (_recv_pbuf || !_pcb)
				return GENODE_ENONE;

			Lwip::tcp_arg(_pcb, NULL);
			Lwip::err_t err = Lwip::tcp_close(_pcb);
			_state = CLOSED;
			_pcb = nullptr;

			return genode_errno(err);
		}
};


Socket::Protocol *Socket::create_tcp(Genode::Allocator &alloc)
{
	return new (alloc) Tcp(alloc);
}
