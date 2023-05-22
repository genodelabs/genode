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
#include <tresor/module.h>

namespace Tresor {

	class Client_data_request;
}

namespace Vfs_tresor {

	class Wrapper;
}

namespace Tresor_tester {

	class Main;
}

class Tresor::Client_data_request : public Module_request
{
	public:

		enum Type { INVALID, OBTAIN_PLAINTEXT_BLK, SUPPLY_PLAINTEXT_BLK };

	private:

		friend class ::Vfs_tresor::Wrapper;
		friend class ::Tresor_tester::Main;

		Type     _type              { INVALID };
		uint64_t _client_req_offset { 0 };
		uint64_t _client_req_tag    { 0 };
		uint64_t _pba               { 0 };
		uint64_t _vba               { 0 };
		addr_t   _plaintext_blk_ptr { 0 };
		bool     _success           { false };

	public:

		Client_data_request() { }

		Type type() const { return _type; }


		/*****************************************************
		 ** can be removed once the tresor translation is done **
		 *****************************************************/

		Client_data_request(Module_id         src_module_id,
		                    Module_request_id src_request_id,
		                    Type          type,
		                    uint64_t      client_req_offset,
		                    uint64_t      client_req_tag,
		                    uint64_t      pba,
		                    uint64_t      vba,
		                    addr_t        plaintext_blk_ptr)
		:
			Module_request     { src_module_id, src_request_id, CLIENT_DATA },
			_type              { type },
			_client_req_offset { client_req_offset },
			_client_req_tag    { client_req_tag },
			_pba               { pba },
			_vba               { vba },
			_plaintext_blk_ptr { plaintext_blk_ptr }
		{ }

		bool success() const { return _success; }

		static char const *type_to_string(Type type)
		{
			switch (type) {
			case INVALID: return "invalid";
			case OBTAIN_PLAINTEXT_BLK: return "obtain_plaintext_blk";
			case SUPPLY_PLAINTEXT_BLK: return "supply_plaintext_blk";
			}
			return "?";
		}


		/********************
		 ** Module_request **
		 ********************/

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};

#endif /* _TRESOR__CLIENT_DATA_H_ */
