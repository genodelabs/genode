/*
 * \brief  Tool for verifying detached signatures
 * \author Norman Feske
 * \date   2018-01-05
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <libc/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

/* local includes */
#include <gnupg.h>

namespace Verify {
	using namespace Genode;
	struct Main;
}


struct Verify::Main
{
	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	bool _verbose = false;

	Constructible<Reporter> _reporter { };

	typedef String<256> Path;
	typedef String<64>  Message;

	static Message _message(Gnupg_verify_result result)
	{
		switch(result) {
		case GNUPG_VERIFY_OK:                 return Message("good signature");
		case GNUPG_VERIFY_PUBKEY_UNAVAILABLE: return Message("public key unavailable");
		case GNUPG_VERIFY_PUBKEY_INVALID:     return Message("malformed public key");
		case GNUPG_VERIFY_SIGNATURE_INVALID:  return Message("bad signature");
		};
		return Message();
	}

	void _process_verify_node(Xml_node, Xml_generator &);
	void _handle_config_with_libc();
	void _handle_config() { Libc::with_libc([&] () { _handle_config_with_libc(); }); }

	Signal_handler<Main> _config_handler { _env.ep(), *this, &Main::_handle_config };

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Verify::Main::_process_verify_node(Xml_node node, Xml_generator &xml)
{
	Path const data_path   = node.attribute_value("path",   Path());
	Path const pubkey_path = node.attribute_value("pubkey", Path());

	Gnupg_verify_result const result =
		gnupg_verify_detached_signature(pubkey_path.string(),
		                                data_path.string(),
		                                Path(data_path, ".sig").string());
	if (_verbose)
		log(data_path, ": ", _message(result));

	bool const success = (result == GNUPG_VERIFY_OK);
	xml.node(success ? "good" : "bad", [&] () {
		xml.attribute("path", data_path);
		if (!success)
			xml.attribute("reason", _message(result));
	});
}


void Verify::Main::_handle_config_with_libc()
{
	Xml_node const config = _config.xml();

	_verbose = _config.xml().attribute_value("verbose", false);

	if (!_reporter.constructed()) {
		_reporter.construct(_env, "result");
		_reporter->enabled(true);
	}

	Reporter::Xml_generator xml(*_reporter, [&] () {
		config.for_each_sub_node("verify", [&] (Xml_node node) {
			_process_verify_node(node, xml); }); });
}


void Libc::Component::construct(Libc::Env &env) { static Verify::Main main(env); }
