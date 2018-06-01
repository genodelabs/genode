/*
 * \brief  Reflects the current router configuration through objects
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_

/* local includes */
#include <domain.h>
#include <report.h>

/* Genode includes */
#include <os/duration.h>

namespace Genode { class Allocator; }

namespace Net { class Configuration; }


class Net::Configuration
{
	private:

		using Mac_string = Genode::String<17>;

		Genode::Allocator          &_alloc;
		bool                 const  _verbose                 { false };
		bool                 const  _verbose_packets         { false };
		bool                 const  _verbose_domain_state    { false };
		Genode::Microseconds const  _dhcp_discover_timeout   { DEFAULT_DHCP_DISCOVER_TIMEOUT_SEC };
		Genode::Microseconds const  _dhcp_request_timeout    { DEFAULT_DHCP_REQUEST_TIMEOUT_SEC  };
		Genode::Microseconds const  _dhcp_offer_timeout      { DEFAULT_DHCP_OFFER_TIMEOUT_SEC    };
		Genode::Microseconds const  _icmp_idle_timeout       { DEFAULT_ICMP_IDLE_TIMEOUT_SEC     };
		Genode::Microseconds const  _udp_idle_timeout        { DEFAULT_UDP_IDLE_TIMEOUT_SEC      };
		Genode::Microseconds const  _tcp_idle_timeout        { DEFAULT_TCP_IDLE_TIMEOUT_SEC      };
		Genode::Microseconds const  _tcp_max_segm_lifetime   { DEFAULT_TCP_MAX_SEGM_LIFETIME_SEC };
		Pointer<Report>             _report                  { };
		Pointer<Genode::Reporter>   _reporter                { };
		Domain_tree                 _domains                 { };
		Genode::Xml_node     const  _node;

		void _invalid_domain(Domain     &domain,
		                     char const *reason);

	public:

		enum { DEFAULT_REPORT_INTERVAL_SEC       =   5 };
		enum { DEFAULT_DHCP_DISCOVER_TIMEOUT_SEC =  10 };
		enum { DEFAULT_DHCP_REQUEST_TIMEOUT_SEC  =  10 };
		enum { DEFAULT_DHCP_OFFER_TIMEOUT_SEC    =  10 };
		enum { DEFAULT_ICMP_IDLE_TIMEOUT_SEC     =  10 };
		enum { DEFAULT_UDP_IDLE_TIMEOUT_SEC      =  30 };
		enum { DEFAULT_TCP_IDLE_TIMEOUT_SEC      = 600 };
		enum { DEFAULT_TCP_MAX_SEGM_LIFETIME_SEC =  30 };

		Configuration(Genode::Xml_node const  node,
		              Genode::Allocator      &alloc);

		Configuration(Genode::Env            &env,
		              Genode::Xml_node const  node,
		              Genode::Allocator      &alloc,
		              Timer::Connection      &timer,
		              Configuration          &legacy);

		~Configuration();


		/***************
		 ** Accessors **
		 ***************/

		bool                  verbose()               const { return _verbose; }
		bool                  verbose_packets()       const { return _verbose_packets; }
		bool                  verbose_domain_state()  const { return _verbose_domain_state; }
		Genode::Microseconds  dhcp_discover_timeout() const { return _dhcp_discover_timeout; }
		Genode::Microseconds  dhcp_request_timeout()  const { return _dhcp_request_timeout; }
		Genode::Microseconds  dhcp_offer_timeout()    const { return _dhcp_offer_timeout; }
		Genode::Microseconds  icmp_idle_timeout()     const { return _icmp_idle_timeout; }
		Genode::Microseconds  udp_idle_timeout()      const { return _udp_idle_timeout; }
		Genode::Microseconds  tcp_idle_timeout()      const { return _tcp_idle_timeout; }
		Genode::Microseconds  tcp_max_segm_lifetime() const { return _tcp_max_segm_lifetime; }
		Domain_tree          &domains()                     { return _domains; }
		Report               &report()                      { return _report(); }
		Genode::Xml_node      node()                  const { return _node; }
};

#endif /* _CONFIGURATION_H_ */
