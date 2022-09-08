/*
 * \brief  Verbosity configuration of the CBE tester
 * \author Martin Stein
 * \date   2020-10-29
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CBE_TESTER__VERBOSE_NODE_H_
#define _CBE_TESTER__VERBOSE_NODE_H_

/* Genode includes */
#include <util/xml_node.h>

class Verbose_node
{
	private:

		bool _cmd_pool_cmd_pending     { false };
		bool _cmd_pool_cmd_in_progress { false };
		bool _cmd_pool_cmd_completed   { false };
		bool _blk_io_req_in_progress   { false };
		bool _blk_io_req_completed     { false };
		bool _ta_req_in_progress       { false };
		bool _ta_req_completed         { false };
		bool _crypto_req_completed     { false };
		bool _crypto_req_in_progress   { false };
		bool _client_data_mismatch     { false };
		bool _client_data_transferred  { false };

	public:

		Verbose_node(Genode::Xml_node const &config)
		{
			config.with_optional_sub_node("verbose", [&] (Genode::Xml_node const &verbose)
			{
				_cmd_pool_cmd_pending     = verbose.attribute_value("cmd_pool_cmd_pending"    , false);
				_cmd_pool_cmd_in_progress = verbose.attribute_value("cmd_pool_cmd_in_progress", false);
				_cmd_pool_cmd_completed   = verbose.attribute_value("cmd_pool_cmd_completed"  , false);
				_blk_io_req_in_progress   = verbose.attribute_value("blk_io_req_in_progress"  , false);
				_blk_io_req_completed     = verbose.attribute_value("blk_io_req_completed"    , false);
				_ta_req_in_progress       = verbose.attribute_value("ta_req_in_progress"      , false);
				_ta_req_completed         = verbose.attribute_value("ta_req_completed"        , false);
				_crypto_req_completed     = verbose.attribute_value("crypto_req_completed"    , false);
				_crypto_req_in_progress   = verbose.attribute_value("crypto_req_in_progress"  , false);
				_client_data_mismatch     = verbose.attribute_value("client_data_mismatch"    , false);
				_client_data_transferred  = verbose.attribute_value("client_data_transferred" , false);
			});
		}

		bool cmd_pool_cmd_pending    () const { return _cmd_pool_cmd_pending    ; }
		bool cmd_pool_cmd_in_progress() const { return _cmd_pool_cmd_in_progress; }
		bool cmd_pool_cmd_completed  () const { return _cmd_pool_cmd_completed  ; }
		bool blk_io_req_in_progress  () const { return _blk_io_req_in_progress  ; }
		bool blk_io_req_completed    () const { return _blk_io_req_completed    ; }
		bool ta_req_in_progress      () const { return _ta_req_in_progress      ; }
		bool ta_req_completed        () const { return _ta_req_completed        ; }
		bool crypto_req_completed    () const { return _crypto_req_completed    ; }
		bool crypto_req_in_progress  () const { return _crypto_req_in_progress  ; }
		bool client_data_mismatch    () const { return _client_data_mismatch    ; }
		bool client_data_transferred () const { return _client_data_transferred ; }
};

#endif /* _CBE_TESTER__VERBOSE_NODE_H_ */
