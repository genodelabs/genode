 /*
 * \author Josef Soentgen
 * \date   2018-07-31
 *
 * This wifi driver front end uses the CTRL interface of the wpa_supplicant via
 * a Genode specific backend, which merely wraps a shared memory buffer, to
 * manage the supplicant.
 *
 * Depending on the 'wifi_config' ROM content it will instruct the supplicant
 * to enable, disable and connect to wireless networks. Commands and their
 * corresponding execute result are handled by the '_cmd_handler' dispatcher.
 * This handler drives the front end's state-machine. Any different type of
 * action can only be initiated from the 'IDLE' state. Unsolicited events, e.g.
 * a scan-results-available event, may influence the current state. Config
 * updates are deferred in case the current state is not 'IDLE'.
 *
 * brain-dump
 * ==========
 *
 * config update overview:
 *   [[block any new update]] > [mark stale] > [rm stale] > [add new] > [update new] > [[unblock update]]
 *
 * add new network:
 *   [[new ap]] > [ssid] > bssid? + [bssid] > [psk] > auto? + [enable] > new ap? + [[new ap]]
 *
 * update network:
 *  [[update ap] > bssid? + [bssid] > psk? + [psk] > auto? + [enable] > update ap? + [[update ap]]
 *
 * remove network:
 *  [[mark stale]] > [remove network] > stale? + [remove network]
 */

/*
 * Copyright (C) 2018-2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _WIFI_FRONTEND_H_
#define _WIFI_FRONTEND_H_

/* Genode includes */
#include <libc/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/registry.h>
#include <base/sleep.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <util/interface.h>
#include <util/xml_node.h>

/* rep includes */
#include <wifi/ctrl.h>
#include <wifi/rfkill.h>

/* local includes */
#include <util.h>

/* declare manually as it is a internal hack^Winterface */
extern void wifi_kick_socketcall();


namespace Wifi {
	struct Frontend;
}


/* keep ordered! */
static struct Recv_msg_table {
	char const *string;
	size_t len;
} recv_table[] = {
	{ "OK",                            2 },
	{ "FAIL",                          4 },
	{ "CTRL-EVENT-SCAN-RESULTS",      23 },
	{ "CTRL-EVENT-CONNECTED",         20 },
	{ "CTRL-EVENT-DISCONNECTED",      23 },
	{ "SME: Trying to authenticate",  27 },
	{ "CTRL-EVENT-NETWORK-NOT-FOUND", 28 },
};

enum Rmi {
	OK = 0,
	FAIL,
	SCAN_RESULTS,
	CONNECTED,
	DISCONNECTED,
	SME_AUTH,
	NOT_FOUND,
};


static inline bool check_recv_msg(char const *msg,
                                  Recv_msg_table const &entry) {
	return Genode::strcmp(entry.string, msg, entry.len) == 0; }


static bool cmd_successful(char const *msg) {
	return check_recv_msg(msg, recv_table[OK]); }


static bool cmd_fail(char const *msg) {
	return check_recv_msg(msg, recv_table[FAIL]); }


static bool results_available(char const *msg) {
	return check_recv_msg(msg, recv_table[SCAN_RESULTS]); }


static bool connecting_to_network(char const *msg) {
	return check_recv_msg(msg, recv_table[SME_AUTH]); }


static bool network_not_found(char const *msg) {
	return check_recv_msg(msg, recv_table[NOT_FOUND]); }


static bool scan_results(char const *msg) {
	return Genode::strcmp("bssid", msg, 5) == 0; }


static bool list_network_results(char const *msg) {
	return Genode::strcmp("network", msg, 7) == 0; }


/*
 * Central network data structure
 */
struct Accesspoint : Genode::Interface
{
	using Bssid = Genode::String<17+1>;
	using Freq  = Genode::String< 4+1>;
	using Prot  = Genode::String< 7+1>;
	using Ssid  = Genode::String<32+1>;
	using Pass  = Genode::String<63+1>;

	/*
	 * Accesspoint information fields used by the front end
	 */
	Bssid    bssid  { };
	Freq     freq   { };
	Prot     prot   { };
	Ssid     ssid   { };
	Pass     pass   { };
	unsigned signal { 0 };

	/*
	 * CTRL interface fields
	 *
	 * The 'enabled' field is set to true if ENABLE_NETWORK
	 * was successfully executed. The network itself might
	 * get disabled by wpa_supplicant itself in case it cannot
	 * connect to the network, which will _not_ be reflected
	 * here.
	 */
	int  id      { -1 };
	bool enabled { false };

	/*
	 * Internal configuration fields
	 */
	bool auto_connect  { false };
	bool update        { false };
	bool stale         { false };
	bool explicit_scan { false };

	/**
	 * Default constructor
	 */
	Accesspoint() { }

	/**
	 * Constructor that initializes information fields
	 */
	Accesspoint(char const *bssid, char const *freq,
	            char const *prot,  char const *ssid, unsigned signal)
	: bssid(bssid), freq(freq), prot(prot), ssid(ssid), signal(signal)
	{ }

	void invalidate() { ssid = Ssid(); bssid = Bssid(); }

	bool valid()       const { return ssid.length() > 1; }
	bool bssid_valid() const { return bssid.length() > 1; }
	bool wpa()         const { return prot != "NONE"; }
	bool wpa3()        const { return prot == "WPA3"; }
	bool stored()      const { return id != -1; }
};


template <typename FUNC>
static void for_each_line(char const *msg, FUNC const &func)
{
	char line_buffer[1024];
	size_t cur = 0;

	while (msg[cur] != 0) {
		size_t until = Util::next_char(msg, cur, '\n');
		Genode::memcpy(line_buffer, &msg[cur], until);
		line_buffer[until] = 0;
		cur += until + 1;

		func(line_buffer);
	}
}


template <typename FUNC>
static void for_each_result_line(char const *msg, FUNC const &func)
{
	char line_buffer[1024];
	size_t cur = 0;

	/* skip headline */
	size_t until = Util::next_char(msg, cur, '\n');
	cur += until + 1;

	while (msg[cur] != 0) {
		until = Util::next_char(msg, cur, '\n');
		Genode::memcpy(line_buffer, &msg[cur], until);
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

		unsigned signal = Util::approximate_quality(s[2]);

		char const *prot = is_wpa1 ? "WPA" : "NONE";
		            prot = is_wpa2 ? "WPA2" : prot;
		            prot = is_wpa3 ? "WPA3" : prot;

		Accesspoint ap(s[0], s[1], prot, s[4], signal);

		func(ap);
	}
}


/*
 * Wifi driver front end
 */
struct Wifi::Frontend : Wifi::Rfkill_notification_handler
{
	Frontend(const Frontend&) = delete;
	Frontend& operator=(const Frontend&) = delete;

	/* accesspoint */

	Genode::Heap _ap_allocator;

	using Accesspoint_r = Genode::Registered<Accesspoint>;

	Genode::Registry<Accesspoint_r> _aps { };

	Accesspoint *_lookup_ap_by_ssid(Accesspoint::Ssid const &ssid)
	{
		Accesspoint *p = nullptr;
		_aps.for_each([&] (Accesspoint &ap) {
			if (ap.valid() && ap.ssid == ssid) { p = &ap; }
		});
		return p;
	}

	Accesspoint *_lookup_ap_by_bssid(Accesspoint::Bssid const &bssid)
	{
		Accesspoint *p = nullptr;
		_aps.for_each([&] (Accesspoint &ap) {
			if (ap.valid() && ap.bssid == bssid) { p = &ap; }
		});
		return p;
	}

	Accesspoint *_alloc_ap()
	{
		return new (&_ap_allocator) Accesspoint_r(_aps);
	}

	void _free_ap(Accesspoint &ap)
	{
		Genode::destroy(&_ap_allocator, &ap);
	}

	template <typename FUNC>
	void _for_each_ap(FUNC const &func)
	{
		_aps.for_each([&] (Accesspoint &ap) {
			func(ap);
		});
	}

	unsigned _count_to_be_enabled()
	{
		unsigned count = 0;
		auto enable = [&](Accesspoint const &ap) {
			count += ap.auto_connect;
		};
		_for_each_ap(enable);
		return count;
	}

	unsigned _count_enabled()
	{
		unsigned count = 0;
		auto enabled = [&](Accesspoint const &ap) {
			count += ap.enabled;
		};
		_for_each_ap(enabled);
		return count;
	}

	unsigned _count_stored()
	{
		unsigned count = 0;
		auto enabled = [&](Accesspoint const &ap) {
			count += ap.stored();
		};
		_for_each_ap(enabled);
		return count;
	}

	/* remaining stuff */

	Msg_buffer &_msg;

	Genode::Blockade _notify_blockade { };

	void _notify_lock_lock()   { _notify_blockade.block(); }
	void _notify_lock_unlock() { _notify_blockade.wakeup(); }

	bool _rfkilled { false };

	Genode::Signal_handler<Wifi::Frontend> _rfkill_handler;

	void _handle_rfkill()
	{
		_rfkilled = Wifi::rfkill_blocked();

		/* re-enable scan timer */
		if (!_rfkilled) {
			_scan_timer.sigh(_scan_timer_sigh);
			_arm_scan_timer(false);
		} else {
			_scan_timer.sigh(Genode::Signal_context_capability());
		}

		if (_rfkilled && _state != State::IDLE) {
			Genode::warning("rfkilled in state ", state_strings(_state));
		}
	}

	/* config */

	Genode::Attached_rom_dataspace         _config_rom;
	Genode::Signal_handler<Wifi::Frontend> _config_sigh;

	bool _verbose       { false };
	bool _verbose_state { false };

	bool _deferred_config_update { false };
	bool _single_autoconnect     { false };

	Genode::uint64_t _connected_scan_interval { 30 };
	Genode::uint64_t _scan_interval           {  5 };

	void _config_update(bool signal)
	{
		_config_rom.update();

		if (!_config_rom.valid()) { return; }

		Genode::Xml_node config = _config_rom.xml();

		_verbose       = config.attribute_value("verbose",       _verbose);
		_verbose_state = config.attribute_value("verbose_state", _verbose_state);

		Genode::uint64_t const connected_scan_interval =
			Util::check_time(config.attribute_value("connected_scan_interval",
			                                        _connected_scan_interval),
			                 0, 15*60);

		Genode::uint64_t const scan_interval =
			Util::check_time(config.attribute_value("scan_interval",
			                                        _scan_interval),
			                 5, 15*60);

		bool const new_connected_scan_interval =
			connected_scan_interval != _connected_scan_interval;

		bool const new_scan_interval =
			connected_scan_interval != _scan_interval;

		_connected_scan_interval = connected_scan_interval;
		_scan_interval           = scan_interval;

		/*
		 * Arm again if intervals changed, implicitly discards
		 * an already scheduled timer.
		 */
		if (new_connected_scan_interval || new_scan_interval)
			_arm_scan_timer(_connected_ap.bssid_valid());

		/*
		 * Always handle rfkill, regardless in which state we are currently in.
		 * When we come back from rfkill, will most certainly will be IDLE anyway.
		 */
		if (config.has_attribute("rfkill")) {
			bool const blocked = config.attribute_value("rfkill", false);
			Wifi::set_rfkill(blocked);

			/*
			 * In case we get blocked set rfkilled immediately to prevent
			 * any further scanning operation. The actual value will be set
			 * by the singal handler but is not expected to be any different
			 * as the rfkill call is not supposed to fail.
			 */
			if (blocked && !_rfkilled) {
				_rfkilled = true;

				Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
					xml.node("accesspoint", [&] () {
						xml.attribute("state", "disconnected");
						xml.attribute("rfkilled", _rfkilled);
					});
				});

				_connected_ap.invalidate();
			}
		}

		/*
		 * Block any further config updates until we have finished applying
		 * the current one.
		 */
		if (_state != State::IDLE) {
			Genode::warning("deferring config update (", state_strings(_state), ")");
			_deferred_config_update = true;
			return;
		}

		bool single_autoconnect = false;

		/* update AP list */
		auto parse = [&] ( Genode::Xml_node node) {

			Accesspoint ap;
			ap.ssid  = node.attribute_value("ssid",  Accesspoint::Ssid());
			ap.bssid = node.attribute_value("bssid", Accesspoint::Bssid());

			size_t const ssid_len = ap.ssid.length() - 1;
			if (ssid_len == 0 || ssid_len > 32) {
				Genode::warning("ignoring accesspoint with invalid ssid");
				return;
			}

			Accesspoint *p = _lookup_ap_by_ssid(ap.ssid);
			if (p) {
				if (_verbose) { Genode::log("Update: '", p->ssid, "'"); }
				/* mark for updating */
				p->update = true;
			} else {
				p = _alloc_ap();
				if (!p) {
					Genode::warning("could not add accesspoint, no slots left");
					return;
				}
			}

			ap.pass          = node.attribute_value("passphrase", Accesspoint::Pass(""));
			ap.prot          = node.attribute_value("protection", Accesspoint::Prot("NONE"));
			ap.auto_connect  = node.attribute_value("auto_connect", true);
			ap.explicit_scan = node.attribute_value("explicit_scan", false);

			if (ap.wpa()) {
				size_t const psk_len = ap.pass.length() - 1;
				if (psk_len < 8 || psk_len > 63) {
					Genode::warning("ignoring accesspoint '", ap.ssid,
					                "' with invalid pass");
					return;
				}
			}


			/* check if updating is really necessary */
			if (p->update) {
				p->update = ((ap.bssid.length() > 1 && ap.bssid != p->bssid)
				          || ap.pass  != p->pass
				          || ap.prot  != p->prot
				          || ap.auto_connect != p->auto_connect);
			}

			/* TODO add better way to check validity */
			if (ap.bssid.length() == 17 + 1) { p->bssid = ap.bssid; }

			p->ssid          = ap.ssid;
			p->prot          = ap.prot;
			p->pass          = ap.pass;
			p->auto_connect  = ap.auto_connect;
			p->explicit_scan = ap.explicit_scan;

			single_autoconnect |= (p->update || p->auto_connect) && !_connected_ap.valid();
		};
		config.for_each_sub_node("network", parse);

		/*
		 * To accomodate a management component that only deals
		 * with on network, e.g. the sculpt_manager, generate a
		 * fake connecting event. Either a connected or disconnected
		 * event will bring us to square one.
		 */
		if (signal && _count_to_be_enabled() == 1 && single_autoconnect && !_rfkilled) {

			auto lookup = [&] (Accesspoint const &ap) {
				if (!ap.auto_connect) { return; }

				if (_verbose) { Genode::log("Single autoconnect event for '", ap.ssid, "'"); }

				try {
					Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
						xml.node("accesspoint", [&] () {
							xml.attribute("ssid",  ap.ssid);
							xml.attribute("state", "connecting");
						});
					});

					_single_autoconnect = true;

				} catch (...) { }
			};
			_for_each_ap(lookup);
		}

		/*
		 * Marking removes stale APs first and triggers adding of
		 * new ones afterwards.
		 */
		_mark_stale_aps(config);
	}

	void _handle_config_update() { _config_update(true); }

	/* state */

	Accesspoint *_processed_ap { nullptr };
	Accesspoint _connected_ap { };

	enum State {
		IDLE    = 0x00,
		SCAN    = 0x01,
		NETWORK = 0x02,
		CONNECT = 0x03,
		STATUS  = 0x04,
		INFO    = 0x05,

		INITIATE_SCAN   = 0x00|SCAN,
		PENDING_RESULTS = 0x10|SCAN,

		ADD_NETWORK           = 0x00|NETWORK,
		FILL_NETWORK_SSID     = 0x10|NETWORK,
		FILL_NETWORK_BSSID    = 0x20|NETWORK,
		FILL_NETWORK_KEY_MGMT = 0x30|NETWORK,
		FILL_NETWORK_PSK      = 0x40|NETWORK,
		REMOVE_NETWORK        = 0x50|NETWORK,
		ENABLE_NETWORK        = 0x60|NETWORK,
		DISABLE_NETWORK       = 0x70|NETWORK,
		LIST_NETWORKS         = 0x90|NETWORK,
		SET_NETWORK_PMF       = 0xA0|NETWORK,

		CONNECTING   = 0x00|CONNECT,
		CONNECTED    = 0x10|CONNECT,
		DISCONNECTED = 0x20|CONNECT,
	};

	State _state { State::IDLE };

	char const *state_strings(State state)
	{
		switch (state) {
		case IDLE:               return "idle";
		case INITIATE_SCAN:      return "initiate scan";
		case PENDING_RESULTS:    return "pending results";
		case ADD_NETWORK:        return "add network";
		case FILL_NETWORK_SSID:  return "fill network ssid";
		case FILL_NETWORK_BSSID: return "fill network bssid";
		case FILL_NETWORK_KEY_MGMT: return "fill network key_mgmt";
		case FILL_NETWORK_PSK:   return "fill network pass";
		case REMOVE_NETWORK:     return "remove network";
		case ENABLE_NETWORK:     return "enable network";
		case DISABLE_NETWORK:    return "disable network";
		case CONNECTING:         return "connecting";
		case CONNECTED:          return "connected";
		case DISCONNECTED:       return "disconnected";
		case STATUS:             return "status";
		case LIST_NETWORKS:      return "list networks";
		case INFO:               return "info";
		case SET_NETWORK_PMF:    return "set network pmf";
		default:                 return "unknown";
		};
	}

	void _state_transition(State &current, State next)
	{
		if (_verbose_state) {
			using namespace Genode;
			log("Transition: ", state_strings(current), " -> ",
			    state_strings(next));
		}

		current = next;
	}

	using Cmd_str = Genode::String<sizeof(Msg_buffer::send)>;

	void _submit_cmd(Cmd_str const &str)
	{
		Genode::memset(_msg.send, 0, sizeof(_msg.send));
		Genode::memcpy(_msg.send, str.string(), str.length());
		++_msg.send_id;

		wpa_ctrl_set_fd();

		/*
		 * We might have to pull the socketcall task out of poll_all()
		 * because otherwise we might be late and wpa_supplicant has
		 * already removed all scan results due to BSS age settings.
		 */
		wifi_kick_socketcall();
	}

	/* scan */

	Timer::Connection                      _scan_timer;
	Genode::Signal_handler<Wifi::Frontend> _scan_timer_sigh;

	void _handle_scan_timer()
	{
		/*
		 * If we are blocked or currently trying to join a network
		 * suspend scanning.
		 */
		if (_rfkilled || _connecting.length() > 1) {
			if (_verbose) { Genode::log("Suspend scan timer"); }
			return;
		}

		/* scanning was disabled, ignore current request */
		if (!_arm_scan_timer(_connected_ap.bssid_valid())) {
			if (_verbose) { Genode::log("Scanning disabled, ignore current scan request"); }
			return;
		}

		/* skip as we will be scheduled some time soon(tm) anyway */
		if (_state != State::IDLE) {
			if (_verbose) {
				Genode::log("Not idle, ignore scan request, state: ",
				            Genode::Hex((unsigned)_state));
			}
			return;
		}

		/* left one attempt out */
		if (_scan_busy) {
			if (_verbose) { Genode::log("Scan already pending, ignore scan request"); }
			_scan_busy = false;
			return;
		}

		enum { SSID_ARG_LEN = 6 + 64, /* " ssid " + "a5a5a5a5..." */ };
		/* send buffer - 'SCAN ' + stuff */
		char ssid_buffer[sizeof(Msg_buffer::send)-16] = { };
		size_t buffer_pos = 0;

		auto valid_ssid = [&] (Accesspoint const &ap) {

			if (buffer_pos + SSID_ARG_LEN >= sizeof(ssid_buffer)) {
				return;
			}

			if (!ap.explicit_scan) { return; }

			char ssid_hex[64+1] = { };
			char const *ssid = ap.ssid.string();

			for (size_t i = 0; i < ap.ssid.length() - 1; i++) {
				Util::byte2hex((ssid_hex + i * 2), ssid[i]);
			}

			Genode::String<SSID_ARG_LEN + 1> tmp(" ssid ", (char const*)ssid_hex);
			size_t const tmp_len = tmp.length() - 1;

			Genode::memcpy((ssid_buffer + buffer_pos), tmp.string(), tmp_len);
			buffer_pos += tmp_len;
		};
		_for_each_ap(valid_ssid);

		_state_transition(_state, State::INITIATE_SCAN);
		_submit_cmd(Cmd_str("SCAN", (char const*)ssid_buffer));
	}

	bool _arm_scan_timer(bool connected)
	{
		Genode::uint64_t const sec = connected ? _connected_scan_interval : _scan_interval;
		if (!sec) { return false; }

		if (_verbose) {
			Genode::log("Arm ", connected ? "connected " : "",
			            "scan: ", sec, " sec");
		}

		_scan_timer.trigger_once(sec * (1000 * 1000));
		return true;
	}

	Genode::Constructible<Genode::Expanding_reporter> _ap_reporter { };

	void _generate_scan_results_report(char const *msg)
	{
		unsigned count_lines = 0;
		for_each_line(msg, [&] (char const*) { count_lines++; });

		if (!count_lines) {
			if (_verbose) { Genode::log("Scan results empty"); }
			return;
		}

		bool connecting_attempt = false;
		try {

			_ap_reporter->generate([&] (Genode::Xml_generator &xml) {

				for_each_result_line(msg, [&] (Accesspoint const &ap) {

					/* ignore potentially empty ssids */
					if (ap.ssid == "") { return; }

					xml.node("accesspoint", [&]() {
						xml.attribute("ssid",    ap.ssid);
						xml.attribute("bssid",   ap.bssid);
						xml.attribute("freq",    ap.freq);
						xml.attribute("quality", ap.signal);
						if (ap.wpa()) { xml.attribute("protection", ap.prot); }
					});

					auto check_existence = [&] (Accesspoint &lap) {
						connecting_attempt |= (lap.ssid == ap.ssid) && ap.auto_connect;
					};
					_for_each_ap(check_existence);
				});
			});

		} catch (...) { /* silently omit report */ }

		try {
			if (!_connected_ap.bssid_valid() && connecting_attempt) {

				Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
					xml.node("accesspoint", [&] () {
						xml.attribute("state", "connecting");
					});
				});
			}
		} catch (...) { /* silently omit state */ }
	}

	/* network commands */

	void _mark_stale_aps(Genode::Xml_node const &config)
	{
		auto mark_stale = [&] (Accesspoint &ap) {
			ap.stale = true;

			config.for_each_sub_node("network", [&] ( Genode::Xml_node node) {
				Accesspoint::Ssid ssid = node.attribute_value("ssid",  Accesspoint::Ssid(""));

				if (ap.ssid == ssid) { ap.stale = false; }
			});
		};
		_for_each_ap(mark_stale);

		_remove_stale_aps();
	}

	void _remove_stale_aps()
	{
		if (_state != State::IDLE) {
			Genode::warning("cannot remove stale APs in non-idle state "
			                "(", state_strings(_state), ")");
			return;
		}

		if (_processed_ap) { return; }

		_aps.for_each([&] (Accesspoint &ap) {
			if (!_processed_ap && ap.valid() && ap.stale) {
				_processed_ap = &ap;
			}
		});

		if (!_processed_ap) {
			/* TODO move State transition somewhere more sane */
			_state_transition(_state, State::IDLE);
			_add_new_aps();
			return;
		}

		if (_verbose) {
			Genode::log("Remove network: '", _processed_ap->ssid, "'");
		}

		_state_transition(_state, State::REMOVE_NETWORK);
		_submit_cmd(Cmd_str("REMOVE_NETWORK ", _processed_ap->id));
	}

	void _update_aps()
	{
		if (_state != State::IDLE) {
			Genode::warning("cannot enable network in non-idle state");
			return;
		}

		if (_processed_ap) { return; }

		_aps.for_each([&] (Accesspoint &ap) {
			if (!_processed_ap && ap.stored() && ap.update) {
				_processed_ap = &ap;
			}
		});

		if (!_processed_ap) { return; }

		if (_verbose) {
			Genode::log("Update network: '", _processed_ap->ssid, "'");
		}

		/* re-use state to change PSK */
		_state_transition(_state, State::FILL_NETWORK_PSK);
		_network_set_psk();
	}


	void _add_new_aps()
	{
		if (_state != State::IDLE) {
			Genode::warning("cannot enable network in non-idle state");
			return;
		}

		if (_processed_ap) { return; }

		_aps.for_each([&] (Accesspoint &ap) {
			if (!_processed_ap && ap.valid() && !ap.stored()) {
				_processed_ap = &ap;
			}
		});

		if (!_processed_ap) {
			/* XXX move State transition somewhere more sane */
			_state_transition(_state, State::IDLE);
			_update_aps();
			return;
		}

		if (_verbose) {
			Genode::log("Add network: '", _processed_ap->ssid, "'");
		}

		_state_transition(_state, State::ADD_NETWORK);
		_submit_cmd(Cmd_str("ADD_NETWORK"));
	}

	void _network_enable(Accesspoint &ap)
	{
		if (_state != State::IDLE) {
			Genode::warning("cannot enable network in non-idle state");
			return;
		}

		if (ap.enabled) { return; }

		if (_verbose) {
			Genode::log("Enable network: ", ap.id, " '", ap.ssid, "'");
		}

		ap.enabled = true;

		_state_transition(_state, State::ENABLE_NETWORK);
		_submit_cmd(Cmd_str("ENABLE_NETWORK ", ap.id));
	}

	void _network_disable(Accesspoint &ap)
	{
		if (_state != State::IDLE) {
			Genode::warning("cannot enable network in non-idle state");
			return;
		}

		if (!ap.enabled) { return; }

		if (_verbose) {
			Genode::log("Disable network: ", ap.id, " '", ap.ssid, "'");
		}

		ap.enabled = false;

		_state_transition(_state, State::DISABLE_NETWORK);
		_submit_cmd(Cmd_str("DISABLE_NETWORK ", ap.id));
	}

	void _network_set_ssid(char const *msg)
	{
		long id = -1;
		Genode::ascii_to(msg, id);

		_processed_ap->id = static_cast<int>(id);
		_submit_cmd(Cmd_str("SET_NETWORK ", _processed_ap->id,
		                     " ssid \"", _processed_ap->ssid, "\""));
	}

	void _network_set_bssid()
	{
		bool const valid = _processed_ap->bssid.length() == 17 + 1;
		char const *bssid = valid ? _processed_ap->bssid.string() : "";

		_submit_cmd(Cmd_str("SET_NETWORK ", _processed_ap->id,
		                     " bssid ", bssid));
	}

	void _network_set_key_mgmt_sae()
	{
		_submit_cmd(Cmd_str("SET_NETWORK ", _processed_ap->id,
		                     " key_mgmt SAE"));
	}

	void _network_set_pmf()
	{
		_submit_cmd(Cmd_str("SET_NETWORK ", _processed_ap->id,
		                     " ieee80211w 2"));
	}

	void _network_set_psk()
	{
		if (_processed_ap->wpa()) {
			_submit_cmd(Cmd_str("SET_NETWORK ", _processed_ap->id,
			                     " psk \"", _processed_ap->pass, "\""));
		} else {
			_submit_cmd(Cmd_str("SET_NETWORK ", _processed_ap->id,
			                     " key_mgmt NONE"));
		}
	}

	/* result handling */

	bool _scan_busy { false };

	void _handle_scan_results(State state, char const *msg)
	{
		switch (state) {
		case State::INITIATE_SCAN:
			if (!cmd_successful(msg)) {
				_scan_busy = Genode::strcmp(msg, "FAIL-BUSY");
				if (!_scan_busy) {
					Genode::warning("could not initiate scan: ", msg);
				}
			}
			_state_transition(_state, State::IDLE);
			break;
		case State::PENDING_RESULTS:
			if (scan_results(msg)) {
				_state_transition(_state, State::IDLE);
				_generate_scan_results_report(msg);
			}
			break;
		default:
			Genode::warning("unknown SCAN state: ", msg);
			break;
		}
	}

	void _handle_network_results(State state, char const *msg)
	{
		bool successfully = false;

		switch (state) {
		case State::ADD_NETWORK:
			if (cmd_fail(msg)) {
				Genode::error("could not add network: ", msg);
				_state_transition(_state, State::IDLE);
			} else {
				_state_transition(_state, State::FILL_NETWORK_SSID);
				_network_set_ssid(msg);

				successfully = true;
			}
			break;
		case State::REMOVE_NETWORK:
		{
			_state_transition(_state, State::IDLE);

			Accesspoint &ap = *_processed_ap;
			/* reset processed AP as this is an end state */
			_processed_ap = nullptr;

			if (cmd_fail(msg)) {
				Genode::error("could not remove network: ", msg);
			} else {
				_free_ap(ap);

				/* trigger the next round */
				_remove_stale_aps();

				successfully = true;
			}
			break;
		}
		case State::FILL_NETWORK_SSID:
			_state_transition(_state, State::IDLE);

			if (!cmd_successful(msg)) {
				Genode::error("could not set ssid for network: ", msg);
				_state_transition(_state, State::IDLE);
			} else {
				_state_transition(_state, State::FILL_NETWORK_BSSID);
				_network_set_bssid();

				successfully = true;
			}
			break;
		case State::FILL_NETWORK_BSSID:
			_state_transition(_state, State::IDLE);

			if (!cmd_successful(msg)) {
				Genode::error("could not set bssid for network: ", msg);
				_state_transition(_state, State::IDLE);
			} else {

				/*
				 * For the moment branch here to handle WPA3-personal-only
				 * explicitly.
				 */
				if (_processed_ap->wpa3()) {
					_state_transition(_state, State::FILL_NETWORK_KEY_MGMT);
					_network_set_key_mgmt_sae();
				} else {
					_state_transition(_state, State::FILL_NETWORK_PSK);
					_network_set_psk();
				}

				successfully = true;
			}
			break;
		case State::FILL_NETWORK_KEY_MGMT:
			_state_transition(_state, State::IDLE);

			if (!cmd_successful(msg)) {
				Genode::error("could not set key_mgmt for network: ", msg);
				_state_transition(_state, State::IDLE);
			} else {
				_state_transition(_state, State::SET_NETWORK_PMF);
				_network_set_pmf();

				successfully = true;
			}
			break;
		case State::SET_NETWORK_PMF:
			_state_transition(_state, State::IDLE);

			if (!cmd_successful(msg)) {
				Genode::error("could not set PMF for network: ", msg);
				_state_transition(_state, State::IDLE);
			} else {
				_state_transition(_state, State::FILL_NETWORK_PSK);
				_network_set_psk();

				successfully = true;
			}
			break;
		case State::FILL_NETWORK_PSK:
		{
			_state_transition(_state, State::IDLE);

			Accesspoint &ap = *_processed_ap;

			if (!cmd_successful(msg)) {
				Genode::error("could not set passphrase for network: ", msg);
			} else {

				/*
				 * Disable network to trick wpa_supplicant into reloading
				 * the settings.
				 */
				if (ap.update) {
					ap.enabled = true;
					_network_disable(ap);
				} else

				if (ap.auto_connect) {
					_network_enable(ap);
				} else {
					/* trigger the next round */
					_add_new_aps();
				}

				successfully = true;
			}
			break;
		}
		case State::ENABLE_NETWORK:
		{
			_state_transition(_state, State::IDLE);

			/* reset processed AP as this is an end state */
			_processed_ap = nullptr;

			if (!cmd_successful(msg)) {
				Genode::error("could not enable network: ", msg);
			} else {
				/* trigger the next round */
				_add_new_aps();

				successfully = true;
			}
			break;
		}
		case State::DISABLE_NETWORK:
		{
			_state_transition(_state, State::IDLE);

			Accesspoint &ap = *_processed_ap;
			/* reset processed AP as this is an end state */
			_processed_ap = nullptr;

			if (!cmd_successful(msg)) {
				Genode::error("could not disable network: ", msg);
			} else {

				/*
				 * Updated settings are applied, enable the network
				 * anew an try again.
				 */
				if (ap.update) {
					ap.update = false;

					if (ap.auto_connect) {
						_network_enable(ap);
					}
				}

				successfully = true;
			}
			break;
		}
		case State::LIST_NETWORKS:
			_state_transition(_state, State::IDLE);

			if (list_network_results(msg)) {
				Genode::error("List networks:\n", msg);
			}
			break;
		default:
			Genode::warning("unknown network state: ", msg);
			break;
		}

		/*
		 * If some step failed we have to generate a fake
		 * disconnect event.
		 */
		if (_single_autoconnect && !successfully) {
			try {
				Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
					xml.node("accesspoint", [&] () {
						xml.attribute("state", "disconnected");
						xml.attribute("rfkilled", _rfkilled);
						xml.attribute("config_error", true);
					});
				});

				_single_autoconnect = false;
			} catch (...) { }
		}
	}

	void _handle_status_result(State &state, char const *msg)
	{
		_state_transition(state, State::IDLE);

		/*
		 * Querying the status might have failed but we already sent
		 * out a rudimentary report, just stop here.
		 */
		if (0 == msg[0]) { return; }

		Accesspoint ap { };

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

		if (!ap.ssid.valid()) {
			Genode::error("Cannot query SSID :-(");
			return;
		}

		Accesspoint *p = _lookup_ap_by_ssid(ap.ssid);
		if (p) {
			p->bssid = ap.bssid;
			p->freq  = ap.freq;
		}

		_connected_ap.ssid = ap.ssid;

		Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
			xml.node("accesspoint", [&] () {
				xml.attribute("ssid",  ap.ssid);
				xml.attribute("bssid", ap.bssid);
				xml.attribute("freq",  ap.freq);
				xml.attribute("state", "connected");
			});
		});
	}

	void _handle_info_result(State &state, char const *msg)
	{
		_state_transition(state, State::IDLE);

		if (!_connected_event && !_disconnected_event) { return; }

		/*
		 * It might happen that the supplicant already flushed
		 * its internal BSS information and cannot help us out.
		 * Since we already sent out a rudimentary report, just
		 * stop here.
		 */
		if (0 == msg[0]) { return; }

		Accesspoint ap { };

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

		/*
		 * When the config is changed while we are still connecting and
		 * for some reasons the accesspoint does not get disabled
		 * a connected event could arrive and we will get a nullptr...
		 */
		Accesspoint *p = _lookup_ap_by_ssid(ap.ssid);

		/*
		 * ... but we still generate a report and let the management
		 * component deal with it.
		 */
		Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
			xml.node("accesspoint", [&] () {
				xml.attribute("ssid",  ap.ssid);
				xml.attribute("bssid", ap.bssid);
				xml.attribute("freq",  ap.freq);
				xml.attribute("state", _connected_event ? "connected"
				                                        : "disconnected");
				if (!_connected_event) {
					xml.attribute("rfkilled", _rfkilled);
					xml.attribute("auth_failure", _disconnected_fail);
				}
			});
		});

		if (_disconnected_fail) {
			/*
			 * Being able to remove a failed network from the internal
			 * state of the supplicant relies on a sucessful BSS request.
			 * In case that failes the supplicant will try to join the
			 * network again and again...
			 */
			if (!p || _processed_ap) {
				Genode::error("cannot disabled failed network");
			} else {
				_processed_ap = p;
				_network_disable(*_processed_ap);
			}
		} else

		if (_connected_event) {
			/*
			 * In case the BSS cmd did not return a valid SSID, which
			 * was observed only with hidden networks so far, check the
			 * current status.
			 */
			if (!p) {
				_state_transition(state, State::STATUS);
				_submit_cmd(Cmd_str("STATUS"));

			} else {
				p->bssid = ap.bssid;
				p->freq  = ap.freq;
			}

			_connected_ap = ap;
		}
	}

	/* connection state */

	Genode::Constructible<Genode::Reporter> _state_reporter { };

	Accesspoint::Bssid _connecting { };

	Accesspoint::Bssid const _extract_bssid(char const *msg, State state)
	{
		char bssid[32] = { };
		/* by the power of wc -c, I have the start pos... */
		enum { BSSID_CONNECT = 37, BSSID_DISCONNECT = 30, BSSID_CONNECTING = 33, };

		bool const connected  = state == State::CONNECTED;
		bool const connecting = state == State::CONNECTING;

		size_t const len   = 17;
		size_t const start = connected ? BSSID_CONNECT
		                               : connecting ? BSSID_CONNECTING
		                                            : BSSID_DISCONNECT;
		Genode::memcpy(bssid, msg + start, len);
		return Accesspoint::Bssid((char const*)bssid);
	}

	bool _auth_failure(char const *msg)
	{
		enum { REASON_OFFSET = 55, };
		unsigned reason = 0;
		Genode::ascii_to((msg + REASON_OFFSET), reason);
		switch (reason) {
		case  2: /* prev auth no longer valid */
		case 15: /* 4-way handshake timeout/failed */
			return true;
		default:
			return false;
		}
	}

	/* events */

	bool _connected_event    { false };
	bool _disconnected_event { false };
	bool _disconnected_fail  { false };
	bool _was_connected      { false };

	enum { MAX_REAUTH_ATTEMPTS = 1 };
	unsigned _reauth_attempts { 0 };

	enum { MAX_ATTEMPTS = 3, };
	unsigned _scan_attempts { 0 };

	Accesspoint::Bssid _pending_bssid { };

	void _handle_connection_events(char const *msg)
	{
		_connected_event    = false;
		_disconnected_event = false;
		_disconnected_fail  = false;

		bool const connected    = check_recv_msg(msg, recv_table[Rmi::CONNECTED]);
		bool const disconnected = check_recv_msg(msg, recv_table[Rmi::DISCONNECTED]);
		bool const auth_failed  = disconnected && _auth_failure(msg);

		State state = connected ? State::CONNECTED : State::DISCONNECTED;
		Accesspoint::Bssid const &bssid = _extract_bssid(msg, state);

		/* simplistic heuristic to ignore re-authentication requests */
		if (_connected_ap.bssid.valid() && auth_failed) {
			if (_reauth_attempts < MAX_ATTEMPTS) {
				Genode::log("ignore deauth from: ", _connected_ap.bssid);
				_reauth_attempts++;
				return;
			}
		}
		_reauth_attempts = 0;

		/*
		 * Always reset the "global" connection state first
		 */
		_connected_ap.invalidate();
		if (connected) { _connected_ap.bssid = bssid; }
		if (connected || disconnected) { _connecting = Accesspoint::Bssid(); }

		/*
		 * Save local connection state here for later re-use when
		 * the BSS information are handled.
		 */
		_connected_event    = connected;
		_disconnected_event = disconnected;
		_disconnected_fail  = auth_failed;

		if (!_rfkilled) {

			/*
			 * As we only received the BSSID, try to gather more information
			 * so we may generate a more thorough follow-up state report.
			 */
			if (_state != State::IDLE) {
				_pending_bssid = bssid;
			} else {
				_state_transition(_state, State::INFO);
				_submit_cmd(Cmd_str("BSS ", bssid));
			}

			_arm_scan_timer(connected);
		}

		/*
		 * Generate the first rudimentary report whose missing information
		 * are (potentially) filled in later (see above).
		 */
		Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
			xml.node("accesspoint", [&] () {
				xml.attribute("bssid", bssid);
				xml.attribute("state", connected ? "connected"
				                                 : "disconnected");
				if (disconnected) {
					xml.attribute("rfkilled", _rfkilled);
					if (auth_failed) {
						xml.attribute("auth_failure", auth_failed);
					}
				}
			});
		});

		/* reset */
		_single_autoconnect = false;
	}

	Genode::Signal_handler<Wifi::Frontend> _events_handler;

	unsigned _last_event_id { 0 };

	void _handle_events()
	{
		char const *msg = reinterpret_cast<char const*>(_msg.event);
		unsigned const event_id = _msg.event_id;

		/* return early */
		if (_last_event_id == event_id) {
			_notify_lock_unlock();
			return;
		}

		if (results_available(msg)) {

			/*
			 * We might have to pull the socketcall task out of poll_all()
			 * because otherwise we might be late and wpa_supplicant has
			 * already removed all scan results due to BSS age settings.
			 */
			wifi_kick_socketcall();

			if (_state == State::IDLE) {
				_state_transition(_state, State::PENDING_RESULTS);
				_submit_cmd(Cmd_str("SCAN_RESULTS"));
			}
		} else

		if (connecting_to_network(msg)) {
			if (!_single_autoconnect) {
				Accesspoint::Bssid const &bssid = _extract_bssid(msg, State::CONNECTING);
				_connecting = bssid;

				Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
					xml.node("accesspoint", [&] () {
						xml.attribute("bssid", bssid);
						xml.attribute("state", "connecting");
					});
				});
			}
		} else

		if (network_not_found(msg)) {

			/* always try to update the accesspoint list */
			if (_state == State::IDLE) {
				_state_transition(_state, State::PENDING_RESULTS);
				_submit_cmd(Cmd_str("SCAN_RESULTS"));
			}

			if (_single_autoconnect && ++_scan_attempts >= MAX_ATTEMPTS) {
				_scan_attempts = 0;
				_single_autoconnect = false;

				Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
					xml.node("accesspoint", [&] () {
						xml.attribute("state", "disconnected");
						xml.attribute("rfkilled", _rfkilled);
						xml.attribute("not_found", true);
					});
				});
			}

		} else

		{
			_handle_connection_events(msg);
		}

		_notify_lock_unlock();
	}

	Genode::Signal_handler<Wifi::Frontend> _cmd_handler;

	unsigned _last_recv_id { 0 };

	void _handle_cmds()
	{
		char const *msg = reinterpret_cast<char const*>(_msg.recv);
		unsigned const recv_id = _msg.recv_id;


		/* return early */
		if (_last_recv_id == recv_id) {
			_notify_lock_unlock();
			return;
		}

		_last_recv_id = recv_id;

		switch (_state & 0xf) {
		case State::SCAN:
			_handle_scan_results(_state, msg);
			break;
		case State::NETWORK:
			_handle_network_results(_state, msg);
			break;
		case State::STATUS:
			_handle_status_result(_state, msg);
			break;
		case State::INFO:
			_handle_info_result(_state, msg);
			break;
		case State::IDLE:
		default:
			break;
		}
		_notify_lock_unlock();

		if (_verbose_state) {
			Genode::log("State:",
			            " connected: ",  _connected_ap.bssid_valid(),
			            " connecting: ", _connecting.length() > 1,
			            " enabled: ",    _count_enabled(),
			            " stored: ",     _count_stored(),
			"");
		}

		if (_state == State::IDLE && _deferred_config_update) {
			_deferred_config_update = false;
			_handle_config_update();
		}

		if (_state == State::IDLE && _pending_bssid.length() > 1) {
			_state_transition(_state, State::INFO);
			_submit_cmd(Cmd_str("BSS ", _pending_bssid));

			_pending_bssid = Accesspoint::Bssid();
		}
	}

	/**
	 * Constructor
	 */
	Frontend(Genode::Env &env, Msg_buffer &msg_buffer)
	:
		_ap_allocator(env.ram(), env.rm()),
		_msg(msg_buffer),
		_rfkill_handler(env.ep(), *this, &Wifi::Frontend::_handle_rfkill),
		_config_rom(env, "wifi_config"),
		_config_sigh(env.ep(), *this, &Wifi::Frontend::_handle_config_update),
		_scan_timer(env),
		_scan_timer_sigh(env.ep(), *this, &Wifi::Frontend::_handle_scan_timer),
		_events_handler(env.ep(), *this, &Wifi::Frontend::_handle_events),
		_cmd_handler(env.ep(),    *this, &Wifi::Frontend::_handle_cmds)
	{
		_config_rom.sigh(_config_sigh);
		_scan_timer.sigh(_scan_timer_sigh);

		/* set/initialize as unblocked */
		_notify_blockade.wakeup();

		try {
			_ap_reporter.construct(env, "accesspoints", "accesspoints");
			_ap_reporter->generate([&] (Genode::Xml_generator &) { });
		} catch (...) {
			Genode::warning("no Report session available, scan results will "
			                "not be reported");
		}

		try {
			_state_reporter.construct(env, "state");
			_state_reporter->enabled(true);

			Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
				xml.node("accesspoint", [&] () {
					xml.attribute("state", "disconnected");
					xml.attribute("rfkilled", _rfkilled);
				});
			});
		} catch (...) {
			Genode::warning("no Report session available, connectivity will "
			                "not be reported");
		}

		/* read in list of APs */
		_config_update(false);

		/* get initial RFKILL state */
		_handle_rfkill();

		/* kick-off initial scanning */
		_handle_scan_timer();
	}

	/**
	 * Trigger RFKILL notification
	 *
	 * Used by the wifi driver to notify front end.
	 */
	void rfkill_notify() override
	{
		_rfkill_handler.local_submit();
	}

	/**
	 * Get result signal capability
	 *
	 * Used by the wpa_supplicant to notify front end after processing
	 * a command.
	 */
	Genode::Signal_context_capability result_sigh()
	{
		return _cmd_handler;
	}

	/**
	 * Get event signal capability
	 *
	 * Used by the wpa_supplicant to notify front whenever a event
	 * was triggered.
	 */
	Genode::Signal_context_capability event_sigh()
	{
		return _events_handler;
	}

	/**
	 * Block until events were handled by the front end
	 *
	 * Used by the wpa_supplicant to wait for the front end.
	 */
	void block_for_processing() { _notify_lock_lock(); }
};

#endif /* _WIFI_FRONTEND_H_ */
