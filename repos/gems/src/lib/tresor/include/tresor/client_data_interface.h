/*
 * \brief  Interface for providing access to the client request data
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__CLIENT_DATA_INTERFACE_H_
#define _TRESOR__CLIENT_DATA_INTERFACE_H_

/* tresor includes */
#include <tresor/types.h>

namespace Tresor { class Client_data_interface; }

struct Tresor::Client_data_interface : Interface
{
	struct Obtain_data_attr
	{
		Request_offset const in_req_off;
		Request_tag const in_req_tag;
		Physical_block_address const in_pba;
		Virtual_block_address const in_vba;
		Block &out_blk;
	};

	virtual void obtain_data(Obtain_data_attr const &) = 0;

	struct Supply_data_attr
	{
		Request_offset const in_req_off;
		Request_tag const in_req_tag;
		Physical_block_address const in_pba;
		Virtual_block_address const in_vba;
		Block const &in_blk;
	};

	virtual void supply_data(Supply_data_attr const &) = 0;
};

#endif /* _TRESOR__CLIENT_DATA_INTERFACE_H_ */
