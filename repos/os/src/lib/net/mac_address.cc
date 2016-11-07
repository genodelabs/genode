/*
 * \brief  Media access control (MAC) address
 * \author Martin Stein
 * \date   2016-06-22
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <net/mac_address.h>
#include <util/token.h>
#include <util/string.h>

using namespace Net;
using namespace Genode;

struct Scanner_policy_number
{
	static bool identifier_char(char c, unsigned  i ) {
		return is_digit(c, true) && c !=':'; }
};

typedef Token<Scanner_policy_number> Mac_token;


Mac_address Net::mac_from_string(const char * mac)
{
	Mac_address   mac_addr;
	Mac_token     t(mac);
	char          tmpstr[3];
	int           cnt = 0;
	unsigned char ipb[6] = {0};

	while (t) {
		if (t.type() == Mac_token::WHITESPACE || t[0] == ':') {
			t = t.next();
			continue;
		}
		t.string(tmpstr, sizeof(tmpstr));

		unsigned long tmpc = 0;
		ascii_to_unsigned(tmpstr, tmpc, 16);
		ipb[cnt] = tmpc & 0xFF;
		t = t.next();

		if (cnt == 6) { break; }
		cnt++;
	}
	if (cnt == 6) {
		mac_addr.addr[0] = ipb[0];
		mac_addr.addr[1] = ipb[1];
		mac_addr.addr[2] = ipb[2];
		mac_addr.addr[3] = ipb[3];
		mac_addr.addr[4] = ipb[4];
		mac_addr.addr[5] = ipb[5];
	}
	return mac_addr;
}
