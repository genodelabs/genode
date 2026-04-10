/*
 * \brief  NIC driver for Linux using Libslirp
 * \author Johannes Schlatow
 * \date   2026-04-01
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__NIC__LIBSLIRP_CONTEXT_H_
#define _DRIVERS__NIC__LIBSLIRP_CONTEXT_H_

/* Genode */
#include <base/log.h>
#include <util/interface.h>
#include <net/port.h>
#include <net/ipv4.h>

/* Linux */
#include <slirp/libslirp.h>
#include <poll.h>
#include <stdlib.h>
#include <time.h>

#if SLIRP_CONFIG_VERSION_MAX < 4
#error "Requires libslirp >= 4.7.0"
#endif

namespace Libslirp {
	struct Action;
	class  Context;
}

/*
 * Interface for sending packet data to uplink.
 */
struct Libslirp::Action : Genode::Interface
{
	virtual size_t send(const void *buf, size_t len) = 0;
};


class Libslirp::Context
{
	private:

		bool         _verbose;
		Action      &_action;
		SlirpConfig  _config { };
		Slirp       *_slirp  { nullptr };

		static Context &_context(void *opaque) {
			return *(Context*)opaque; }

		/*
		 * Poll fd management
		 */
		struct pollfd *_fds      { nullptr };
		size_t         _fds_len  { 0 };
		size_t         _fds_size { 0 };

		static int _add_poll(int fd, int events, void *opaque)
		{
			enum { NUM_NEW_FDS = 16 };

			if (!opaque)
				return -1;

			Context &ctx = _context(opaque);

			if (ctx._fds_len >= ctx._fds_size) {
				ctx._fds_size += NUM_NEW_FDS;
				ctx._fds = (struct pollfd*)reallocarray(ctx._fds, ctx._fds_size, sizeof(struct pollfd));

				if (!ctx._fds) {
					ctx._fds_size = 0;
					return -1;
				}
			}

			size_t const i = ctx._fds_len++;
			ctx._fds[i].fd     = fd;
			ctx._fds[i].events = 0;

			if (events & SLIRP_POLL_IN)  ctx._fds[i].events |= POLLIN;
			if (events & SLIRP_POLL_OUT) ctx._fds[i].events |= POLLOUT;
			if (events & SLIRP_POLL_PRI) ctx._fds[i].events |= POLLPRI;
			if (events & SLIRP_POLL_ERR) ctx._fds[i].events |= POLLERR;
			if (events & SLIRP_POLL_HUP) ctx._fds[i].events |= POLLHUP;

			return (int)i;
		}


		static int _get_revents(int idx, void *opaque)
		{
			if (!opaque)
				return 0;

			int       result  = 0;
			int const revents = _context(opaque)._fds[idx].revents;
			if (revents & POLLIN)  result |= SLIRP_POLL_IN;
			if (revents & POLLOUT) result |= SLIRP_POLL_OUT;
			if (revents & POLLPRI) result |= SLIRP_POLL_PRI;
			if (revents & POLLERR) result |= SLIRP_POLL_ERR;
			if (revents & POLLHUP) result |= SLIRP_POLL_HUP;

			return result;
		}

		/*
		 * Callbacks
		 */

		static ssize_t _send_packet(const void *buf, size_t len, void *opaque)
		{
			if (!opaque) return 0;

			return _context(opaque).action().send(buf, len);
		}

		static void _guest_error(const char *msg, void *) {
			Genode::error(Genode::Cstring(msg)); }

		static int64_t _clock_get_ns(void *)
		{
			struct timespec ts;
			clock_gettime(CLOCK_MONOTONIC, &ts);
			return ts.tv_sec * 1000LL * 1000LL * 1000LL + ts.tv_nsec;
		}
		
		static void _init_completed(Slirp *, void *opaque) {
			if (opaque && _context(opaque)._verbose)
				Genode::log("Slirp stack has been initialized.");
		}

		static void *_timer_new_opaque(SlirpTimerId, void *, void *)
		{
			Genode::error(__func__, " unexpectedly called");
			return nullptr;
		}

		static void _timer_free(void *, void *) {
			Genode::warning(__func__, " not implemented"); }

		static void _timer_mod(void *, int64_t, void *) {
			Genode::warning(__func__, " not implemented"); }

		static void _notify(void *) {
			Genode::warning(__func__, " not implemented"); }

		static void _register_poll_fd(int, void *) { }

		static void _unregister_poll_fd(int, void *) { }

		struct SlirpCb _callbacks = {
			.send_packet        = Context::_send_packet,
			.guest_error        = Context::_guest_error,
			.clock_get_ns       = Context::_clock_get_ns,
			.timer_new          = nullptr,                      /* superseded by timer_new_opaque */
			.timer_free         = Context::_timer_free,         /* only needed for IPv6 */
			.timer_mod          = Context::_timer_mod,          /* only needed for IPv6 */
			.register_poll_fd   = Context::_register_poll_fd,   /* deprecated */
			.unregister_poll_fd = Context::_unregister_poll_fd, /* deprecated */
			.notify             = Context::_notify,             /* needed for guestfwd */

			/* fields introduced with config version 4 */
			.init_completed     = Context::_init_completed,
			.timer_new_opaque   = Context::_timer_new_opaque,
		};

		/* make non-copyable because of pointer members */
		Context(Context const &) = delete;
		Context & operator = (Context const &) = delete;

	public:

		Context(Action &action, Net::Ipv4_address network, bool verbose)
		: _verbose(verbose),
		  _action(action)
		{
			_config.version = 4;

			_config.restricted = 0; /* allow access to internet */

			_config.in_enabled = 1; /* enable ipv4 */
			_config.vnetwork   = { network.to_uint32_big_endian() };
			inet_pton(AF_INET, "255.255.255.0", &(_config.vnetmask));

			Net::Ipv4_address host_ip { network };
			host_ip.addr[3] = 2; /* default host ip: 10.0.2.2 */
			_config.vhost      = { host_ip.to_uint32_big_endian() };

			_config.in6_enabled = 0; /* disable ipv6 */

			/* hostname exposed in DHCP hostname option */
			_config.vhostname = "slirp_nic";

			/* no tftp */
			_config.tftp_server_name = NULL;
			_config.tftp_path        = NULL;
			_config.bootfile         = NULL;

			Net::Ipv4_address guest_ip { network };
			guest_ip.addr[3]   = 15; /* default guest ip: 10.0.2.15 */

			Net::Ipv4_address nameserver { network };
			nameserver.addr[3] = 3; /* default guest ip: 10.0.2.3 */

			_config.vdhcp_start = { guest_ip.to_uint32_big_endian() };
			_config.vnameserver = { nameserver.to_uint32_big_endian() };

			/* no domain name and search domain */
			_config.vdnssearch  = NULL;
			_config.vdomainname = NULL;

			_config.if_mtu = 0; /* use default MTU */
			_config.if_mru = 0; /* use default MRU */
			_config.disable_host_loopback = 0; /* allow access to 127.0.0.1 */

			/*
			 * Note: Debug output must be enabled in glib via G_MESSAGES_DEBUG=Slirp
			 *       in base-linux/src/core/native_pd_component.cc
			 */

#if SLIRP_CONFIG_VERSION_MAX >= 6
			if (verbose)
				slirp_set_debug(SLIRP_DBG_CALL  |
				                SLIRP_DBG_MISC  |
				                SLIRP_DBG_ERROR |
				                SLIRP_DBG_TFTP  |
				                SLIRP_DBG_VERBOSE_CALL);
#else
			if (verbose)
				Genode::warning("Unable to enable libslirp debugging (requires libslirp >= 4.9.0)");
#endif

			_slirp = slirp_new(&_config, &_callbacks, this);

		}

		~Context()
		{
			if (_slirp)
				slirp_cleanup(_slirp);
		}

		bool valid() const { return _slirp != nullptr; }

		Net::Ipv4_address guest_ip() const {
			return Net::Ipv4_address::from_uint32_big_endian(_config.vdhcp_start.s_addr); }

		void add_hostfwd(Net::Port port, Net::Ipv4_address to, Net::Port to_port, bool udp)
		{
			if (!valid()) {
				Genode::error("Unable to add port-forwarding rule to uninitialized slirp stack.");
				return;
			}

			struct in_addr host_addr  { 0 };
			struct in_addr guest_addr { to.to_uint32_big_endian() };

			if (slirp_add_hostfwd(_slirp, udp, host_addr, port.value, guest_addr, to_port.value) != 0)
				Genode::error("slirp_add_hostfwd() failed");
		}

		size_t input(const uint8_t *base, int len)
		{
			if (!valid())
				return 0;

			slirp_input(_slirp, base, len);

			return len;
		}

		void poll_loop()
		{
			while (valid()) {

				uint32_t timeout { UINT32_MAX };
				slirp_pollfds_fill(_slirp, &timeout, Context::_add_poll, this);

				/*
				 * This would be the place to update timeout with the earliest
				 * timeout among the registered timers
				 */

				int const pollout = poll(_fds, _fds_len, timeout);

				slirp_pollfds_poll(_slirp, pollout <= 0, Context::_get_revents, this);

				/*
				 * This would be the place to call handlers for expired timers.
				 */

			}
		}

		Action &action() { return _action; }
};

#endif /* _DRIVERS__NIC__LIBSLIRP_CONTEXT_H_ */
