
/*
 * \brief  Child representation
 * \author Norman Feske
 * \date   2018-01-23
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <child.h>

using namespace Depot_deploy;

enum { GLOBAL_LOG_BUF_SZ = 1024 * 1024 };
static char   global_log_buf[GLOBAL_LOG_BUF_SZ];
static char  *global_log_buf_base { global_log_buf };
static size_t global_log_buf_sz   { GLOBAL_LOG_BUF_SZ };


static void forward_to_log(Genode::uint64_t const sec,
                           Genode::uint64_t const ms,
                           char      const *const base,
                           char      const *const end)
{
	log(sec, ".", ms < 10 ? "00" : ms < 100 ? "0" : "", ms, " ",
	    Cstring(base, end - base));
}


void Child::gen_start_node(Xml_generator          &xml,
                           Xml_node                common,
                           Depot_rom_server const &cached_depot_rom,
                           Depot_rom_server const &uncached_depot_rom)
{
	if (_state != UNFINISHED) {
		while (Timeout_event *event = _timeout_events.first()) {
			_timeout_events.remove(event);
			destroy(_alloc, event);
		}
		while (Log_event *event = _log_events.first()) {
			_log_events.remove(event);
			destroy(_alloc, event);
		}
		return;
	}

	if (_skip) {

		log("");
		log("--- Run \"", _name, "\" (max 1 sec) ---");
		log("");

		_state = State::SKIPPED;

		char name_padded[32];
		for (size_t i = 0; i < sizeof(name_padded) - 1; name_padded[i++] = ' ');
		name_padded[sizeof(name_padded) - 1] = '\0';
		memcpy(name_padded, _name.string(), min(_name.length() - 1, sizeof(name_padded) - 1));

		_conclusion = Conclusion {
			Cstring(name_padded), " skipped" };

		log(" ", _conclusion);
		_config_handler.submit();

		return;
	}

	if (!_configured() || _condition == UNSATISFIED)
		return;

	if (_defined_by_launcher() && !_launcher_xml.constructed())
		return;

	if (!_pkg_xml->xml().has_sub_node("runtime")) {
		warning("blueprint for '", _name, "' lacks runtime information");
		return;
	}

	Xml_node const runtime = _pkg_xml->xml().sub_node("runtime");

	xml.node("start", [&] () {

		xml.attribute("name", _name);

		unsigned long caps = _pkg_cap_quota;
		if (_defined_by_launcher())
			caps = _launcher_xml->xml().attribute_value("caps", caps);
		caps = _start_xml->xml().attribute_value("caps", caps);

		xml.attribute("caps", caps);

		typedef String<64> Version;
		Version const version = _start_xml->xml().attribute_value("version", Version());
		if (version.valid())
			xml.attribute("version", version);

		xml.node("binary", [&] () { xml.attribute("name", _binary_name); });

		Number_of_bytes ram = _pkg_ram_quota;
		if (_defined_by_launcher())
			ram = _launcher_xml->xml().attribute_value("ram", ram);
		ram = _start_xml->xml().attribute_value("ram", ram);

		xml.node("resource", [&] () {
			xml.attribute("name", "RAM");
			xml.attribute("quantum", String<32>(ram));
		});

		/*
		 * Insert inline '<config>' node if provided by the start node,
		 * the launcher definition (if a launcher is user), or the
		 * blueprint. The former is preferred over the latter.
		 */
		if (_start_xml->xml().has_sub_node("config")) {
			_gen_copy_of_sub_node(xml, _start_xml->xml(), "config");
		} else {
			if (_defined_by_launcher() && _launcher_xml->xml().has_sub_node("config")) {
				_gen_copy_of_sub_node(xml, _launcher_xml->xml(), "config");
			} else {
				if (runtime.has_sub_node("config"))
					_gen_copy_of_sub_node(xml, runtime, "config");
			}
		}

		/*
		 * Declare services provided by the subsystem.
		 */
		if (runtime.has_sub_node("provides")) {
			xml.node("provides", [&] () {
				runtime.sub_node("provides").for_each_sub_node([&] (Xml_node service) {
					_gen_provides_sub_node(xml, service, "audio_in",    "Audio_in");
					_gen_provides_sub_node(xml, service, "audio_out",   "Audio_out");
					_gen_provides_sub_node(xml, service, "block",       "Block");
					_gen_provides_sub_node(xml, service, "file_system", "File_system");
					_gen_provides_sub_node(xml, service, "framebuffer", "Framebuffer");
					_gen_provides_sub_node(xml, service, "input",       "Input");
					_gen_provides_sub_node(xml, service, "log",         "LOG");
					_gen_provides_sub_node(xml, service, "nic",         "Nic");
					_gen_provides_sub_node(xml, service, "nitpicker",   "Nitpicker");
					_gen_provides_sub_node(xml, service, "report",      "Report");
					_gen_provides_sub_node(xml, service, "rom",         "ROM");
					_gen_provides_sub_node(xml, service, "terminal",    "Terminal");
					_gen_provides_sub_node(xml, service, "timer",       "Timer");
				});
			});
		}

		xml.node("route", [&] () {
			_gen_routes(xml, common, cached_depot_rom, uncached_depot_rom); });
	});
	if (_running) {
		return; }

	Genode::uint64_t max_timeout_sec = 0;
	try {
		Xml_node const events = _pkg_xml->xml().sub_node("runtime").sub_node("events");
		events.for_each_sub_node("timeout", [&] (Xml_node const &event) {
			try {
				Timeout_event &timeout = *new (_alloc) Timeout_event(_timer, *this, event);
				if (timeout.sec() > max_timeout_sec) {
					max_timeout_sec = timeout.sec();
				}
				_timeout_events.insert(&timeout);
			}
			catch (Timeout_event::Invalid) { warning("Invalid timeout event"); }
		});
		events.for_each_sub_node("log", [&] (Xml_node const &event) {
			_log_events.insert(new (_alloc) Log_event(event));
		});
	}
	catch (...) { }
	log("");
	log("--- Run \"", _name, "\" (max ", max_timeout_sec, " sec) ---");
	log("");
	_running = true;
	init_time_us = _timer.curr_time().trunc_to_plain_us().value;
}


void Child::_gen_routes(Xml_generator          &xml,
                        Xml_node                common,
                        Depot_rom_server const &cached_depot_rom,
                        Depot_rom_server const &uncached_depot_rom) const
{
	if (_skip) {
		return; }

	if (!_pkg_xml.constructed())
		return;

	typedef String<160> Path;

	/*
	 * Add routes given in the start node.
	 */
	if (_start_xml->xml().has_sub_node("route")) {
		Xml_node const route = _start_xml->xml().sub_node("route");
		route.with_raw_content([&] (char const *start, size_t length) {
			xml.append(start, length); });
	}

	/*
	 * Add routes given in the launcher definition.
	 */
	if (_launcher_xml.constructed() && _launcher_xml->xml().has_sub_node("route")) {
		Xml_node const route = _launcher_xml->xml().sub_node("route");
		route.with_raw_content([&] (char const *start, size_t length) {
			xml.append(start, length); });
	}

	/**
	 * Return name of depot-ROM server used for obtaining the 'path'
	 *
	 * If the depot path refers to the depot-user "local", route the
	 * session request to the non-cached ROM service.
	 */
	auto rom_server = [&] (Path const &path) {

		return (String<7>(path) == "local/") ? uncached_depot_rom
		                                     : cached_depot_rom;
	};

	/*
	 * Redirect config ROM request to label as given in the 'config' attribute,
	 * if present. We need to search the blueprint's <rom> nodes for the
	 * matching ROM module to rewrite the label with the configuration's path
	 * within the depot.
	 */
	if (_config_name.valid()) {
		_pkg_xml->xml().for_each_sub_node("rom", [&] (Xml_node rom) {

			if (!rom.has_attribute("path"))
				return;

			if (rom.attribute_value("label", Name()) != _config_name)
				return;

			/* we found the <rom> node for the config ROM */
			xml.node("service", [&] () {
				xml.attribute("name",  "ROM");
				xml.attribute("label", "config");
				typedef String<160> Path;
				Path const path = rom.attribute_value("path", Path());

				if (cached_depot_rom.valid())
					xml.node("child", [&] () {
						xml.attribute("name", rom_server(path));
						xml.attribute("label", path); });
				else
					xml.node("parent", [&] () {
						xml.attribute("label", path); });
			});
		});
	}

	/*
	 * Add common routes as defined in our config.
	 */
	common.with_raw_content([&] (char const *start, size_t length) {
		xml.append(start, length); });

	/*
	 * Add ROM routing rule with the label rewritten to the path within the
	 * depot.
	 */
	_pkg_xml->xml().for_each_sub_node("rom", [&] (Xml_node rom) {

		if (!rom.has_attribute("path"))
			return;

		typedef Name Label;
		Path  const path  = rom.attribute_value("path",  Path());
		Label const label = rom.attribute_value("label", Label());

		xml.node("service", [&] () {
			xml.attribute("name", "ROM");
			xml.attribute("label_last", label);

			if (cached_depot_rom.valid()) {
				xml.node("child", [&] () {
					xml.attribute("name",  rom_server(path));
					xml.attribute("label", path);
				});
			} else {
				xml.node("parent", [&] () {
					xml.attribute("label", path); });
			}
		});
	});
}


bool Child::_defined_by_launcher() const
{
	if (_skip) {
		return false; }

	/*
	 * If the <start> node lacks a 'pkg' attribute, we expect the
	 * policy to be defined by a launcher XML snippet.
	 */
	return _start_xml.constructed() && !_start_xml->xml().has_attribute("pkg");
}


Archive::Path Child::_config_pkg_path() const
{
	if (_skip) {
		return Archive::Path(); }

	if (_defined_by_launcher() && _launcher_xml.constructed())
		return _launcher_xml->xml().attribute_value("pkg", Archive::Path());

	return _start_xml->xml().attribute_value("pkg", Archive::Path());
}


Child::Launcher_name Child::_launcher_name() const
{
	if (_skip) {
		return Launcher_name(); }

	if (!_defined_by_launcher())
		return Launcher_name();

	if (_start_xml->xml().has_attribute("launcher"))
		return _start_xml->xml().attribute_value("launcher", Launcher_name());

	return _start_xml->xml().attribute_value("name", Launcher_name());
}


bool Child::_configured() const
{
	if (_skip) {
		return false; }

	return _pkg_xml.constructed()
	   && (_config_pkg_path() == _blueprint_pkg_path);
}


void Child::_gen_provides_sub_node(Xml_generator        &xml,
                                   Xml_node              service,
                                   Xml_node::Type const &node_type,
                                   Service::Name  const &service_name)
{
	if (service.type() == node_type)
		xml.node("service", [&] () {
			xml.attribute("name", service_name); });
}


void Child::_gen_copy_of_sub_node(Xml_generator        &xml,
                                  Xml_node              from_node,
                                  Xml_node::Type const &sub_node_type)
{
	if (!from_node.has_sub_node(sub_node_type.string()))
		return;

	Xml_node const sub_node = from_node.sub_node(sub_node_type.string());
	sub_node.with_raw_node([&] (char const *start, size_t length) {
		xml.append(start, length); });
}


Child::Child(Allocator                       &alloc,
             Xml_node                         start_node,
             Timer::Connection               &timer,
             Signal_context_capability const &config_handler)
:
	_skip           { start_node.attribute_value("skip", false) },
	_alloc          { alloc },
	_start_xml      { _alloc, start_node },
	_name           { _start_xml->xml().attribute_value("name", Name()) },
	_timer          { timer },
	_config_handler { config_handler }
{ }


Child::~Child()
{
	while (Timeout_event *event = _timeout_events.first()) {
		_timeout_events.remove(event);
		destroy(_alloc, event);
	}
	while (Log_event *event = _log_events.first()) {
		_log_events.remove(event);
		destroy(_alloc, event);
	}
}


void Child::log_session_write(Log_event::Line const &log_line)
{
	if (_skip) {
		return; }

	enum { ASCII_ESC = 27 };
	enum { ASCII_LF  = 10 };
	enum { ASCII_TAB = 9 };

	struct Break : Exception { };

	
	struct Skip_escape_sequence
	{
		char const * const base;
		size_t       const size;
	};
	struct Replace_ampersend_sequence
	{
		char const * const base;
		size_t       const size;
		char         const by;
	};
	static Skip_escape_sequence skip_esc_seq[5]
	{
		{ "[0m", 3 },
		{ "[31m", 4 },
		{ "[32m", 4 },
		{ "[33m", 4 },
		{ "[34m", 4 },
	};
	static Replace_ampersend_sequence replace_amp_seq[3]
	{
		{ "lt;", 3, '<' },
		{ "amp;", 4, '&' },
		{ "#42;", 4, '*' }
	};

	/* calculate timestamp that prefixes*/
	Genode::uint64_t const time_us  { _timer.curr_time().trunc_to_plain_us().value - init_time_us };
	Genode::uint64_t       time_ms  { time_us / 1000UL };
	Genode::uint64_t const time_sec { time_ms / 1000UL };
	time_ms = time_ms - time_sec * 1000UL;

	char const *const log_base { log_line.string() };
	size_t      const log_sz   { strlen(log_base) };
	char const *const log_end  { log_base + log_sz };

	size_t const memcpy_sz { Genode::min(log_sz, global_log_buf_sz) };
	Genode::memcpy(global_log_buf_base, log_base, memcpy_sz);
	global_log_buf_base += memcpy_sz;
	global_log_buf_sz   -= memcpy_sz;

	try {
		char const *log_print { log_base };
		_log_events.for_each([&] (Log_event &log_event) {

			bool const log_matches { log_event.meaning() == "succeeded" };

			bool        match        { false };
			char const *pattern_end  { log_event.remaining_end() };
			char const *pattern_curr { log_event.remaining_base() };
			char const *log_curr     { log_base };

			for (;;) {

				/* handle end of pattern */
				if (pattern_curr == pattern_end) {
					match = true;
					log_event.remaining_base() = log_event.base();
					log_event.reset_to()       = log_event.base();
					log_event.reset_retry()    = false;
					break;
				}
				/* skip irrelevant characters in the pattern */
				if (*pattern_curr == ASCII_LF || *pattern_curr == ASCII_TAB) {
					pattern_curr++;
					continue;
				}
				if (*pattern_curr == '*') {
					pattern_curr++;
					log_event.reset_to()    = pattern_curr;
					log_event.reset_retry() = false;
					continue;
				}
				/* handle end of log line */
				if (log_curr == log_end) {
					log_event.remaining_base() = pattern_curr;
					break;
				}
				/* skip irrelevant characters in the log line */
				if (*log_curr == ASCII_LF) {

					/* forward to our log session a complete line */
					if (log_print < log_curr) {
						forward_to_log(time_sec, time_ms, log_print, log_curr);
						log_print = log_curr + 1;
					}
					log_curr++;
if (log_matches) {
					log_event.match_sz++;
					log_event.log_curr_idx++;
}
					continue;
				}
				if (*log_curr == ASCII_TAB) {
					log_curr++;
if (log_matches) {
					log_event.match_sz++;
					log_event.log_curr_idx++;
}
					continue;
				}
				/* skip irrelevant escape sequences in the log line */
				if (*log_curr == ASCII_ESC) {

					bool seq_match { false };

					for (unsigned i = 0; i < sizeof(skip_esc_seq)/sizeof(skip_esc_seq[0]); i++) {

						char const *seq_curr { skip_esc_seq[i].base };
						char const *seq_end  { seq_curr + skip_esc_seq[i].size };

						for (char const *log_seq_curr { log_curr + 1 } ; ; log_seq_curr++, seq_curr++) {

							if (seq_curr == seq_end) {
								seq_match = true;
								log_curr  = log_seq_curr;
if (log_matches) {
								log_event.match_sz     += skip_esc_seq[i].size+1;
								log_event.log_curr_idx += skip_esc_seq[i].size+1;
}
								break;
							}
							if (log_seq_curr == log_end) {
								break; }

							if (*log_seq_curr != *seq_curr) {
								break; }
						}
						if (seq_match) {
							break; }
					}
					if (seq_match) {
						continue; }
				}
				char   pattern_curr_san    = *pattern_curr;
				size_t pattern_curr_san_sz = 1;

				/* replace ampersend sequences in the pattern */
				if (*pattern_curr == '&') {

					bool seq_match { false };

					for (unsigned i = 0; i < sizeof(replace_amp_seq)/sizeof(replace_amp_seq[0]); i++) {

						char const *seq_curr { replace_amp_seq[i].base };
						char const *seq_end  { seq_curr + replace_amp_seq[i].size };

						for (char const *pattern_seq_curr { pattern_curr + 1 } ; ; pattern_seq_curr++, seq_curr++) {

							if (seq_curr == seq_end) {
								seq_match = true;
								pattern_curr_san    = replace_amp_seq[i].by;
								pattern_curr_san_sz = replace_amp_seq[i].size + 1;
								break;
							}
							if (pattern_seq_curr == pattern_end) {
								break; }

							if (*pattern_seq_curr != *seq_curr) {
								break; }
						}
						if (seq_match) {
							break;
						}
					}
				}
				/* check if log keeps matching pattern */
				if (*log_curr != pattern_curr_san) {

if (log_matches) {
					if (log_event.match_sz) {
						Log_event::Match *match = new (_alloc)
							Log_event::Match { (unsigned)(log_event.log_curr_idx-log_event.match_sz),
							                   (unsigned)(log_event.log_curr_idx-1) };

						log_event.matches.insert(match);
						log_event.match_sz = 0;
					}
}

					pattern_curr = log_event.reset_to();
					if (!log_event.reset_retry()) {

						log_curr++;

if (log_matches) {
						log_event.log_curr_idx++;
}
					}
					else {
						log_event.reset_retry() = false; }
				} else {
					pattern_curr += pattern_curr_san_sz;
					log_curr++;

if (log_matches) {
					log_event.match_sz++;
					log_event.log_curr_idx++;
}
					log_event.reset_retry() = true;
				}
			}
			/* forward to our log session what is left */
			if (log_print < log_curr) {
				for (;; log_curr++) {

					if (log_curr == log_end) {

						forward_to_log(time_sec, time_ms, log_print, log_curr);
						log_print = log_curr;
						break;
					}
					if (*log_curr == ASCII_LF) {

						forward_to_log(time_sec, time_ms, log_print, log_curr);
						log_print = log_curr + 1;
						break;
					}
				}
			}
			/* check if log line finished a match with the pattern */
			if (!match) {
				return; }

			/* execute event handler and stop trying further events */
			event_occured(log_event, time_us);
			throw Break();
		});
	}
	catch (...) { }
}


void Child::apply_config(Xml_node start_node)
{
	if (_skip)
		return;

	if (!start_node.differs_from(_start_xml->xml()))
		return;

	Archive::Path const old_pkg_path = _config_pkg_path();

	/* import new start node */
	_start_xml.construct(_alloc, start_node);

	Archive::Path const new_pkg_path = _config_pkg_path();

	/* invalidate blueprint if 'pkg' path changed */
	if (old_pkg_path != new_pkg_path) {
		_blueprint_pkg_path = new_pkg_path;
		_pkg_xml.destruct();

		/* reset error state, attempt to obtain the blueprint again */
		_pkg_incomplete = false;
	}
}


void Child::apply_blueprint(Xml_node pkg)
{
	if (_skip) {
		return; }

	if (pkg.attribute_value("path", Archive::Path()) != _blueprint_pkg_path)
		return;

	try {
		Xml_node const runtime = pkg.sub_node("runtime");

		/* package was missing but is installed now */
		_pkg_incomplete = false;

		_pkg_ram_quota = runtime.attribute_value("ram", Number_of_bytes());
		_pkg_cap_quota = runtime.attribute_value("caps", 0UL);

		_binary_name = runtime.attribute_value("binary", Binary_name());
		_config_name = runtime.attribute_value("config", Config_name());

		/* keep copy of the blueprint info */
		_pkg_xml.construct(_alloc, pkg);
	}
	catch (Xml_node::Nonexistent_sub_node) {
		error("missing runtime subnode in packege blueprint");
	}
}


void Child::apply_launcher(Launcher_name const &name,
                           Xml_node             launcher)
{
	if (_skip)
		return;

	if (!_defined_by_launcher())
		return;

	if (_launcher_name() != name)
		return;

	if (_launcher_xml.constructed() && !launcher.differs_from(_launcher_xml->xml()))
		return;

	_launcher_xml.construct(_alloc, launcher);

	_blueprint_pkg_path = _config_pkg_path();
}


Child::State_name Child::_padded_state_name() const
{
	if (_skip) {
		return "?"; }

	switch (_state) {
	case SUCCEEDED:  return "ok     ";
	case FAILED:     return "failed ";
	case SKIPPED:;
	case UNFINISHED: ;
	}
	return "?";
}


void Child::print_conclusion()
{
	log(" ", _conclusion);
}

void Child::conclusion(Result &result)
{
	struct Bad_state : Exception { };
	switch (_state) {
	case SUCCEEDED: result.succeeded++; break;
	case FAILED:    result.failed++;    break;
	case SKIPPED:   result.skipped++;   break;
	default:        throw Bad_state();
	}
}


void Child::mark_as_incomplete(Xml_node missing)
{
	if (_skip) {
		return; }

	/* print error message only once */
	if(_pkg_incomplete)
		return;

	Archive::Path const path = missing.attribute_value("path", Archive::Path());
	if (path != _blueprint_pkg_path)
		return;

	log(path, " incomplete or missing");

	_pkg_incomplete = true;
}


void Child::reset_incomplete()
{
	if (_skip) {
		return; }

	if (_pkg_incomplete) {
		_pkg_incomplete = false;
		_pkg_xml.destruct();
	}
}


bool Child::gen_query(Xml_generator &xml) const
{
	if (_skip) {
		return false; }

	if (_configured() || _pkg_incomplete)
		return false;

	if (_defined_by_launcher() && !_launcher_xml.constructed())
		return false;

	xml.node("blueprint", [&] () {
		xml.attribute("pkg", _blueprint_pkg_path); });

	return true;
}


void Child::gen_installation_entry(Xml_generator &xml) const
{
	if (_skip) {
		return; }

	if (!_pkg_incomplete) return;

	xml.node("archive", [&] () {
		xml.attribute("path", _config_pkg_path());
		xml.attribute("source", "no");
	});
}


void Child::event_occured(Event            const &event,
                          Genode::uint64_t const  time_us)
{
	if (_skip) {
		return; }

	if (_state != UNFINISHED) {
		return; }

	if      (event.meaning() == "succeeded") { _finished(SUCCEEDED, event, time_us); }
	else if (event.meaning() == "failed"   ) {

log("");
log("------- captured log ---------");
		unsigned idx = 0;
		for (char * curr = global_log_buf; curr < global_log_buf_base; ) {
			addr_t const remains { (addr_t)global_log_buf_base - (addr_t)curr };

			if      (remains >  16) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[2], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[3], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[4], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[5], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[6], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[7], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[8], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[9], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[10], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[11], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[12], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[13], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[14], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[15], Hex::OMIT_PREFIX, Hex::PAD)); curr += 16; }
			else if (remains == 15) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[2], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[3], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[4], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[5], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[6], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[7], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[8], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[9], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[10], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[11], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[12], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[13], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[14], Hex::OMIT_PREFIX, Hex::PAD)                    ); curr += 15; }
			else if (remains == 14) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[2], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[3], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[4], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[5], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[6], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[7], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[8], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[9], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[10], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[11], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[12], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[13], Hex::OMIT_PREFIX, Hex::PAD)                                        ); curr += 14; }
			else if (remains == 13) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[2], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[3], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[4], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[5], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[6], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[7], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[8], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[9], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[10], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[11], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[12], Hex::OMIT_PREFIX, Hex::PAD)                                                            ); curr += 13; }
			else if (remains == 12) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[2], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[3], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[4], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[5], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[6], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[7], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[8], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[9], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[10], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[11], Hex::OMIT_PREFIX, Hex::PAD)                                                                                ); curr += 12; }
			else if (remains == 11) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[2], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[3], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[4], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[5], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[6], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[7], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[8], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[9], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[10], Hex::OMIT_PREFIX, Hex::PAD)                                                                                                    ); curr += 11; }
			else if (remains == 10) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[2], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[3], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[4], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[5], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[6], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[7], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[8], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[9], Hex::OMIT_PREFIX, Hex::PAD)                                                                                                                        ); curr += 10; }
			else if (remains ==  9) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[2], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[3], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[4], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[5], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[6], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[7], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[8], Hex::OMIT_PREFIX, Hex::PAD)                                                                                                                                           ); curr += 9; }
			else if (remains ==  8) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[2], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[3], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[4], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[5], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[6], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[7], Hex::OMIT_PREFIX, Hex::PAD)); curr += 8; }
			else if (remains ==  7) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[2], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[3], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[4], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[5], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[6], Hex::OMIT_PREFIX, Hex::PAD)                   ); curr += 7; }
			else if (remains ==  6) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[2], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[3], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[4], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[5], Hex::OMIT_PREFIX, Hex::PAD)                                      ); curr += 6; }
			else if (remains ==  5) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[2], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[3], Hex::OMIT_PREFIX, Hex::PAD), "  ", Hex(curr[4], Hex::OMIT_PREFIX, Hex::PAD)                                                         ); curr += 5; }
			else if (remains ==  4) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[2], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[3], Hex::OMIT_PREFIX, Hex::PAD)                                                                            ); curr += 4; }
			else if (remains ==  3) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[2], Hex::OMIT_PREFIX, Hex::PAD)                                                                                               ); curr += 3; }
			else if (remains ==  2) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD), " ", Hex(curr[1], Hex::OMIT_PREFIX, Hex::PAD)                                                                                                                  ); curr += 2; }
			else if (remains ==  1) { log(Hex(idx, Hex::OMIT_PREFIX, Hex::PAD), ":  ", Hex(curr[0], Hex::OMIT_PREFIX, Hex::PAD)                                                                                                                                     ); curr += 1; }
			idx += 16;
		}
unsigned log_event_idx { 0 };
_log_events.for_each([&] (Log_event &log_event) {
	bool log_matches = (log_event.meaning() == "succeeded");
	if (log_matches) {
		log("");
		log("--- matches of log event ",log_event_idx++," ---");
	}
	while (Log_event::Match *match = log_event.matches.first()) {
		if (log_matches) {
			log("[", Hex(match->from, Hex::OMIT_PREFIX, Hex::PAD) , "..",
			    Hex(match->to, Hex::OMIT_PREFIX, Hex::PAD), "]");
		}
		log_event.matches.remove(match);
		destroy(_alloc, match);
	}
});
log("------------------------------");

		_finished(FAILED,    event, time_us);
	}
global_log_buf_base = global_log_buf;
global_log_buf_sz   = GLOBAL_LOG_BUF_SZ;
}


void Child::_finished(State                   state,
                      Event            const &event,
                      Genode::uint64_t const  time_us)
{
	if (_skip) {
		return; }

	_running = false;
	_state = state;

	Genode::uint64_t       time_ms  { time_us / 1000UL };
	Genode::uint64_t const time_sec { time_ms / 1000UL };
	time_ms = time_ms - time_sec * 1000UL;

	char name_padded[32];
	for (size_t i = 0; i < sizeof(name_padded) - 1; name_padded[i++] = ' ');
	name_padded[sizeof(name_padded) - 1] = '\0';
	memcpy(name_padded, _name.string(), min(_name.length() - 1, sizeof(name_padded) - 1));

	if (event.has_type(Event::Type::LOG)) {

		enum { MAX_EXPL_SZ = 32 };

		Log_event const &log_event = *static_cast<Log_event const *>(&event);

		char const *const pattern_base = log_event.base();
		size_t      const pattern_sz   = log_event.size();
		char const *const pattern_end  = pattern_base + pattern_sz;

		char const *expl_base = pattern_base;
		for (; expl_base < pattern_end && *expl_base < 33; expl_base++) ;

		char const *expl_end = expl_base;
		for (; expl_end < pattern_end && *expl_end > 31; expl_end++) ;

		size_t const expl_sz = min((size_t)(expl_end - expl_base), (size_t)MAX_EXPL_SZ);

		_conclusion = Conclusion {
			Cstring(name_padded), " ", _padded_state_name(), "  ",
			time_sec < 10 ? "  " : time_sec < 100 ? " " : "", time_sec, ".",
			time_ms  < 10 ? "00" : time_ms  < 100 ? "0" : "", time_ms,
			"  log \"", Cstring(expl_base, expl_sz),
			expl_sz < MAX_EXPL_SZ ? "" : " ...", "\"" };

	} else if (event.has_type(Event::Type::TIMEOUT)) {

		Timeout_event const &timeout_event = *static_cast<Timeout_event const *>(&event);

		_conclusion = Conclusion {
			Cstring(name_padded), " ", _padded_state_name(), "  ",
			time_sec < 10 ? "  " : time_sec < 100 ? " " : "", time_sec, ".",
			time_ms  < 10 ? "00" : time_ms  < 100 ? "0" : "", time_ms,
			"  timeout ", timeout_event.sec(), " sec" };

	} else {

		_conclusion = Conclusion { _padded_state_name(), "  ", _name };

	}
	log("");
	log(" ", _conclusion);
	_config_handler.submit();
}


/*******************
 ** Timeout_event **
 *******************/

Timeout_event::Timeout_event(Timer::Connection &timer,
                             Child             &child,
                             Xml_node    const &event)
:
	Event    { event, Type::TIMEOUT },
	_child   { child },
	_timer   { timer },
	_sec     { event.attribute_value("sec", (Genode::uint64_t)0) },
	_timeout { timer, *this, &Timeout_event::_handle_timeout }
{
	if (!_sec) {
		throw Invalid(); }

	_timeout.schedule(Microseconds(_sec * 1000 * 1000));
}


void Timeout_event::_handle_timeout(Duration)
{
	_child.event_occured(*this, _timer.curr_time().trunc_to_plain_us().value - _child.init_time_us);
}


/***************
 ** Log_event **
 ***************/

static char const *xml_content_base(Xml_node node)
{
	char const *result = nullptr;
	node.with_raw_content([&] (char const *start, size_t) { result = start; });
	return result;
}


static size_t xml_content_size(Xml_node node)
{
	size_t result = 0;
	node.with_raw_content([&] (char const *, size_t length) { result = length; });
	return result;
}


Log_event::Log_event(Xml_node const &xml)
:
	Event           { xml, Type::LOG },
	_base           { xml_content_base(xml) },
	_size           { xml_content_size(xml) },
	_remaining_base { xml_content_base(xml) },
	_remaining_end  { _remaining_base + xml_content_size(xml) },
	_reset_to       { xml_content_base(xml) }
{ }


/***********
 ** Event **
 ***********/

Event::Event(Xml_node const &node,
             Type            type)
:
	_meaning { node.attribute_value("meaning", Meaning_string()) },
	_type    { type }
{
	if (_meaning != Meaning_string("failed") &&
	    _meaning != Meaning_string("succeeded"))
	{
		throw Invalid();
	}
}


/************
 ** Result **
 ************/

void Result::print(Output &output) const
{
	Genode::print(output, "succeeded: ", succeeded, " failed: ", failed,
	              " skipped: ", skipped);
}
