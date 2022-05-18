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

/* dde_linux wireguard includes */
#include <config_model.h>

using namespace Genode;
using namespace Net;
using namespace Wireguard;


/******************
 ** Config_model **
 ******************/

Config_model::Config_model(Genode::Allocator &alloc)
:
	_alloc { alloc }
{ }


void Config_model::update(genode_wg_config_callbacks &callbacks,
                          Xml_node                    node)
{
	if (!_config.constructed()) {

		_config.construct(
			node.attribute_value("private_key", Key_base64 { }),
			node.attribute_value("listen_port", (uint16_t)0U),
			node.attribute_value("interface", Ipv4_address_prefix { }));

		uint8_t private_key[WG_KEY_LEN];
		if (!_config->private_key_b64().valid() ||
			!key_from_base64(private_key, _config->private_key_b64().string())) {

			error("Invalid private key!");
		}
		callbacks.add_device(_config->listen_port(), private_key);
	}
	Peer_update_policy policy { _alloc, callbacks, _config->listen_port() };
	_peers.update_from_xml(policy, node);
}


/**************************
 ** Config_model::Config **
 **************************/

Config_model::Config::Config(Key_base64          private_key_b64,
                             uint16_t            listen_port,
                             Ipv4_address_prefix interface)
:
	_private_key_b64 { private_key_b64 },
	_listen_port     { listen_port },
	_interface       { interface }
{ }


/**************************************
 ** Config_model::Peer_update_policy **
 **************************************/

void Config_model::Peer_update_policy::update_element(Element  &,
                                                      Xml_node  )
{ }


Config_model::
Peer_update_policy::Peer_update_policy(Allocator                  &alloc,
                                       genode_wg_config_callbacks &callbacks,
                                       uint16_t                    listen_port)
:
	_alloc       { alloc },
	_callbacks   { callbacks },
	_listen_port { listen_port }
{ }


void Config_model::Peer_update_policy::destroy_element(Element &peer)
{
	_callbacks.remove_peer(
		_listen_port, peer._endpoint_ip.addr, peer._endpoint_port);

	destroy(_alloc, &peer);
}


Config_model::Peer_update_policy::Element &
Config_model::Peer_update_policy::create_element(Xml_node node)
{
	Ipv4_address        endpoint_ip    { node.attribute_value("endpoint_ip", Ipv4_address { }) };
	uint16_t            endpoint_port  { node.attribute_value("endpoint_port", (uint16_t)0U ) };
	Key_base64          public_key_b64 { node.attribute_value("public_key", Key_base64 { }) };
	Ipv4_address_prefix allowed_ip     { node.attribute_value("allowed_ip", Ipv4_address_prefix { }) };

	uint8_t public_key[WG_KEY_LEN];
	if (!public_key_b64.valid() ||
	    !key_from_base64(public_key, public_key_b64.string())) {

		error("Invalid public key!");
	}
	if (!allowed_ip.valid()) {
		error("Invalid allowed ip!");
	}
	_callbacks.add_peer(
		_listen_port, endpoint_ip.addr, endpoint_port, public_key,
		allowed_ip.address.addr, allowed_ip.prefix);

	return *(
		new (_alloc)
			Element(public_key_b64, endpoint_ip, endpoint_port, allowed_ip));
}


bool
Config_model::
Peer_update_policy::element_matches_xml_node(Element const &peer,
                                             Xml_node       node)
{
	Ipv4_address  endpoint_ip    { node.attribute_value("endpoint_ip", Ipv4_address { }) };
	uint16_t      endpoint_port  { node.attribute_value("endpoint_port", (uint16_t)0U ) };
	Key_base64    public_key_b64 { node.attribute_value("public_key", Key_base64 { }) };

	return
		(endpoint_ip    == peer._endpoint_ip) &&
		(endpoint_port  == peer._endpoint_port) &&
		(public_key_b64 == peer._public_key_b64);
}


bool Config_model::Peer_update_policy::node_is_element(Xml_node node)
{
	return node.has_type("peer");
}


/************************
 ** Config_model::Peer **
 ************************/

Config_model::Peer::Peer(Key_base64          public_key_b64,
                         Ipv4_address        endpoint_ip,
                         uint16_t            endpoint_port,
                         Ipv4_address_prefix allowed_ip)
:
	_public_key_b64 { public_key_b64 },
	_endpoint_ip    { endpoint_ip },
	_endpoint_port  { endpoint_port },
	_allowed_ip     { allowed_ip }
{ }
