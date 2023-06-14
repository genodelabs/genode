/*
 * \brief  DHCP related definitions
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <net/dhcp.h>
#include <base/output.h>

using namespace Genode;

using Message_type = Net::Dhcp_packet::Message_type;


static char const *msg_type_to_string(Message_type type)
{
	switch (type) {
	case Message_type::DISCOVER: return "DISCOVER";
	case Message_type::OFFER   : return "OFFER";
	case Message_type::REQUEST : return "REQUEST";
	case Message_type::DECLINE : return "DECLINE";
	case Message_type::ACK     : return "ACK";
	case Message_type::NAK     : return "NAK";
	case Message_type::RELEASE : return "RELEASE";
	case Message_type::INFORM  : return "INFORM";
	}
	class Never_reached { };
	throw Never_reached { };
}


static char const *opcode_to_string(uint8_t op)
{
	switch (op) {
	case 1:  return "REPLY";
	case 2:  return "REQUEST";
	default: return "?";
	}
	class Never_reached { };
	throw Never_reached { };
}


void Net::Dhcp_packet::print(Genode::Output &output) const
{
	bool msg_type_found { false };
	for_each_option([&] (Option const &opt) {

		if (opt.code() == Option::Code::MSG_TYPE) {

			msg_type_found = true;
			Message_type_option const &msg_type {
				*reinterpret_cast<Message_type_option const *>(&opt) };

			Genode::print(output, "DHCP ",
			              msg_type_to_string(msg_type.value()), " ",
			              client_mac(), " > ", siaddr());
		}
	});
	if (!msg_type_found) {

		Genode::print(output, "DHCP ", opcode_to_string(op()),
		              " ", client_mac(), " > ", siaddr());
	}
}
