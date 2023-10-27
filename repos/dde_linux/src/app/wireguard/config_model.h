/*
 * \brief  A differentially updating model of the component configuration
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CONFIG_MODEL_H_
#define _CONFIG_MODEL_H_

/* base includes */
#include <util/list_model.h>
#include <util/reconstructible.h>
#include <base/allocator.h>

/* app/wireguard includes */
#include <genode_c_api/wireguard.h>
#include <base64.h>
#include <ipv4_address_prefix.h>

namespace Wireguard {

	class Config_model;
}


class Wireguard::Config_model
{
	private:

		using Key_base64          = Genode::String<WG_KEY_LEN_BASE64>;
		using Ipv4_address        = Net::Ipv4_address;
		using Ipv4_address_prefix = Net::Ipv4_address_prefix;
		using uint16_t            = Genode::uint16_t;

		class Peer;

		struct Config
		{
			Key_base64          private_key_b64;
			uint16_t            listen_port;
			Ipv4_address_prefix interface;

			static Config from_xml(Genode::Xml_node const &node)
			{
				return {
					.private_key_b64 = node.attribute_value("private_key", Key_base64 { }),
					.listen_port     = node.attribute_value("listen_port", (uint16_t)0U),
					.interface       = node.attribute_value("interface",   Ipv4_address_prefix { })
				};
			}

			bool operator != (Config const &other) const
			{
				return (private_key_b64 != other.private_key_b64)
				    || (listen_port     != other.listen_port)
				    || (interface       != other.interface);
			}
		};

		Genode::Allocator             &_alloc;
		Genode::Constructible<Config>  _config { };
		Genode::List_model<Peer>       _peers  { };

	public:

		Config_model(Genode::Allocator &alloc) : _alloc(alloc) { }

		void update(genode_wg_config_callbacks &callbacks,
		            Genode::Xml_node     const &config_node);
};


struct Wireguard::Config_model::Peer : Genode::List_model<Peer>::Element
{
		Key_base64          const public_key_b64;
		Ipv4_address        const endpoint_ip;
		uint16_t            const endpoint_port;
		Ipv4_address_prefix const allowed_ip;

		Peer(Key_base64          public_key_b64,
		     Ipv4_address        endpoint_ip,
		     uint16_t            endpoint_port,
		     Ipv4_address_prefix allowed_ip)
		:
			public_key_b64(public_key_b64), endpoint_ip(endpoint_ip),
			endpoint_port(endpoint_port), allowed_ip(allowed_ip)
		{ }

		bool matches(Genode::Xml_node const &node) const
		{
			return (endpoint_ip    == node.attribute_value("endpoint_ip",   Ipv4_address { }))
			    && (endpoint_port  == node.attribute_value("endpoint_port", (uint16_t)0U ))
			    && (public_key_b64 == node.attribute_value("public_key",    Key_base64 { }));
		}

		static bool type_matches(Genode::Xml_node const &node)
		{
			return node.has_type("peer");
		}
};

#endif /* _CONFIG_MODEL_H_ */
