/*
 * \brief  Module that provides access to the client request data
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__CLIENT_DATA_H_
#define _TRESOR__CLIENT_DATA_H_

/* tresor includes */
#include <tresor/types.h>

namespace Tresor { class Client_data_request; }

namespace Vfs_tresor { class Client_data; }

namespace Tresor_tester { class Client_data; }

class Tresor::Client_data_request : public Module_request
{
	friend class ::Vfs_tresor::Client_data;
	friend class ::Tresor_tester::Client_data;

	public:

		enum Type { OBTAIN_PLAINTEXT_BLK, SUPPLY_PLAINTEXT_BLK };

	private:

		Type const _type;
		Request_offset const _req_off;
		Request_tag const _req_tag;
		Physical_block_address const _pba;
		Virtual_block_address const _vba;
		Block &_blk;
		bool &_success;

		NONCOPYABLE(Client_data_request);

	public:

		Client_data_request(Module_id src_mod_id, Module_channel_id src_chan_id, Type type,
		                    Request_offset req_off, Request_tag req_tag, Physical_block_address pba,
		                    Virtual_block_address vba, Block &blk, bool &success)
		:
			Module_request { src_mod_id, src_chan_id, CLIENT_DATA }, _type { type }, _req_off { req_off },
			_req_tag { req_tag }, _pba { pba }, _vba { vba }, _blk { blk }, _success { success }
		{ }

		static char const *type_to_string(Type type)
		{
			switch (type) {
			case OBTAIN_PLAINTEXT_BLK: return "obtain_plaintext_blk";
			case SUPPLY_PLAINTEXT_BLK: return "supply_plaintext_blk";
			}
			ASSERT_NEVER_REACHED;
		}

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};

#endif /* _TRESOR__CLIENT_DATA_H_ */
