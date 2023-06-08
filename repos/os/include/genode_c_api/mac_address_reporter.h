/*
 * \brief  C-API Genode MAC address reporter utility
 * \author Christian Helmuth
 * \date   2023-06-06
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GENODE_C_API__MAC_ADDRESS_REPORTER_H_
#define _GENODE_C_API__MAC_ADDRESS_REPORTER_H_

#include <genode_c_api/base.h>


#ifdef __cplusplus

#include <util/xml_node.h>

void genode_mac_address_reporter_init(Genode::Env &, Genode::Allocator &);

void genode_mac_address_reporter_config(Genode::Xml_node const &);

#endif


#ifdef __cplusplus
extern "C" {
#endif

struct genode_mac_address { unsigned char addr[6]; };

void genode_mac_address_register(char const * name, struct genode_mac_address addr);

#ifdef __cplusplus
}
#endif

#endif /* _GENODE_C_API__MAC_ADDRESS_REPORTER_H_ */
