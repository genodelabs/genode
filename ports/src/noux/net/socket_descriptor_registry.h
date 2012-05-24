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


#ifndef _NOUX__SOCKET_DESCRIPTOR_REGISTRY_H_
#define _NOUX__SOCKET_DESCRIPTOR_REGISTRY_H_

/* Noux includes */
#include <shared_pointer.h>

namespace Noux {

	enum { MAX_SOCKET_DESCRIPTORS = 64 };

	template<typename T>
	class Socket_descriptor_registry
	{
		private:

			struct {
				bool allocated;
				Shared_pointer<T> io_channel;
			} _sds[MAX_SOCKET_DESCRIPTORS];

			void _assign_sd(int sd, Shared_pointer<T> &io_channel)
			{
				_sds[sd].io_channel = io_channel;
				_sds[sd].allocated = true;
			}

			bool _is_valid_sd(int sd) const
			{
				return (sd >= 0) && (sd < MAX_SOCKET_DESCRIPTORS);
			}

			void _reset_sd(int sd)
			{
				_sds[sd].io_channel = Shared_pointer<T>();
				_sds[sd].allocated = false;
			}

		public:
			Socket_descriptor_registry()
			{
				for (unsigned i = 0; i < MAX_SOCKET_DESCRIPTORS; i++)
					_reset_sd(i);
			}

			int add_io_channel(Shared_pointer<T> io_channel, int sd = -1)
			{
				if (sd == -1) {
					PERR("Could not allocate socket descriptor");
					return -1;
				}

				if (!_is_valid_sd(sd)) {
					PERR("Socket descriptor %d is out of range", sd);
					return -2;
				}

				_assign_sd(sd, io_channel);
				return sd;
			}

			void remove_io_channel(int sd)
			{
				if (!_is_valid_sd(sd))
					PERR("Socket descriptor %d is out of range", sd);
				else
					_reset_sd(sd);
			}

			bool sd_in_use(int sd) const
			{
				return (_is_valid_sd(sd) && _sds[sd].allocated);
			}

			Shared_pointer<T> io_channel_by_sd(int sd) const
			{
				if (!sd_in_use(sd)) {
					PWRN("Socket descriptor %d is not open", sd);
					return Shared_pointer<T>();
				}

				return _sds[sd].io_channel;
			}

			static Socket_descriptor_registry<T> *instance()
			{
				static Socket_descriptor_registry<T> _sdr;
				return &_sdr;
			}
	};

};
#endif /* _NOUX__SOCKET_DESCRIPTOR_REGISTRY_H_ */
