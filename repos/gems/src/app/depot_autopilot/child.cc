
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


/*********************
 ** Local utilities **
 *********************/

class Filter
{
	private:

		char const *const _keyword;
		size_t      const _keyword_size;
		char const *const _replacement;
		size_t      const _replacement_size;

	public:

		Filter(char const *const keyword,
		       char const *const replacement)
		:
			_keyword          { keyword },
			_keyword_size     { strlen(keyword) },
			_replacement      { replacement },
			_replacement_size { strlen(replacement) }
		{ }

		char const *keyword()          const { return _keyword; }
		size_t      keyword_size()     const { return _keyword_size; }
		char const *replacement()      const { return _replacement; }
		size_t      replacement_size() const { return _replacement_size; }
};


template <size_t NR_OF_FILTERS>
struct Filters
{
	Filter array[NR_OF_FILTERS];
};


template <typename FILTERS>
static Filter const *filter_to_apply(FILTERS const &filters,
                                     char    const *curr,
                                     char    const *end)
{
	for (Filter const &flt : filters.array) {
		char const *keyword_end { curr + flt.keyword_size() };
		if (keyword_end < curr || keyword_end > end) {
			continue;
		}
		if (memcmp(curr, flt.keyword(), flt.keyword_size()) != 0) {
			continue;
		}
		return &flt;
	}
	return 0;
}


static size_t sanitize_pattern(char *const base,
                               size_t      size)
{
	static Filters<5> pattern_filters
	{
		{
			{ "\x9", "" },
			{ "\xa", "" },
			{ "&lt;", "<" },
			{ "&amp;", "&" },
			{ "&#42;", "*" }
		}
	};
	struct Bad_filter : Exception { };
	char const *end { base + size };
	for (char *curr { base }; curr < end; ) {
		Filter const *flt { filter_to_apply(pattern_filters, curr, end) };
		if (!flt) {
			curr++;
			continue;
		}
		if (flt->replacement_size() > flt->keyword_size()) {
			throw Bad_filter();
		}
		memcpy(curr, flt->replacement(), flt->replacement_size());
		if (flt->replacement_size() < flt->keyword_size()) {
			char       *const replacement_end { curr + flt->replacement_size() };
			char const *const keyword_end     { curr + flt->keyword_size() };
			memmove(replacement_end, keyword_end, (size_t)(end - keyword_end));
		}
		curr += flt->replacement_size();
		size -= flt->keyword_size() - flt->replacement_size();
		end   = base + size;
	}
	return size;
}


static void forward_to_log(uint64_t    const sec,
                           uint64_t    const ms,
                           char const *const base,
                           char const *const end)
{
	log(sec, ".", ms < 10 ? "00" : ms < 100 ? "0" : "", ms, " ",
	    Cstring(base, end - base));
}


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


static void c_string_append(char       * &dst,
                            char   const *dst_end,
                            char   const *src,
                            size_t const  src_size)
{
	struct Bad_args : Exception { };
	char const *src_end = dst + src_size;
	if (src_end < dst ||
	    src_end > dst_end)
	{
		throw Bad_args();
	}
	memcpy(dst, src, src_size);
	dst += src_size;
}


static size_t sanitize_log(char                      *dst,
                           size_t              const  dst_sz,
                           Log_session::String const &str,
                           Session_label       const &label)
{
	static Filters<7> log_filters
	{
		{
			{ "\x9", "" },
			{ "\xa", "" },
			{ "\x1b[0m", "" },
			{ "\x1b[31m", "" },
			{ "\x1b[32m", "" },
			{ "\x1b[33m", "" },
			{ "\x1b[34m", "" }
		}
	};
	/* first, write the label prefix to the buffer */
	char const *const dst_base { dst };
	char const *const dst_end { dst + dst_sz };
	c_string_append(dst, dst_end, "[", 1);
	c_string_append(dst, dst_end, label.string(), label.length() - 1);
	c_string_append(dst, dst_end, "] ", 2);

	/* then, write the string but apply string filters */
	char const *src_curr   { str.string() };
	char const *src_copied { str.string() };
	char const *src_end    { str.string() + str.size() };
	for (; src_curr < src_end; ) {
		Filter const *const flt { filter_to_apply(log_filters, src_curr, src_end) };
		if (!flt) {
			src_curr++;
			continue;
		}
		c_string_append(
			dst, dst_end, src_copied, (size_t)(src_curr - src_copied));
		c_string_append(
			dst, dst_end, flt->replacement(), flt->replacement_size());

		src_curr += flt->keyword_size();
		src_copied = src_curr;
	}
	c_string_append(dst, dst_end, src_copied, (size_t)(src_curr - src_copied));

	/* return length of sanitized string (without the terminating zero) */
	return (size_t)(dst - dst_base - 1);
}


/***********
 ** Child **
 ***********/

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

	uint64_t max_timeout_sec = 0;
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
			_log_events.insert(new (_alloc) Log_event(_alloc, event));
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


size_t Child::log_session_write(Log_session::String const &str,
                                Session_label       const &label)
{
	if (_skip || finished()) {
		return 0;
	}
	/* max log string size + max label size + size of label framing "[ ]" */
	enum { LOG_BUF_SZ = Log_session::MAX_STRING_LEN + 160 + 3 };

	char               log_buf[LOG_BUF_SZ];
	size_t      const  log_len { sanitize_log(log_buf, LOG_BUF_SZ, str, label) };
	Log_event   const *matching_event { nullptr };

	_log.append(log_buf, log_len);
	_log_events.for_each([&] (Log_event &log_event) {
		if (matching_event) {
			return;
		}
		if (log_event.handle_log_update(_log)) {
			matching_event = &log_event;
		}
	});
	/* calculate timestamp */
	uint64_t const time_us  { _timer.curr_time().trunc_to_plain_us().value - init_time_us };
	uint64_t       time_ms  { time_us / 1000UL };
	uint64_t const time_sec { time_ms / 1000UL };
	time_ms = time_ms - time_sec * 1000UL;

	/* forward timestamp and sanitized string to back-end log session */
	forward_to_log(time_sec, time_ms, log_buf, log_buf + log_len);

	/* handle a matching log event */
	if (matching_event) {
		event_occured(*matching_event, time_us);
	}
	/* return length of original string */
	return strlen(str.string());
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


void Child::event_occured(Event    const &event,
                          uint64_t const  time_us)
{
	if (_skip) {
		return; }

	if (_state != UNFINISHED) {
		return; }

	if      (event.meaning() == "succeeded") { _finished(SUCCEEDED, event, time_us); }
	else if (event.meaning() == "failed"   ) { _finished(FAILED,    event, time_us); }
}


void Child::_finished(State           state,
                      Event    const &event,
                      uint64_t const  time_us)
{
	if (_skip) {
		return; }

	_running = false;
	_state = state;

	uint64_t       time_ms  { time_us / 1000UL };
	uint64_t const time_sec { time_ms / 1000UL };
	time_ms = time_ms - time_sec * 1000UL;

	char name_padded[32];
	for (size_t i = 0; i < sizeof(name_padded) - 1; name_padded[i++] = ' ');
	name_padded[sizeof(name_padded) - 1] = '\0';
	memcpy(name_padded, _name.string(), min(_name.length() - 1, sizeof(name_padded) - 1));

	if (event.has_type(Event::Type::LOG)) {

		_conclusion = Conclusion {
			Cstring(name_padded), " ", _padded_state_name(), "  ",
			time_sec < 10 ? "  " : time_sec < 100 ? " " : "", time_sec, ".",
			time_ms  < 10 ? "00" : time_ms  < 100 ? "0" : "", time_ms,
			"  log"
		};

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
	_sec     { event.attribute_value("sec", (uint64_t)0) },
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


/**********************
 ** Expanding_string **
 **********************/

Expanding_string::Chunk::Chunk(Genode::Allocator    &alloc,
                               char    const *const  str,
                               Genode::size_t const  str_size)
:
	_alloc { alloc },
	_base  { (char *)alloc.alloc(str_size) },
	_size  { str_size }
{
	memcpy(_base, str, str_size);
}


Expanding_string::Chunk::~Chunk()
{
	_alloc.free((void *)_base, _size);
}


Expanding_string::Expanding_string(Allocator &alloc)
:
	_alloc { alloc }
{
}


Expanding_string::~Expanding_string()
{
	_chunks.dequeue_all([&] (Chunk &str) {
		destroy(_alloc, &str);
	});
}


void Expanding_string::append(char   const *str,
                              size_t const  str_size)
{
	_chunks.enqueue(*new (_alloc) Chunk(_alloc, str, str_size));
}


/***************
 ** Log_event **
 ***************/

bool Log_event::handle_log_update(Expanding_string const &log_str)
{
	while (true) {

		/* determine current pattern chunk */
		Plain_string const *pattern_chunk        { nullptr };
		size_t              pattern_chunk_offset { _pattern_offset };
		_plain_strings.for_each([&] (Plain_string const &chunk) {
			if (pattern_chunk) {
				return;
			}
			if (pattern_chunk_offset >= chunk.size()) {
				pattern_chunk_offset -= chunk.size();
			} else {
				pattern_chunk = &chunk;
			}
		});
		if (!pattern_chunk) {
			return true;
		}
		char   const *pattern_chunk_curr { pattern_chunk->base() + pattern_chunk_offset };
		size_t const  pattern_chunk_left { pattern_chunk->size() - pattern_chunk_offset };

		/* determine current log chunk */
		Expanding_string::Chunk const *log_chunk        { nullptr };
		size_t                         log_chunk_offset { _log_offset };
		log_str.for_each_chunk([&] (Expanding_string::Chunk const &chunk) {
			if (log_chunk) {
				return;
			}
			if (log_chunk_offset >= chunk.size()) {
				log_chunk_offset -= chunk.size();
			} else {
				log_chunk = &chunk;
			}
		});
		if (!log_chunk) {
			return false;
		}
		char   const *log_chunk_curr { log_chunk->base() + log_chunk_offset };
		size_t const  log_chunk_left { log_chunk->size() - log_chunk_offset };

		/* compare log with pattern */
		size_t const cmp_size { min(log_chunk_left, pattern_chunk_left) };
		if (memcmp(pattern_chunk_curr, log_chunk_curr, cmp_size)) {
			_pattern_offset -= pattern_chunk_offset;
			_log_offset     -= pattern_chunk_offset;
			_log_offset     += 1;
		} else {
			_pattern_offset += cmp_size;
			_log_offset     += cmp_size;
		}
	}
}


Log_event::Plain_string::Plain_string(Allocator         &alloc,
                                      char const *const  base,
                                      size_t      const  size)
:
	_alloc      { alloc },
	_alloc_size { size },
	_base       { (char *)alloc.alloc(_alloc_size) },
	_size       { size }
{
	memcpy(_base, base, size);
	_size = sanitize_pattern(_base, _size);
}


Log_event::Plain_string::~Plain_string()
{
	_alloc.free((void *)_base, _alloc_size);
}


Log_event::~Log_event()
{
	_plain_strings.dequeue_all([&] (Plain_string &str) {
		destroy(_alloc, &str);
	});
}


Log_event::Log_event(Allocator      &alloc,
                     Xml_node const &xml)
:
	Event  { xml, Type::LOG },
	_alloc { alloc }
{
	char const *const base   { xml_content_base(xml) };
	size_t      const size   { xml_content_size(xml) };
	char const *const end    { base + size };
	char const *      copied { base };
	for (char const *curr { base }; curr < end; curr++) {
		if (*curr != '*') {
			continue;
		}
		_plain_strings.enqueue(
			*new (_alloc) Plain_string(
				_alloc, copied, (size_t)(curr - copied)));
		copied = curr + 1;
	}
	if (copied < end) {
		_plain_strings.enqueue(
			*new (_alloc) Plain_string(
				_alloc, copied, (size_t)(end - copied)));
	}
}


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
