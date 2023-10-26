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

		using Key_base64 = Genode::String<WG_KEY_LEN_BASE64>;

		class Peer;

		class Config
		{
			private:

				Key_base64 const               _private_key_b64;
				Genode::uint16_t const         _listen_port;
				Net::Ipv4_address_prefix const _interface;

			public:

				Config(Key_base64               private_key_b64,
				       Genode::uint16_t         listen_port,
				       Net::Ipv4_address_prefix interface);


				Key_base64               const &private_key_b64() const { return _private_key_b64; }
				Genode::uint16_t                listen_port()     const { return _listen_port; }
				Net::Ipv4_address_prefix const &interface()       const { return _interface; }
		};

		Genode::Allocator             &_alloc;
		Genode::Constructible<Config>  _config { };
		Genode::List_model<Peer>       _peers  { };

	public:

		Config_model(Genode::Allocator &alloc);

		void update(genode_wg_config_callbacks &callbacks,
		            Genode::Xml_node            config_node);
};


class Wireguard::Config_model::Peer : public Genode::List_model<Peer>::Element
{
	private:

		Key_base64               _public_key_b64;
		Net::Ipv4_address        _endpoint_ip;
		Genode::uint16_t         _endpoint_port;
		Net::Ipv4_address_prefix _allowed_ip;

	public:

		Peer(Key_base64               public_key_b64,
		     Net::Ipv4_address        endpoint_ip,
		     Genode::uint16_t         endpoint_port,
		     Net::Ipv4_address_prefix allowed_ip);

		Key_base64 public_key_b64() const { return _public_key_b64; }

		bool matches(Genode::Xml_node const &) const;

		static bool type_matches(Genode::Xml_node const &node)
		{
			return node.has_type("peer");
		}
};

#endif /* _CONFIG_MODEL_H_ */
