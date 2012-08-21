/*
 * \brief  I/O channel for sockets
 * \author Josef Söntgen
 * \date   2012-04-12
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


#ifndef _NOUX__SOCKET_IO_CHANNEL_H_
#define _NOUX__SOCKET_IO_CHANNEL_H_

/* Genode includes */
#include <base/printf.h>

/* Noux includes */
#include <io_channel.h>
#include <noux_session/sysio.h>

/* Libc includes */
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>


namespace Noux {

	class Socket_io_channel : public Io_channel
	{
		private:

			int _socket;

			enum { UNBLOCK_READ   = 0x1,
			       UNBLOCK_WRITE  = 0x2,
			       UNBLOCK_EXCEPT = 0x4 };

			int _unblock;

		public:

			Socket_io_channel()
			:
				_socket(-1),
				_unblock(0)
			{ }

			Socket_io_channel(int s)
			:
				_socket(s),
				_unblock(0)
			{ }

			~Socket_io_channel()
			{
				if (_socket != -1)
					::shutdown(_socket, SHUT_RDWR);
			}

			int get_socket() const
			{
				return _socket;
			}

			void set_unblock(bool rd, bool wr, bool ex)
			{
				if (rd)
					_unblock |= UNBLOCK_READ;
				if (wr)
					_unblock |= UNBLOCK_WRITE;
				if (ex)
					_unblock |= UNBLOCK_EXCEPT;
			}

			bool fstat(Sysio *sysio) { return false; }

			bool fcntl(Sysio *sysio)
			{
				switch (sysio->fcntl_in.cmd) {

				case Sysio::FCNTL_CMD_GET_FILE_STATUS_FLAGS:
				case Sysio::FCNTL_CMD_SET_FILE_STATUS_FLAGS:
				{
					int result = ::fcntl(_socket, sysio->fcntl_in.cmd,
					                     sysio->fcntl_in.long_arg);

					sysio->fcntl_out.result = result;
					return true;
				}
				default:
					PWRN("invalid fcntl command: %d", sysio->fcntl_in.cmd);
					sysio->error.fcntl = Sysio::FCNTL_ERR_CMD_INVALID;

					return false;
				}
			}

			bool fchdir(Sysio *sysio, Pwd *pwd) { return false; }
			bool dirent(Sysio *sysio)           { return false; }

			bool check_unblock(bool rd, bool wr, bool ex) const
			{
				if (_unblock & UNBLOCK_READ)
					return true;
				if (_unblock & UNBLOCK_WRITE)
					return true;
				if (_unblock & UNBLOCK_EXCEPT)
					return true;

				return false;
			}

			bool write(Sysio *sysio, size_t &count)
			{
				size_t result = ::write(_socket, sysio->write_in.chunk,
				                         sysio->write_in.count);

				if (result > -1) {
					sysio->write_out.count = result;
					count = result;

					return true;
				}

				return false;
			}

			bool read(Sysio *sysio)
			{
				size_t const max_count =
					Genode::min(sysio->read_in.count,
					            sizeof(sysio->read_out.chunk));

				sysio->read_out.count = ::read(_socket, sysio->read_out.chunk,
				                               max_count);

				return true;
			}

			bool socket(Sysio *sysio)
			{
				_socket = ::socket(sysio->socket_in.domain,
				                   sysio->socket_in.type,
				                   sysio->socket_in.protocol);

				return (_socket == -1) ? false : true;
			}

			bool getsockopt(Sysio *sysio)
			{
				int result = ::getsockopt(_socket, sysio->getsockopt_in.level,
				                          sysio->getsockopt_in.optname,
				                          sysio->getsockopt_in.optval,
				                          &sysio->getsockopt_in.optlen);

				return (result == -1) ? false : true;
			}

			bool setsockopt(Sysio *sysio)
			{
				/*
				 * Filter options out because lwip only supports several socket
				 * options. Therefore for now we silently return 0 and notify
				 * the user via debug message.
				 */
				switch (sysio->setsockopt_in.optname) {
				case SO_DEBUG:
				case SO_LINGER:
				case SO_REUSEADDR:

					PWRN("SOL_SOCKET option '%d' is currently not supported, however we report success",
					     sysio->setsockopt_in.optname);
					return true;
				}

				int result = ::setsockopt(_socket, sysio->setsockopt_in.level,
				                          sysio->setsockopt_in.optname,
				                          sysio->setsockopt_in.optval,
				                          sysio->setsockopt_in.optlen);

				return (result == -1) ? false : true;
			}

			int accept(Sysio *sysio)
			{
				int result;

				if (sysio->accept_in.addrlen == 0) {
					result = ::accept(_socket, NULL, NULL);
				}
				else {
					result = ::accept(_socket, (sockaddr *)&sysio->accept_in.addr,
					                  &sysio->accept_in.addrlen);
				}

				return result;
			}

			int bind(Sysio *sysio)
			{
				return ::bind(_socket, (const struct sockaddr *)&sysio->bind_in.addr,
				              sysio->bind_in.addrlen);
			}

			int connect(Sysio *sysio)
			{
				return ::connect(_socket, (struct sockaddr *)&sysio->connect_in.addr,
				                 sysio->connect_in.addrlen);
			}

			int getpeername(Sysio *sysio)
			{
				return ::getpeername(_socket, (struct sockaddr *)&sysio->getpeername_in.addr,
				                     (socklen_t *)&sysio->getpeername_in.addrlen);
			}

			bool ioctl(Sysio *sysio)
			{
				int result = ::ioctl(_socket, sysio->ioctl_in.request, NULL);

				return result ? false : true;
			}

			int listen(Sysio *sysio)
			{
				return ::listen(_socket, sysio->listen_in.backlog);
			}

			ssize_t recv(Sysio *sysio)
			{
				return ::recv(_socket, sysio->recv_in.buf, sysio->recv_in.len,
				               sysio->recv_in.flags);
			}

			ssize_t recvfrom(Sysio *sysio)
			{
				ssize_t result = ::recvfrom(_socket, sysio->recv_in.buf, sysio->recv_in.len,
				                            sysio->recv_in.flags, (struct sockaddr *)&sysio->recvfrom_in.src_addr,
				                            &sysio->recvfrom_in.addrlen);

				return result;
			}

			ssize_t send(Sysio *sysio)
			{
				return ::send(_socket, sysio->send_in.buf, sysio->send_in.len,
				               sysio->send_in.flags);
			}

			ssize_t sendto(Sysio *sysio)
			{
				ssize_t result = ::sendto(_socket, sysio->sendto_in.buf, sysio->sendto_in.len,
				                          sysio->sendto_in.flags,
				                          (const struct sockaddr *) &sysio->sendto_in.dest_addr,
				                          sysio->sendto_in.addrlen);

				return result;
			}

			int shutdown(Sysio *sysio)
			{
				return ::shutdown(_socket, sysio->shutdown_in.how);
			}
	};
}

#endif /* _NOUX__SOCKET_IO_CHANNEL_H_ */

