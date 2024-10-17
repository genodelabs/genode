 /*
 * \author Josef Soentgen
 * \date   2018-07-31
 *
 * Wifi manager uses the CTRL interface of the wpa_supplicant via a Genode
 * specific ctrl_iface implementation that comprises two distinct memory
 * buffers for communication - one for the command results and one for events.
 */

/*
 * Copyright (C) 2018-2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _WIFI_MANAGER_H_
#define _WIFI_MANAGER_H_

/* Genode includes */
#include <libc/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/registry.h>
#include <base/sleep.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <util/attempt.h>
#include <util/list_model.h>
#include <util/interface.h>
#include <util/xml_node.h>

/* rep includes */
#include <wifi/ctrl.h>
#include <wifi/rfkill.h>
using Ctrl_msg_buffer = Wifi::Msg_buffer;

/* local includes */
#include <util.h>

using namespace Genode;

/* internal interface of lib/wifi/socket_call.cc */
extern void wifi_kick_socketcall();


namespace Wifi {
	struct Manager;
}


enum Recv_msg_index : unsigned char {
	OK = 0,
	FAIL,
	SCAN_RESULTS,
	CONNECTED,
	DISCONNECTED,
	SME_AUTH,
	NOT_FOUND,
	MAX_INDEX, /* keep as last entry */
};


static struct {
	char const *string;
	size_t len;
} recv_table[Recv_msg_index::MAX_INDEX] = {
	{ "OK",                            2 },
	{ "FAIL",                          4 },
	{ "CTRL-EVENT-SCAN-RESULTS",      23 },
	{ "CTRL-EVENT-CONNECTED",         20 },
	{ "CTRL-EVENT-DISCONNECTED",      23 },
	{ "SME: Trying to authenticate",  27 },
	{ "CTRL-EVENT-NETWORK-NOT-FOUND", 28 },
};


static inline bool check_recv_msg(char const *msg,
                                  auto const &entry) {
	return Genode::strcmp(entry.string, msg, entry.len) == 0; }


static bool cmd_successful(char const *msg) {
	return check_recv_msg(msg, recv_table[Recv_msg_index::OK]); }


static bool cmd_fail(char const *msg) {
	return check_recv_msg(msg, recv_table[Recv_msg_index::FAIL]); }


static bool results_available(char const *msg) {
	return check_recv_msg(msg, recv_table[Recv_msg_index::SCAN_RESULTS]); }


static bool connecting_to_network(char const *msg) {
	return check_recv_msg(msg, recv_table[Recv_msg_index::SME_AUTH]); }


static bool network_not_found(char const *msg) {
	return check_recv_msg(msg, recv_table[Recv_msg_index::NOT_FOUND]); }


static bool disconnected_from_network(char const *msg) {
	return check_recv_msg(msg, recv_table[Recv_msg_index::DISCONNECTED]); }


static bool connected_to_network(char const *msg) {
	return check_recv_msg(msg, recv_table[Recv_msg_index::CONNECTED]); }


static bool scan_results(char const *msg) {
	return Genode::strcmp("bssid", msg, 5) == 0; }


using Cmd = String<sizeof(Wifi::Msg_buffer::send)>;
static void ctrl_cmd(Ctrl_msg_buffer &msg, Cmd const &cmd)
{
	memcpy(msg.send, cmd.string(), cmd.length());
	++msg.send_id;

	wpa_ctrl_set_fd();

	/*
	 * We might have to pull the socketcall task out of poll_all()
	 * because otherwise we might be late and wpa_supplicant has
	 * already removed all scan results due to BSS age settings.
	 */
	wifi_kick_socketcall();
}


/*
 * The Accesspoint object contains all information to join
 * a wireless network.
 */
struct Accesspoint : Interface
{
	using Bssid = String<17+1>;
	using Freq  = String< 4+1>;
	using Prot  = String< 7+1>;
	using Ssid  = String<32+1>;
	using Pass  = String<63+1>;

	static bool valid(Ssid const &ssid) {
		return ssid.length() > 1 && ssid.length() <= 32 + 1; }

	static bool valid(Pass const &pass) {
		return pass.length() > 8 && pass.length() <= 63 + 1; }

	static bool valid(Bssid const &bssid) {
		return bssid.length() == 17 + 1; }

	/*
	 * Accesspoint information fields used by manager
	 */
	Bssid    bssid   { };
	Freq     freq    { };
	Prot     prot    { };
	Ssid     ssid    { };
	Pass     pass    { };
	unsigned quality { 0 };

	static Accesspoint from_xml(Xml_node const &node)
	{
		Accesspoint ap { };

		ap.ssid  = node.attribute_value("ssid",  Accesspoint::Ssid());
		ap.bssid = node.attribute_value("bssid", Accesspoint::Bssid());

		ap.pass = node.attribute_value("passphrase", Accesspoint::Pass(""));
		ap.prot = node.attribute_value("protection", Accesspoint::Prot("NONE"));

		return ap;
	}

	/*
	 * CTRL interface fields
	 */
	int id { -1 };

	/**
	 * Default constructor
	 */
	Accesspoint() { }

	/**
	 * Constructor that initializes SSID fields
	 *
	 * It is used by the join-state handling to construct
	 * the connect/connecting AP.
	 */
	Accesspoint(Bssid const &bssid, Ssid const &ssid)
	: bssid { bssid }, ssid { ssid } { }

	/**
	 * Constructor that initializes information fields
	 *
	 * It is used when parsing the scan results into an
	 * AP.
	 */
	Accesspoint(char const *bssid, char const *freq,
	            char const *prot,  char const *ssid, unsigned quality)
	: bssid(bssid), freq(freq), prot(prot), ssid(ssid), quality(quality)
	{ }

	void print(Output &out) const
	{
		Genode::print(out, "SSID: '",         ssid, "'",  " "
		                   "BSSID: '",        bssid, "'", " "
		                   "protection: ",    prot, " "
		                   "id: ",            id, " "
		                   "quality: ",       quality);
	}

	bool wpa()    const { return prot != "NONE"; }
	bool wpa3()   const { return prot == "WPA3"; }
	bool stored() const { return id != -1; }

	bool updated_from(Accesspoint const &other)
	{
		bool const update = ((Accesspoint::valid(other.bssid) && other.bssid != bssid)
		                 || pass          != other.pass
		                 || prot          != other.prot);
		if (!update)
			return false;

		if (Accesspoint::valid(other.bssid))
			bssid = other.bssid;

		pass = other.pass;
		prot = other.prot;
		return true;
	}
};


struct Network : List_model<Network>::Element
{

	Accesspoint _accesspoint { };

	Network(Accesspoint const &ap) : _accesspoint { ap } { }

	virtual ~Network() { }

	void with_accesspoint(auto const &fn) {
		fn(_accesspoint); }

	void with_accesspoint(auto const &fn) const {
		fn(_accesspoint); }

	/**************************
	 ** List_model interface **
	 **************************/

	static bool type_matches(Xml_node const &node) {
		return node.has_type("network"); }

	bool matches(Xml_node const &node) {
		return _accesspoint.ssid == node.attribute_value("ssid", Accesspoint::Ssid()); }
};


struct Explicit_scan : List_model<Explicit_scan>::Element
{

	Accesspoint::Ssid _ssid { };

	Explicit_scan(Accesspoint::Ssid const &ssid) : _ssid { ssid } { }

	virtual ~Explicit_scan() { }

	void with_ssid(auto const &fn) {
		if (Accesspoint::valid(_ssid))
			fn(_ssid);
	}

	void with_ssid(auto const &fn) const {
		if (Accesspoint::valid(_ssid))
			fn(_ssid);
	}

	/**************************
	 ** List_model interface **
	 **************************/

	static bool type_matches(Xml_node const &node) {
		return node.has_type("explicit_scan"); }

	bool matches(Xml_node const &node) {
		return _ssid == node.attribute_value("ssid", Accesspoint::Ssid()); }
};


static void for_each_line(char const *msg, auto const &fn)
{
	char line_buffer[1024];
	size_t cur = 0;

	while (msg[cur] != 0) {
		size_t until = Util::next_char(msg, cur, '\n');
		if (until >= sizeof(line_buffer)) {
			Genode::error(__func__, ": line too large, abort processing");
			return;
		}
		memcpy(line_buffer, &msg[cur], until);
		line_buffer[until] = 0;
		cur += until + 1;

		fn(line_buffer);
	}
}


static void for_each_result_line(char const *msg, auto const &fn)
{
	char line_buffer[1024];
	size_t cur = 0;

	/* skip headline */
	size_t until = Util::next_char(msg, cur, '\n');
	cur += until + 1;

	while (msg[cur] != 0) {
		until = Util::next_char(msg, cur, '\n');
		if (until >= sizeof(line_buffer)) {
			Genode::error(__func__, ": line too large, abort processing");
			return;
		}
		memcpy(line_buffer, &msg[cur], until);
		line_buffer[until] = 0;
		cur += until + 1;

		char const *s[5] = { };

		for (size_t c = 0, i = 0; i < 5; i++) {
			size_t pos = Util::next_char(line_buffer, c, '\t');
			line_buffer[c+pos] = 0;
			s[i] = (char const*)&line_buffer[c];
			c += pos + 1;
		}

		bool const is_wpa1 = Util::string_contains((char const*)s[3], "WPA");
		bool const is_wpa2 = Util::string_contains((char const*)s[3], "WPA2");
		bool const is_wpa3 = Util::string_contains((char const*)s[3], "SAE");

		unsigned const quality = Util::approximate_quality(s[2]);

		char const *prot = is_wpa1 ? "WPA" : "NONE";
		            prot = is_wpa2 ? "WPA2" : prot;
		            prot = is_wpa3 ? "WPA3" : prot;

		fn(Accesspoint(s[0], s[1], prot, s[4], quality));
	}
}


struct Action : Fifo<Action>::Element
{
	enum class Type    : unsigned { COMMAND, QUERY };
	enum class Command : unsigned {
		INVALID, ADD, DISABLE, ENABLE, EXPLICIT_SCAN,
		LOG_LEVEL, REMOVE, SCAN, SCAN_RESULTS, SET, UPDATE, };
	enum class Query   : unsigned {
		INVALID, BSS, RSSI, STATUS, };

	Type    const type;
	Command const command;
	Query   const query;

	bool successful;

	Action(Command cmd)
	:
		type       { Type::COMMAND },
		command    { cmd },
		query      { Query::INVALID },
		successful { true }
	{ }

	Action(Query query)
	:
		type       { Type::QUERY },
		command    { Command::INVALID },
		query      { query },
		successful { true }
	{ }

	virtual void execute() { }

	virtual void check(char const *) { }

	virtual void response(char const *, Accesspoint &) { }

	virtual bool complete() const = 0;

	virtual void print(Genode::Output &) const = 0;
};


/*
 * Action for adding a new network
 *
 * In case the 'auto_connect' option is set for the network it
 * will also be enabled to active auto-joining.
 */
struct Add_network_cmd : Action
{
	enum class State : unsigned {
		INIT, ADD_NETWORK, FILL_NETWORK_SSID, FILL_NETWORK_BSSID,
		FILL_NETWORK_KEY_MGMT, SET_NETWORK_PMF, FILL_NETWORK_PSK,
		ENABLE_NETWORK, COMPLETE
	};

	Ctrl_msg_buffer &_msg;
	Accesspoint      _accesspoint;
	State            _state;

	Add_network_cmd(Ctrl_msg_buffer &msg, Accesspoint const &ap)
	:
		Action       { Command::ADD },
		_msg         { msg },
		_accesspoint { ap },
		_state       { State::INIT }
	{ }

	void print(Output &out) const override
	{
		Genode::print(out, "Add_network_cmd[", (unsigned)_state,
		                   "] '", _accesspoint.ssid, "'");
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("ADD_NETWORK"));
			_state = State::ADD_NETWORK;
			break;
		case State::ADD_NETWORK:
			ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
			                       " ssid \"", _accesspoint.ssid, "\""));
			_state = State::FILL_NETWORK_SSID;
			break;
		case State::FILL_NETWORK_SSID:
		{
			bool const valid = Accesspoint::valid(_accesspoint.bssid);
			char const *bssid = valid ? _accesspoint.bssid.string() : "";

			ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
			                       " bssid ", bssid));
			_state = State::FILL_NETWORK_BSSID;
			break;
		}
		case State::FILL_NETWORK_BSSID:
			if (_accesspoint.wpa3()) {
				ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
				                       " key_mgmt SAE"));
				_state = State::FILL_NETWORK_KEY_MGMT;
			} else {
				if (_accesspoint.wpa())
					ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
					                       " psk \"", _accesspoint.pass, "\""));
				else
					ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
					                       " key_mgmt NONE"));
				_state = State::FILL_NETWORK_PSK;
			}
			break;
		case State::FILL_NETWORK_KEY_MGMT:
			ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
			         " ieee80211w 2"));
			_state = State::SET_NETWORK_PMF;
			break;
		case State::SET_NETWORK_PMF:
			ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
			                       " psk \"", _accesspoint.pass, "\""));
			_state = State::FILL_NETWORK_PSK;
			break;
		case State::FILL_NETWORK_PSK:
			ctrl_cmd(_msg, Cmd("ENABLE_NETWORK ", _accesspoint.id));
			_state = State::ENABLE_NETWORK;
			break;
		case State::ENABLE_NETWORK:
			_state = State::COMPLETE;
			break;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		/*
		 * Handle response by expected failure handling
		 * and use fallthrough switch cases to reduce code.
		 */
		switch (_state) {
		case State::INIT: break;

		case State::ADD_NETWORK:
			if (cmd_fail(msg)) {
				error("ADD_NETWORK(", (unsigned)_state, ") failed: ", msg);
				Action::successful = false;
				complete = true;
			}
			break;

		case State::FILL_NETWORK_SSID:     [[fallthrough]];
		case State::FILL_NETWORK_BSSID:    [[fallthrough]];
		case State::FILL_NETWORK_KEY_MGMT: [[fallthrough]];
		case State::SET_NETWORK_PMF:       [[fallthrough]];
		case State::FILL_NETWORK_PSK:      [[fallthrough]];
		case State::ENABLE_NETWORK:
			if (!cmd_successful(msg)) {
				error("ADD_NETWORK(", (unsigned)_state, ") failed: ", msg);
				Action::successful = false;
				complete = true;
			}
			break;
		case State::COMPLETE: break;
		}

		if (complete) {
			_state = State::COMPLETE;
			return;
		}

		switch (_state) {
		case State::INIT: break;
		case State::ADD_NETWORK:
		{
			long id = -1;
			ascii_to(msg, id);
			_accesspoint.id = static_cast<int>(id);
			break;
		}
		case State::FILL_NETWORK_SSID:     break;
		case State::FILL_NETWORK_BSSID:    break;
		case State::FILL_NETWORK_KEY_MGMT: break;
		case State::SET_NETWORK_PMF:       break;
		case State::FILL_NETWORK_PSK:      break;
		case State::ENABLE_NETWORK:        break;
		case State::COMPLETE:              break;
		}
	}

	Accesspoint const &accesspoint() const
	{
		return _accesspoint;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for removing a network
 */
struct Remove_network_cmd : Action
{
	enum class State : unsigned {
		INIT, REMOVE_NETWORK, COMPLETE
	};

	Ctrl_msg_buffer &_msg;
	int              _id;
	State            _state;

	Remove_network_cmd(Ctrl_msg_buffer &msg, int id)
	:
		Action      { Command::REMOVE },
		_msg        { msg },
		_id         { id },
		_state      { State::INIT }
	{ }

	void print(Output &out) const override
	{
		Genode::print(out, "Remove_network_cmd[", (unsigned)_state, "] id: ", _id);
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("REMOVE_NETWORK ", _id));
			_state = State::REMOVE_NETWORK;
			break;
		case State::REMOVE_NETWORK:
			_state = State::COMPLETE;
			break;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		switch (_state) {
		case State::INIT: break;
		case State::REMOVE_NETWORK:
			if (cmd_fail(msg)) {
				error("could not remove network: ", msg);
				Action::successful = false;
				complete = true;
			}
			break;
		case State::COMPLETE: break;
		}

		if (complete)
			_state = State::COMPLETE;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for updating a network
 *
 * For now only the PSK is updated.
 */
struct Update_network_cmd : Action
{
	enum class Op : unsigned {
		UPDATE_ALL, DISABLE_ONLY };
	enum class State : unsigned {
		INIT, UPDATE_NETWORK_PSK,
		DISABLE_NETWORK, ENABLE_NETWORK, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	Accesspoint      _accesspoint;
	State            _state;
	Op               _op;

	Update_network_cmd(Ctrl_msg_buffer   &msg,
	                   Accesspoint const &ap,
	                   Op                 op = Op::UPDATE_ALL)
	:
		Action       { Command::UPDATE },
		_msg         { msg },
		_accesspoint { ap },
		_state       { State::INIT },
		_op          { op }
	{ }

	void print(Output &out) const override
	{
		Genode::print(out, "Update_network_cmd[", (unsigned)_state,
		                   "] id: ", _accesspoint.id);
	}

	void execute() override
	{
		// XXX change to disable -> psk ?-> enable
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
			                       " psk \"", _accesspoint.pass, "\""));
			_state = State::UPDATE_NETWORK_PSK;
			break;
		case State::UPDATE_NETWORK_PSK:
			ctrl_cmd(_msg, Cmd("DISABLE_NETWORK ", _accesspoint.id));
			_state = State::DISABLE_NETWORK;
			break;
		case State::DISABLE_NETWORK:
			if (_op != Op::DISABLE_ONLY) {
				ctrl_cmd(_msg, Cmd("ENABLE_NETWORK ", _accesspoint.id));
				_state = State::ENABLE_NETWORK;
			} else
				_state = State::COMPLETE;
			break;
		case State::ENABLE_NETWORK:
			_state  = State::COMPLETE;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		switch (_state) {
		case State::INIT: break;
		case State::UPDATE_NETWORK_PSK: [[fallthrough]];
		case State::ENABLE_NETWORK:     [[fallthrough]];
		case State::DISABLE_NETWORK:
			if (!cmd_successful(msg)) {
				error("UPDATE_NETWORK(", (unsigned)_state, ") failed: ", msg);
				Action::successful = false;
				complete = true;
			}
			break;
		case State::COMPLETE: break;
		}

		if (complete)
			_state = State::COMPLETE;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for initiating a scan request
 */
struct Scan_cmd : Action
{
	enum class State : unsigned {
		INIT, SCAN, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	State            _state;

	/* enough to store 64 hidden networks */
	char ssid_buffer[4060] { };
	size_t buffer_pos { 0 };

	Scan_cmd(Ctrl_msg_buffer &msg)
	:
		Action      { Command::SCAN },
		_msg        { msg },
		_state      { State::INIT }
	{ }

	void print(Output &out) const override
	{
		Genode::print(out, "Scan_cmd[", (unsigned)_state, "]");
	}

	void append_ssid(Accesspoint::Ssid const &ssid)
	{
		enum { SSID_ARG_LEN = 6 + 64, /* " ssid " + "a5a5a5a5..." */ };
		/* silently ignore SSID */
		if (buffer_pos + SSID_ARG_LEN >= sizeof(ssid_buffer))
			return;

		char ssid_hex[64+1] { };
		char const *ssid_ptr = ssid.string();

		for (size_t i = 0; i < ssid.length() - 1; i++) {
			Util::byte2hex((ssid_hex + i * 2), ssid_ptr[i]);
		}

		Genode::String<SSID_ARG_LEN + 1> tmp(" ssid ", (char const*)ssid_hex);
		size_t const tmp_len = tmp.length() - 1;

		Genode::memcpy((ssid_buffer + buffer_pos), tmp.string(), tmp_len);
		buffer_pos += tmp_len;
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("SCAN ", (char const*)ssid_buffer));
			_state = State::SCAN;
			break;
		case State::SCAN:
			_state = State::COMPLETE;
			break;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		switch (_state) {
		case State::INIT: break;
		case State::SCAN:
			if (!cmd_successful(msg)) {
				/* ignore busy fails silently */
				bool const scan_busy = strcmp(msg, "FAIL-BUSY");
				if (!scan_busy) {
					error("could not initiate scan: ", msg);
					Action::successful = false;
					complete = true;
				}
			}
			break;
		case State::COMPLETE: break;
		}

		if (complete)
			_state = State::COMPLETE;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for initiating a scan results request
 */
struct Scan_results_cmd : Action
{
	enum class State : unsigned {
		INIT, SCAN_RESULTS, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	State            _state;

	Expanding_reporter &_reporter;

	void _generate_report(char const *msg)
	{
		unsigned count_lines = 0;
		for_each_line(msg, [&] (char const*) { count_lines++; });

		if (!count_lines)
			return;

		try {
			_reporter.generate([&] (Xml_generator &xml) {

				for_each_result_line(msg, [&] (Accesspoint const &ap) {

					/* ignore potentially empty ssids */
					if (ap.ssid == "")
						return;

					xml.node("accesspoint", [&]() {
						xml.attribute("ssid",    ap.ssid);
						xml.attribute("bssid",   ap.bssid);
						xml.attribute("freq",    ap.freq);
						xml.attribute("quality", ap.quality);
						if (ap.wpa()) { xml.attribute("protection", ap.prot); }
					});
				});
			});

		} catch (...) { /* silently omit report */ }
	}

	Scan_results_cmd(Ctrl_msg_buffer &msg,
	                 Genode::Expanding_reporter &reporter)
	:
		Action      { Command::SCAN_RESULTS },
		_msg        { msg },
		_state      { State::INIT },
		_reporter   { reporter }
	{ }

	void print(Output &out) const override
	{
		Genode::print(out, "Scan_results_cmd[", (unsigned)_state, "]");
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("SCAN_RESULTS"));
			_state = State::SCAN_RESULTS;
			break;
		case State::SCAN_RESULTS:
			_state = State::COMPLETE;
			break;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		switch (_state) {
		case State::INIT: break;
		case State::SCAN_RESULTS:
			if (scan_results(msg))
				_generate_report(msg);
			break;
		case State::COMPLETE: break;
		}

		if (complete)
			_state = State::COMPLETE;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for setting a configuration variable
 */
struct Set_cmd : Action
{
	using Key   = String<64>;
	using Value = String<128>;

	enum class State : unsigned {
		INIT, SET, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	State            _state;

	Key   _key;
	Value _value;

	Set_cmd(Ctrl_msg_buffer &msg, Key key, Value value)
	:
		Action      { Command::SET },
		_msg        { msg },
		_state      { State::INIT },
		_key        { key },
		_value      { value }
	{ }

	void print(Output &out) const override
	{
		Genode::print(out, "Set_cmd[", (unsigned)_state, "] key: '",
		                   _key, "' value: '", _value, "'");
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("SET ", _key, " \"", _value, "\""));
			_state = State::SET;
			break;
		case State::SET:
			_state = State::COMPLETE;
			break;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		switch (_state) {
		case State::INIT: break;
		case State::SET:
			if (!cmd_successful(msg)) {
				error("could not set '", _key, "' to '",
				      _value, "': '", msg, "'");
				Action::successful = false;
				complete = true;
			}
			break;
		case State::COMPLETE: break;
		}

		if (complete)
			_state = State::COMPLETE;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for setting a configuration variable
 */
struct Log_level_cmd : Action
{
	using Level = Genode::String<16>;

	enum class State : unsigned {
		INIT, LOG_LEVEL, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	State            _state;

	Level _level;

	Log_level_cmd(Ctrl_msg_buffer &msg, Level const &level)
	:
		Action      { Command::LOG_LEVEL },
		_msg        { msg },
		_state      { State::INIT },
		_level      { level }
	{ }

	void print(Output &out) const override
	{
		Genode::print(out, "Log_level_cmd[", (unsigned)_state, "] '", _level, "'");
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("LOG_LEVEL ", _level));
			_state = State::LOG_LEVEL;
			break;
		case State::LOG_LEVEL:
			_state = State::COMPLETE;
			break;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		switch (_state) {
		case State::INIT: break;
		case State::LOG_LEVEL:
			if (!cmd_successful(msg)) {
				error("could not set LOG_LEVEL to ", _level);
				Action::successful = false;
				complete = true;
			}
			break;
		case State::COMPLETE: break;
		}

		if (complete)
			_state = State::COMPLETE;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for querying BSS information
 */
struct Bss_query : Action
{
	enum class State : unsigned {
		INIT, BSS, COMPLETE
	};
	Ctrl_msg_buffer    &_msg;
	Accesspoint::Bssid  _bssid;
	State               _state;

	Bss_query(Ctrl_msg_buffer &msg, Accesspoint::Bssid bssid)
	:
		Action      { Query::BSS },
		_msg        { msg },
		_bssid      { bssid },
		_state      { State::INIT }
	{ }

	void print(Output &out) const override
	{
		Genode::print(out, "Bss_query[", (unsigned)_state, "] ", _bssid);
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("BSS ", _bssid));
			_state = State::BSS;
			break;
		case State::BSS:      break;
		case State::COMPLETE: break;
		}
	}

	void response(char const *msg, Accesspoint &ap) override
	{
		if (_state != State::BSS)
			return;

		_state = State::COMPLETE;

		/*
		 * It might happen that the supplicant already flushed
		 * its internal BSS information and cannot help us out.
		 * Since we already sent out a rudimentary report, just
		 * stop here.
		 */
		if (0 == msg[0])
			return;

		auto fill_ap = [&] (char const *line) {
			if (Genode::strcmp(line, "ssid=", 5) == 0) {
				ap.ssid = Accesspoint::Ssid(line+5);
			} else

			if (Genode::strcmp(line, "bssid=", 6) == 0) {
				ap.bssid = Accesspoint::Bssid(line+6);
			} else

			if (Genode::strcmp(line, "freq=", 5) == 0) {
				ap.freq = Accesspoint::Freq(line+5);
			}
		};
		for_each_line(msg, fill_ap);
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for querying RSSI information
 */
struct Rssi_query : Action
{
	enum class State : unsigned {
		INIT, RSSI, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	State            _state;

	Rssi_query(Ctrl_msg_buffer &msg)
	:
		Action      { Query::RSSI },
		_msg        { msg },
		_state      { State::INIT }
	{ }

	void print(Output &out) const override
	{
		Genode::print(out, "Rssi_query[", (unsigned)_state, "]");
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("SIGNAL_POLL"));
			_state = State::RSSI;
			break;
		case State::RSSI:     break;
		case State::COMPLETE: break;
		}
	}

	void response(char const *msg, Accesspoint &ap) override
	{
		if (_state != State::RSSI)
			return;

		_state = State::COMPLETE;

		using Rssi = Genode::String<5>;
		Rssi rssi { };
		auto get_rssi = [&] (char const *line) {
			if (strcmp(line, "RSSI=", 5) != 0)
				return;

			rssi = Rssi(line + 5);
		};
		for_each_line(msg, get_rssi);

		/*
		 * Use the same simplified approximation for denoting
		 * the quality to be in line with the scan results.
		 */
		ap.quality = Util::approximate_quality(rssi.valid() ? rssi.string()
		                                                    : "-100");
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for querying the current connection status
 */
struct Status_query : Action
{
	enum class State : unsigned {
		INIT, STATUS, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	State            _state;

	Status_query(Ctrl_msg_buffer &msg)
	:
		Action      { Query::STATUS },
		_msg        { msg },
		_state      { State::INIT }
	{ }

	void print(Output &out) const override
	{
		Genode::print(out, "Status_query[", (unsigned)_state, "]");
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("STATUS"));
			_state = State::STATUS;
			break;
		case State::STATUS:   break;
		case State::COMPLETE: break;
		}
	}

	void response(char const *msg, Accesspoint &ap) override
	{
		if (_state != State::STATUS)
			return;

		_state = State::COMPLETE;

		/*
		 * It might happen that the supplicant already flushed
		 * its internal BSS information and cannot help us out.
		 * Since we already sent out a rudimentary report, just
		 * stop here.
		 */
		if (0 == msg[0])
			return;

		auto fill_ap = [&] (char const *line) {
			if (strcmp(line, "ssid=", 5) == 0) {
				ap.ssid = Accesspoint::Ssid(line+5);
			} else

			if (strcmp(line, "bssid=", 6) == 0) {
				ap.bssid = Accesspoint::Bssid(line+6);
			} else

			if (strcmp(line, "freq=", 5) == 0) {
				ap.freq = Accesspoint::Freq(line+5);
			}
		};
		for_each_line(msg, fill_ap);
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Wifi driver manager
 */
struct Wifi::Manager : Wifi::Rfkill_notification_handler
{
	Manager(const Manager&) = delete;
	Manager& operator=(const Manager&) = delete;

	/* Network handling */

	Heap                _network_allocator;
	List_model<Network> _network_list { };

	/* Explicit_scan handling */

	Heap                      _explicit_scan_allocator;
	List_model<Explicit_scan> _explicit_scan_list { };

	/*
	 * Action queue handling
	 */

	Heap _actions_alloc;
	Fifo<Action> _actions { };

	Action *_pending_action { nullptr };

	void _queue_action(Action &action, bool verbose)
	{
		_actions.enqueue(action);
		if (verbose)
			Genode::log("Queue ", action);
	}

	enum class Pending_action_result : unsigned {
		INCOMPLETE, COMPLETE };

	void _with_pending_action(auto const &fn)
	{
		if (!_pending_action)
			_actions.dequeue([&] (Action &action) {
				_pending_action = &action; });

		Pending_action_result const result =
			_pending_action ? fn(*_pending_action)
			                : Pending_action_result::INCOMPLETE;
		if (result == Pending_action_result::COMPLETE) {
			destroy(_actions_alloc, _pending_action);
			_pending_action = nullptr;
		}
	}

	void _dispatch_action_if_needed()
	{
		if (_pending_action)
			return;

		/*
		 * Grab the next action and call execute()
		 * to poke the CTRL interface.
		 */

		_actions.dequeue([&] (Action &action) {
			_pending_action = &action;
			_pending_action->execute();
		});
	}

	Signal_handler<Wifi::Manager> _cmd_handler;
	Signal_handler<Wifi::Manager> _events_handler;

	Blockade _notify_blockade { };

	struct Notify : Notify_interface
	{
		Signal_handler<Wifi::Manager> &_response;
		Signal_handler<Wifi::Manager> &_event;
		Blockade                      &_blockade;

		Notify(Signal_handler<Wifi::Manager> &response,
		       Signal_handler<Wifi::Manager> &event,
		       Blockade                      &blockade)
		:
			_response { response },
			_event    { event },
			_blockade { blockade }
		{ }

		/**********************
		 ** Notify_interface **
		 **********************/

		void submit_response() override {
			_response.local_submit(); }

		void submit_event() override {
			_event.local_submit(); }

		void block_for_processing() override {
			_blockade.block(); }
	} _notify { _cmd_handler, _events_handler, _notify_blockade };

	Msg_buffer _msg { _notify };

	void _handle_rfkill()
	{
		_join.rfkilled = Wifi::rfkill_blocked();

		/* re-enable scan timer */
		if (!_join.rfkilled)
			_try_arming_any_timer();
	}

	Signal_handler<Wifi::Manager> _rfkill_handler;

	/*
	 * Configuration handling
	 */

	Attached_rom_dataspace         _config_rom;
	Signal_handler<Wifi::Manager> _config_sigh;

	struct Config
	{
		enum {
			DEFAULT_CONNECTED_SCAN_INTERVAL  = 30,
			DEFAULT_SCAN_INTERVAL            =  5,
			DEFAULT_UPDATE_QUAILITY_INTERVAL = 30,

			DEFAULT_VERBOSE       = false,
			DEFAULT_RFKILL        = false,
		};

		unsigned scan_interval           { DEFAULT_SCAN_INTERVAL };
		unsigned update_quality_interval { DEFAULT_UPDATE_QUAILITY_INTERVAL };

		bool intervals_changed(Config const &cfg) const
		{
			return scan_interval           != cfg.scan_interval
			    || update_quality_interval != cfg.update_quality_interval;
		}

		bool verbose { DEFAULT_VERBOSE };
		bool rfkill  { DEFAULT_RFKILL };

		bool rfkill_changed(Config const &cfg) const {
			return rfkill != cfg.rfkill; }

		/* see wpa_debug.h - EXCESSIVE, MSGDUMP, DEBUG, INFO, WARNING, ERROR */
		using Log_level = Log_level_cmd::Level;
		Log_level log_level { "" };

		bool log_level_changed(Config const &cfg) const {
			return log_level != cfg.log_level; }

		bool log_level_set() const {
			return log_level.length() > 1; }

		using Bgscan = Genode::String<16>;
		Bgscan bgscan { "" };

		bool bgscan_changed(Config const &cfg) const {
			return bgscan != cfg.bgscan; }

		bool bgscan_set() const {
			return bgscan.length() >= 1; }

		static Config from_xml(Xml_node const &node)
		{
			bool const verbose       = node.attribute_value("verbose",
			                                                (bool)DEFAULT_VERBOSE);
			bool const rfkill        = node.attribute_value("rfkill",
			                                                (bool)DEFAULT_RFKILL);
			Log_level log_level =
				node.attribute_value("log_level", Log_level("error"));
			/* always enforce at leaast error level of verbosity */
			if (log_level.length() <= 1)
				log_level = Log_level("error");

			Bgscan const bgscan =
				node.attribute_value("bgscan", Bgscan("simple:30:-70:600"));

			unsigned const scan_interval =
				Util::check_time(node.attribute_value("scan_interval",
				                 (unsigned)DEFAULT_SCAN_INTERVAL),
				                 5, 15*60);

			unsigned const update_quality_interval =
				Util::check_time(node.attribute_value("update_quality_interval",
				                 (unsigned)DEFAULT_UPDATE_QUAILITY_INTERVAL),
				                 10, 15*60);

			Config new_config {
				.scan_interval           = scan_interval,
				.update_quality_interval = update_quality_interval,
				.verbose                 = verbose,
				.rfkill                  = rfkill,
				.log_level               = log_level,
				.bgscan                  = bgscan
			};
			return new_config;
		}
	};

	Config _config { };

	void _config_update(bool initial_config)
	{
		_config_rom.update();

		if (!_config_rom.valid())
			return;

		Xml_node const config_node = _config_rom.xml();

		Config const old_config = _config;

		_config = Config::from_xml(config_node);

		if (_config.intervals_changed(old_config) || initial_config)
			_try_arming_any_timer();

		if (_config.rfkill_changed(old_config) || initial_config) {
			Wifi::set_rfkill(_config.rfkill);

			/*
			 * In case we get blocked set rfkilled immediately to prevent
			 * any further scanning operation. The actual value will be set
			 * by the singal handler but is not expected to be any different
			 * as the rfkill call is not supposed to fail.
			 */
			if (_config.rfkill && !_join.rfkilled)
				_join.rfkilled = true;
		}

		if (_config.log_level_changed(old_config) || initial_config)
			if (_config.log_level_set())
				_queue_action(*new (_actions_alloc)
					Log_level_cmd(_msg, _config.log_level), _config.verbose);

		if (_config.bgscan_changed(old_config) || initial_config)
			if (_config.bgscan_set())
					_queue_action(*new (_actions_alloc)
						Set_cmd(_msg, Set_cmd::Key("bgscan"),
						              Set_cmd::Value(_config.bgscan)),
						_config.verbose);

		_network_list.update_from_xml(config_node,

			[&] (Genode::Xml_node const &node) -> Network & {

				Accesspoint const ap = Accesspoint::from_xml(node);

				/*
				 * Only make the supplicant acquainted with the network
				 * when it is usable, it always needs a valid SSID and
				 * in case it is protected also a valid PSK, but create
				 * the Network object nonetheless to satisfy the List_model
				 * requirements.
				 */

				bool const ssid_valid = Accesspoint::valid(ap.ssid);
				if (!ssid_valid)
					warning("accesspoint has invalid ssid: '", ap.ssid, "'");

				bool const pass_valid = ap.wpa() ? Accesspoint::valid(ap.pass)
				                                 : true;
				if (!pass_valid)
					warning("accesspoint '", ap.ssid, "' has invalid psk");

				if (ssid_valid && pass_valid)
						_queue_action(*new (_actions_alloc)
							Add_network_cmd(_msg, ap), _config.verbose);

				return *new (_network_allocator) Network(ap);
			},
			[&] (Network &network) {

				network.with_accesspoint([&] (Accesspoint &ap) {

					if (!Accesspoint::valid(ap.ssid) || !ap.stored())
						return;

					_queue_action(*new (_actions_alloc)
						Remove_network_cmd(_msg, ap.id), _config.verbose);
				});

				Genode::destroy(_network_allocator, &network);
			},
			[&] (Network &network, Genode::Xml_node const &node) {
				Accesspoint const updated_ap = Accesspoint::from_xml(node);

				network.with_accesspoint([&] (Accesspoint &ap) {

					if (!ap.updated_from(updated_ap))
						return;

					if (!ap.stored())
						return;

					_queue_action(*new (_actions_alloc)
						Update_network_cmd(_msg, ap), _config.verbose);
				});
			});

		_explicit_scan_list.update_from_xml(config_node,

			[&] (Genode::Xml_node const &node) -> Explicit_scan & {
				Accesspoint::Ssid const ssid =
					node.attribute_value("ssid", Accesspoint::Ssid());

				/*
				 * Always created the Explicit_scan object but ignore
				 * invalid ones during SCAN operation to satisfy the
				 * List_model requirements.
				 */
				return *new (_explicit_scan_allocator) Explicit_scan(ssid);
			},
			[&] (Explicit_scan &explicit_scan) {
				Genode::destroy(_explicit_scan_allocator,
				                &explicit_scan);
			},
			[&] (Explicit_scan &explicit_scan, Genode::Xml_node const &node) {
				/*
				 * Intentionally left empty as we never have to update the
				 * object as it only contains the SSID that also serves as
				 * identifier.
				 */
			});

		_dispatch_action_if_needed();
	}

	void _handle_config_update() { _config_update(false); }

	/*
	 * Timeout handling
	 */

	Timer::Connection _timer;

	Timer::One_shot_timeout<Wifi::Manager> _scan_timeout {
		_timer, *this, &Wifi::Manager::_handle_scan_timeout };

	Timer::One_shot_timeout<Wifi::Manager> _quality_timeout {
		_timer, *this, &Wifi::Manager::_handle_quality_timeout };

	enum class Timer_type : uint8_t { SCAN, SIGNAL_POLL };

	bool _arm_timer(Timer_type const type)
	{
		auto seconds_from_type = [&] (Timer_type const type) {
			switch (type) {
			case Timer_type::SCAN:           return _config.scan_interval;
			case Timer_type::SIGNAL_POLL:    return _config.update_quality_interval;
			}
			return 0u;
		};

		Microseconds const us { seconds_from_type(type) * 1000'000u };
		if (!us.value)
			return false;

		if (_config.verbose) {
			auto name_from_type = [&] (Timer_type const type) {
				switch (type) {
				case Timer_type::SCAN:           return "scan";
				case Timer_type::SIGNAL_POLL:    return "signal-poll";
				}
				return "";
			};

			log("Arm timer for ", name_from_type(type), ": ", us);
		}

		switch (type) {
		case Timer_type::SCAN:           _scan_timeout.schedule(us);    break;
		case Timer_type::SIGNAL_POLL:    _quality_timeout.schedule(us); break;
		}
		return true;
	}

	bool _arm_scan_timer()
	{
		if (_join.state == Join_state::State::CONNECTED)
			return false;

		return _arm_timer(Timer_type::SCAN);
	}

	bool _arm_poll_timer()
	{
		if (_join.state != Join_state::State::CONNECTED)
			return false;

		return _arm_timer(Timer_type::SIGNAL_POLL);
	}

	void _try_arming_any_timer()
	{
		_arm_scan_timer();
		_arm_poll_timer();
	}

	void _handle_scan_timeout(Genode::Duration)
	{
		if (_join.rfkilled) {
			if (_config.verbose)
				log("Scanning: suspend due to RFKILL");
			return;
		}

		if (!_arm_scan_timer()) {
			if (_config.verbose)
				log("Timer: scanning disabled");
			return;
		}

		Scan_cmd &scan_cmd = *new (_actions_alloc) Scan_cmd(_msg);
		_explicit_scan_list.for_each([&] (Explicit_scan const &explicit_scan) {
			explicit_scan.with_ssid([&] (Accesspoint::Ssid const &ssid) {
				scan_cmd.append_ssid(ssid);
			});
		});
		_queue_action(scan_cmd, _config.verbose);

		_dispatch_action_if_needed();
	}

	void _handle_quality_timeout(Genode::Duration)
	{
		if (_join.rfkilled) {
			if (_config.verbose)
				log("Quality polling: suspend due to RFKIL");
			return;
		}

		if (!_arm_poll_timer()) {
			if (_config.verbose)
				log("Timer: signal-strength polling disabled");
			return;
		}

		_queue_action(*new (_actions_alloc) Rssi_query(_msg), _config.verbose);

		_dispatch_action_if_needed();
	}

	/*
	 * CTRL interface event handling
	 */

	Constructible<Expanding_reporter> _state_reporter { };
	Constructible<Expanding_reporter> _ap_reporter    { };

	enum class Bssid_offset : unsigned {
		/* by the power of wc -c, I have the start pos... */
		CONNECT = 37, CONNECTING = 33, DISCONNECT = 30, };

	Accesspoint::Bssid const _extract_bssid(char const *msg, Bssid_offset offset)
	{
		char bssid[32] = { };

		size_t const len   = 17;
		size_t const start = (size_t)offset;

		memcpy(bssid, msg + start, len);
		return Accesspoint::Bssid((char const*)bssid);
	}

	Accesspoint::Ssid const _extract_ssid(char const *msg)
	{
		char ssid[64] = { };
		size_t const start = 58;

		/* XXX assume "SME:.*SSID='xx xx' ...)", so look for the
		 *     closing ' but we _really_ should use something like
		 *     printf_encode/printf_deccode functions
		 *     (see wpa_supplicant/src/utils/common.c) and
		 *     remove our patch…
		 */
		size_t const len = Util::next_char(msg, start, 0x27);
		if (!len || len >= 33)
			return Accesspoint::Ssid();

		memcpy(ssid, msg + start, len);

		return Accesspoint::Ssid((char const *)ssid);
	}

	enum class Auth_result : unsigned {
		OK, FAILED, INVALIDED };

	Auth_result _auth_result(char const *msg)
	{
		enum { REASON_OFFSET = 55, };
		unsigned reason = 0;
		ascii_to((msg + REASON_OFFSET), reason);
		switch (reason) {
		case  2: /* prev auth no longer valid */
			return Auth_result::INVALIDED;
		case 15: /* 4-way handshake timeout/failed */
			return Auth_result::FAILED;
		default:
			return Auth_result::OK;
		}
	}

	struct Join_state
	{
		enum class State : unsigned {
			DISCONNECTED, CONNECTING, CONNECTED };

		Accesspoint ap { };

		State state { DISCONNECTED };

		bool auth_failure { false };
		bool not_found    { false };
		bool rfkilled     { false };

		enum { MAX_REAUTH_ATTEMPTS = 3 };
		unsigned reauth_attempts { 0 };

		enum { MAX_NOT_FOUND_IGNORE_ATTEMPTS = 3 };
		unsigned ignore_not_found { 0 };

		void print(Output &out) const
		{
			auto state_string = [&] (State const state) {
				switch (state) {
				case State::DISCONNECTED: return "disconnected";
				case State::CONNECTED:    return "connected";
				case State::CONNECTING:   return "connecting";
				}
				return "";
			};
			Genode::print(out, state_string(state), " "
			                   "ssid: '",  ap.ssid, "' "
			                   "bssid: ", ap.bssid, " "
			                   "freq: ",  ap.freq, " "
			                   "quality: ",  ap.quality, " "
			                   "auth_failure: ", auth_failure, " "
			                   "reauth_attempts: ", reauth_attempts, " "
			                   "not_found: ", not_found, " "
			                   "ignore_not_found: ", ignore_not_found, " "
			                   "rfkilled: ", rfkilled);
		}

		void generate_state_report_if_needed(Expanding_reporter &reporter,
		                                     Join_state const &old)
		{
			/*
			 * Explicitly check for the all changes provoked by
			 * actions or events.
			 */
			if (state      == old.state
			 && ap.quality == old.ap.quality
			 && ap.ssid    == old.ap.ssid
			 && ap.bssid   == old.ap.bssid
			 && ap.freq    == old.ap.freq)
				return;

			reporter.generate([&] (Xml_generator &xml) {
				xml.node("accesspoint", [&] () {
					xml.attribute("ssid",  ap.ssid);
					xml.attribute("bssid", ap.bssid);
					xml.attribute("freq",  ap.freq);

					if (state == Join_state::State::CONNECTED)
						xml.attribute("state", "connected");
					else

					if (state == Join_state::State::DISCONNECTED) {
						xml.attribute("state", "disconnected");
						xml.attribute("rfkilled",     rfkilled);
						xml.attribute("auth_failure", auth_failure);
						xml.attribute("not_found",    not_found);
					} else

					if (state == Join_state::State::CONNECTING)
						xml.attribute("state", "connecting");

					/*
					 * Only add the attribute when we have something
					 * to report so that a consumer of the state report
					 * may take appropriate actions.
					 */
					if (ap.quality)
						xml.attribute("quality", ap.quality);
				});
			});
		}
	};

	Join_state _join { };

	bool _single_network() const
	{
		unsigned count = 0;
		_network_list.for_each([&] (Network const &network) {
			network.with_accesspoint([&] (Accesspoint const &ap) {
				++count; }); });
		return count == 1;
	}

	void _handle_events()
	{
		Join_state const old_join = _join;

		_msg.with_new_event([&] (char const *msg) {

			/*
			 * CTRL-EVENT-SCAN-RESULTS
			 */
			if (results_available(msg)) {

				/*
				 * We might have to pull the socketcall task out of poll_all()
				 * because otherwise we might be late and wpa_supplicant has
				 * already removed all scan results due to BSS age settings.
				 */
				wifi_kick_socketcall();

				_queue_action(*new (_actions_alloc)
					Scan_results_cmd(_msg, *_ap_reporter), _config.verbose);
			} else

			/*
			 * SME: Trying to authenticate with ...
			 */
			if (connecting_to_network(msg)) {

				_join.state = Join_state::State::CONNECTING;
				_join.ap    = Accesspoint(_extract_bssid(msg, Bssid_offset::CONNECTING),
				                          _extract_ssid(msg));
				_join.auth_failure = false;
				_join.not_found    = false;
			} else

			/*
			 * CTRL-EVENT-NETWORK-NOT-FOUND
			 */
			if (network_not_found(msg)) {

				/*
				 * In case there is only one auto-connect network configured
				 * generate a disconnect event so that a management component
				 * can deal with that situation. However, we do not disable the
				 * network to allow for automatically rejoining a reapparing
				 * network that was previously not found.
				 *
				 * This may happen when an accesspoint is power-cycled or when
				 * there is a key management mismatch due to operator error.
				 * Unfortunately we cannot easily distinguish a wrongly prepared
				 * <wifi_config> where the 'protection' attribute does not match
				 * as we do not have the available accesspoints at hand to compare
				 * that.
				 */
				if ((_join.state == Join_state::State::CONNECTING) && _single_network()) {

					/*
					 * Ignore the event for a while as it may happen that hidden
					 * networks may take some time.
					 */
					if (++_join.ignore_not_found >= Join_state::MAX_NOT_FOUND_IGNORE_ATTEMPTS) {
						_join.ignore_not_found = 0;

						_network_list.for_each([&] (Network &network) {
							network.with_accesspoint([&] (Accesspoint &ap) {

								if (ap.ssid != _join.ap.ssid)
									return;

								_join.state     = Join_state::State::DISCONNECTED;
								_join.ap        = Accesspoint();
								_join.not_found = true;
							});
						});
					}
				}
			} else

			/*
			 * CTRL-EVENT-DISCONNECTED ... reason=...
			 */
			if (disconnected_from_network(msg)) {

				Join_state::State const old_state = _join.state;

				Auth_result const auth_result = _auth_result(msg);

				_join.auth_failure = auth_result != Auth_result::OK;
				_join.state        = Join_state::State::DISCONNECTED;
				_join.not_found    = false;

				Accesspoint::Bssid const bssid =
					_extract_bssid(msg, Bssid_offset::DISCONNECT);

				if (bssid != _join.ap.bssid)
					warning(bssid, " does not match stored ", _join.ap.bssid);

				/*
				 * Use a simplistic heuristic to ignore re-authentication requests
				 * and hope for the supplicant to do its magic.
				 */
				if ((old_state == Join_state::State::CONNECTED) && _join.auth_failure)
					if (++_join.reauth_attempts <= Join_state::MAX_REAUTH_ATTEMPTS) {
						log("ignore deauth from: ", bssid);
						return;
					}
				_join.reauth_attempts = 0;

				_network_list.for_each([&] (Network &network) {
					network.with_accesspoint([&] (Accesspoint &ap) {

						if (ap.ssid != _join.ap.ssid)
							return;

						if (!_join.auth_failure)
							return;

						/*
						 * Prevent the supplicant from trying to join the network
						 * again. At this point intervention by the management
						 * component is needed.
						 */

						_queue_action(*new (_actions_alloc)
							Update_network_cmd(_msg, ap,
							                   Update_network_cmd::Op::DISABLE_ONLY),
							_config.verbose);
					});
				});
			} else

			/*
			 * CTRL-EVENT-CONNECTED - Connection to ...
			 */
			if (connected_to_network(msg)) {

				_join.state           = Join_state::State::CONNECTED;
				_join.ap.bssid        = _extract_bssid(msg, Bssid_offset::CONNECT);
				_join.auth_failure    = false;
				_join.not_found       = false;
				_join.reauth_attempts = 0;

				/* collect further information like frequency and so on  */
				_queue_action(*new (_actions_alloc) Status_query(_msg),
					_config.verbose);

				_arm_poll_timer();
			}
		});

		_notify_blockade.wakeup();

		_join.generate_state_report_if_needed(*_state_reporter, old_join);

		_dispatch_action_if_needed();
	}

	/*
	 * CTRL interface command handling
	 */

	void _handle_cmds()
	{
		Join_state const old_join = _join;

		_msg.with_new_reply([&] (char const *msg) {

			_with_pending_action([&] (Action &action) {

				/*
				 * Check response first as we ended up here due
				 * to an already submitted cmd.
				 */
				switch (action.type) {

				case Action::Type::COMMAND:
					action.check(msg);
					break;

				case Action::Type::QUERY:
					action.response(msg, _join.ap);
					break;
				}

				/*
				 * We always switch to the next state after checking and
				 * handling the response from the CTRL interface.
				 */
				action.execute();

				if (!action.complete())
					return Pending_action_result::INCOMPLETE;

				switch (action.command) {
				case Action::Command::ADD:
				{
					Add_network_cmd const &add_cmd =
						*dynamic_cast<Add_network_cmd*>(&action);

					bool handled = false;
					Accesspoint const &added_ap = add_cmd.accesspoint();
					_network_list.for_each([&] (Network &network) {
						network.with_accesspoint([&] (Accesspoint &ap) {
							if (ap.ssid != added_ap.ssid)
								return;

							if (ap.stored()) {
								error("accesspoint for SSID '", ap.ssid, "' "
								       "already stored ", ap.id);
								return;
							}

							ap.id = added_ap.id;
							handled = true;
						});
					});

					/*
					 * We have to guard against having the accesspoint removed via a config
					 * update while we are still adding it to the supplicant by removing the
					 *
					 * network directly afterwards.
					 */
					if (!handled) {
						_queue_action(*new (_actions_alloc)
							Remove_network_cmd(_msg, added_ap.id), _config.verbose);
					} else

					if (handled && _single_network())
						/*
						 * To accomodate a management component that only deals
						 * with one network, e.g. the sculpt_manager, generate a
						 * fake connecting event. Either a connected or disconnected
						 * event will bring us to square one.
						 */
						if ((_join.state != Join_state::State::CONNECTED) && !_join.rfkilled) {
							_network_list.for_each([&] (Network const &network) {
								network.with_accesspoint([&] (Accesspoint const &ap) {

									_join.ap    = ap;
									_join.state = Join_state::State::CONNECTING;
								});
							});
						}

					break;
				}
				default: /* ignore the rest */
					break;
				}

				return Pending_action_result::COMPLETE;
			});
		});

		_notify_blockade.wakeup();

		_join.generate_state_report_if_needed(*_state_reporter, old_join);

		_dispatch_action_if_needed();
	}

	/**
	 * Constructor
	 */
	Manager(Env &env)
	:
		_network_allocator(env.ram(), env.rm()),
		_explicit_scan_allocator(env.ram(), env.rm()),
		_actions_alloc(env.ram(), env.rm()),
		_cmd_handler(env.ep(),    *this, &Wifi::Manager::_handle_cmds),
		_events_handler(env.ep(), *this, &Wifi::Manager::_handle_events),
		_rfkill_handler(env.ep(), *this, &Wifi::Manager::_handle_rfkill),
		_config_rom(env, "wifi_config"),
		_config_sigh(env.ep(), *this, &Wifi::Manager::_handle_config_update),
		_timer(env)
	{
		_config_rom.sigh(_config_sigh);

		/* set/initialize as unblocked */
		_notify_blockade.wakeup();

		/*
		 * Both Report sessions are mandatory, let the driver fail in
		 * case they cannot be created.
		 */
		{
			_ap_reporter.construct(env, "accesspoints", "accesspoints");
			_ap_reporter->generate([&] (Genode::Xml_generator &) { });
		}

		{
			_state_reporter.construct(env, "state", "state");
			_state_reporter->generate([&] (Genode::Xml_generator &xml) {
				xml.node("accesspoint", [&] () {
					xml.attribute("state", "disconnected");
				});
			});
		}

		/* read in list of APs */
		_config_update(true);

		/* get initial RFKILL state */
		_handle_rfkill();

		/* kick-off initial scanning */
		_handle_scan_timeout(Duration(Microseconds(0)));
	}

	/**
	 * Trigger RFKILL notification
	 *
	 * Used by the wifi driver to notify the manager.
	 */
	void rfkill_notify() override {
		_rfkill_handler.local_submit(); }

	/**
	 * Return message buffer
	 *
	 * Used for communication with the CTRL interface
	 */
	Msg_buffer &msg_buffer() { return _msg; }
};

#endif /* _WIFI_MANAGER_H_ */
