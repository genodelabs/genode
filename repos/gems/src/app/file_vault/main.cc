/*
 * \brief  Graphical front end for controlling Tresor devices
 * \author Martin Stein
 * \author Norman Feske
 * \date   2021-02-24
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <base/buffered_output.h>
#include <base/session_object.h>
#include <os/buffered_xml.h>
#include <os/vfs.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <report_session/report_session.h>

/* local includes */
#include <child_state.h>
#include <sandbox.h>

using namespace File_vault;

using Service_name = String<64>;

static bool has_name(Xml_node const &node, Node_name const &name) {
	return node.attribute_value("name", Node_name { }) == name; }


static void with_child(Xml_node const &init_state, Child_state const &child_state, auto const &fn)
{
	bool done = false;
	init_state.for_each_sub_node("child", [&] (Xml_node const &child) {
		if (!done && has_name(child, child_state.start_name())) {
			fn(child);
			done = true; } });
}


static void with_exit_code(Child_state const &child_state, Xml_node const &init_state, auto const &fn)
{
	bool exists = false, exited = false;
	int code = 0;
	with_child(init_state, child_state, [&] (Xml_node const &child) {
		exists = true;
		if (child.has_attribute("exited")) {
			exited = true;
			code = child.attribute_value("exited", (int)0L); } });

	ASSERT(exists);
	if (exited)
		fn(code);
}


static bool child_succeeded(Child_state const &child, Xml_node const &sandbox)
{
	bool result = false;
	with_exit_code(child, sandbox, [&] (int code) {
		ASSERT(!code);
		result = true; });
	return result;
}


static void with_file(Xml_node const &fs_query_listing, File_path const &name, auto const &fn)
{
	bool done = false;
	fs_query_listing.with_optional_sub_node("dir", [&] (Xml_node const &dir) {
		dir.for_each_sub_node("file", [&] (Xml_node const &file) {
			if (!done && has_name(file, name)) {
				fn(file);
				done = true; } }); });
}


static bool file_starts_with(Xml_node const &fs_query_listing, File_path const &file_name, auto const &str)
{
	bool result = false;
	with_file(fs_query_listing, file_name, [&] (Xml_node const &file) {
		file.with_raw_content([&] (char const *base, size_t size) {
			result = decltype(str)(Cstring(base, size)) == str; }); });
	return result;
}


struct Report_session_component : Session_object<Report::Session>
{
	struct Handler_base : Interface, Genode::Noncopyable
	{
		virtual void handle_report(char const *, size_t) = 0;
	};

	template <typename T>
	struct Xml_handler : Handler_base
	{
		T &obj;
		void (T::*member) (Xml_node const &);

		Xml_handler(T &obj, void (T::*member)(Xml_node const &)) : obj(obj), member(member) { }

		void handle_report(char const *start, size_t length) override
		{
			(obj.*member)(Xml_node(start, length));
		}
	};

	Attached_ram_dataspace ds;
	Handler_base &handler;

	Dataspace_capability dataspace() override { return ds.cap(); }

	void submit(size_t length) override
	{
		handler.handle_report(ds.local_addr<char const>(), min(ds.size(), length));
	}

	void response_sigh(Signal_context_capability) override { }

	size_t obtain_response() override { return 0; }

	template <typename... ARGS>
	Report_session_component(Env &env, Handler_base &handler, Entrypoint &ep, Resources const &res, ARGS &&... args)
	:
		Session_object(ep, res, args...),
		ds(env.ram(), env.rm(), res.ram_quota.value), handler(handler)
	{ }
};


struct Main : Sandbox::Local_service_base::Wakeup, Sandbox::State_handler
{
	using Report_service = Sandbox::Local_service<Report_session_component>;
	using Report_xml_handler = Report_session_component::Xml_handler<Main>;

	static constexpr char const *DEPRECATED_IMAGE_NAME = "cbe.img";

	enum State {
		INVALID, UNINITIALIZED, SETUP_CREATE_IMAGE, SETUP_INIT_TRUST_ANCHOR, SETUP_TRESOR_INIT,
		SETUP_START_TRESOR, SETUP_MKE2FS, SETUP_READ_FS_SIZE, LOCKED, UNLOCK_INIT_TRUST_ANCHOR,
		UNLOCK_START_TRESOR, UNLOCK_READ_FS_SIZE, UNLOCKED, LOCK_PENDING, START_LOCKING, LOCKING };

	struct Extend { enum State {
		INACTIVE, ADAPT_IMAGE_SIZE, WAIT_FOR_TRESOR, SEND_REQUEST, REQUEST_IN_PROGRESS, READ_FS_SIZE, RESIZE2FS }; };

	struct Rekey { enum State { INACTIVE, WAIT_FOR_TRESOR, SEND_REQUEST, REQUEST_IN_PROGRESS }; };

	Env &env;
	State state { INVALID };
	Heap heap { env.ram(), env.rm() };
	Timer::Connection timer { env };
	Attached_rom_dataspace config_rom { env, "config" };
	bool jent_avail { config_rom.xml().attribute_value("jitterentropy_available", true) };
	Root_directory vfs { env, heap, config_rom.xml().sub_node("vfs") };
	Registry<Child_state> children { };
	Child_state mke2fs { children, "mke2fs", Ram_quota { 32 * 1024 * 1024 }, Cap_quota { 300 } };
	Child_state resize2fs { children, "resize2fs", Ram_quota { 32 * 1024 * 1024 }, Cap_quota { 300 } };
	Child_state tresor_vfs { children, "tresor_vfs", "vfs", Ram_quota { 32 * 1024 * 1024 }, Cap_quota { 200 } };
	Child_state tresor_trust_anchor_vfs { children, "tresor_trust_anchor_vfs", "vfs", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 200 } };
	Child_state rump_vfs { children, "rump_vfs", "vfs", Ram_quota { 32 * 1024 * 1024 }, Cap_quota { 200 } };
	Child_state sync_to_tresor_vfs_init { children, "sync_to_tresor_vfs_init", "file_vault-sync_to_tresor_vfs_init", Ram_quota { 8 * 1024 * 1024 }, Cap_quota { 100 } };
	Child_state truncate_file { children, "truncate_file", "file_vault-truncate_file", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
	Child_state tresor_vfs_block { children, "vfs_block", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
	Child_state image_fs_query { children, "image_fs_query", "fs_query", Ram_quota { 2 * 1024 * 1024 }, Cap_quota { 100 } };
	Child_state client_fs_query { children, "client_fs_query", "fs_query", Ram_quota { 2 * 1024 * 1024 }, Cap_quota { 100 } };
	Child_state tresor_init_trust_anchor { children, "tresor_init_trust_anchor", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 300 } };
	Child_state tresor_init { children, "tresor_init", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 200 } };
	Child_state extend_fs_tool { children, "extend_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
	Child_state extend_fs_query { children, "extend_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
	Child_state rekey_fs_tool { children, "rekey_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
	Child_state rekey_fs_query { children, "rekey_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
	Child_state lock_fs_tool { children, "lock_fs_tool", "fs_tool", Ram_quota { 6 * 1024 * 1024 }, Cap_quota { 200 } };
	Child_state lock_fs_query { children, "lock_fs_query", "fs_query", Ram_quota { 2 * 1024 * 1024 }, Cap_quota { 100 } };
	Report_xml_handler image_fs_query_listing_handler { *this, &Main::handle_image_fs_query_listing };
	Report_xml_handler client_fs_query_listing_handler { *this, &Main::handle_client_fs_query_listing };
	Report_xml_handler extend_fs_query_listing_handler { *this, &Main::handle_extend_fs_query_listing };
	Report_xml_handler rekey_fs_query_listing_handler { *this, &Main::handle_rekey_fs_query_listing };
	Report_xml_handler lock_fs_query_listing_handler { *this, &Main::handle_lock_fs_query_listing };
	Sandbox sandbox { env, *this };
	Report_service report_service { sandbox, *this };
	Signal_handler<Main> state_handler { env.ep(), *this, &Main::handle_state };
	Extend::State extend_state { Extend::INACTIVE };
	Rekey::State rekey_state { Rekey::INACTIVE };
	Timer::One_shot_timeout<Main> unlock_retry_delay { timer, *this, &Main::handle_unlock_retry_delay };
	File_path image_name { "tresor.img" };
	Attached_rom_dataspace ui_config_rom { env, "ui_config" };
	Signal_handler<Main> ui_config_handler { env.ep(), *this, &Main::handle_ui_config_rom };
	Reconstructible<Ui_config> ui_config { };
	Ui_report ui_report { };
	Expanding_reporter ui_report_reporter { env, "ui_report", "ui_report" };

	static Ui_report::State state_to_ui_report_state(State);

	void handle_unlock_retry_delay(Duration)
	{
		set_state(LOCKED);
		ui_config->passphrase = Passphrase();
		Signal_transmitter(state_handler).submit();
	}

	void handle_sandbox_state_extend_and_rekey(Xml_node const &, bool &, bool &);

	void generate_sandbox_config(Xml_generator &) const;

	void gen_sandbox_cfg_extend_and_rekey(Xml_generator &) const;

	void handle_image_fs_query_listing(Xml_node const &);

	void handle_client_fs_query_listing(Xml_node const &);

	void handle_extend_fs_query_listing(Xml_node const &);

	void handle_rekey_fs_query_listing(Xml_node const &);

	void handle_lock_fs_query_listing(Xml_node const &node)
	{
		if (state != LOCKING)
			return;
		if (file_starts_with(node, "deinitialize", String<10>("succeeded"))) {
			set_state(LOCKED);
			Signal_transmitter(state_handler).submit();
		} else
			error("failed to deinitialize: operation failed at tresor");
	}

	void handle_ui_config_rom()
	{
		ui_config_rom.update();
		ui_config.construct(ui_config_rom.xml());
		handle_ui_config();
	}

	void handle_ui_config();

	void handle_state()
	{
		update_sandbox_config();
		handle_ui_config();
	}

	void update_sandbox_config()
	{
		Buffered_xml config { heap, "config", [&] (Xml_generator &xml) { generate_sandbox_config(xml); } };
		config.with_xml_node([&] (Xml_node const &config) { sandbox.apply_config(config); });
	}

	void generate_ui_report()
	{
		ui_report_reporter.generate([&] (Xml_generator &xml) { ui_report.generate(xml); });
	}

	void set_state(State new_state)
	{
		state = new_state;
		if (ui_report.state != state_to_ui_report_state(state)) {
			ui_report.state = state_to_ui_report_state(state);
			generate_ui_report();
		}
	}

	bool rekey_operation_pending() const
	{
		if (!ui_config->rekey.constructed())
			return false;

		if (!ui_report.rekey.constructed())
			return true;

		return ui_report.rekey->id.value != ui_config->rekey->id.value;
	}

	bool extend_operation_pending() const
	{
		if (!ui_config->extend.constructed())
			return false;

		if (!ui_report.extend.constructed())
			return true;

		return ui_report.extend->id.value != ui_config->extend->id.value;
	}

	void wakeup_local_service() override;

	void handle_sandbox_state() override;

	Main(Env &env) : env(env)
	{
		ui_config_rom.sigh(ui_config_handler);
		handle_ui_config_rom();
		update_sandbox_config();
	}
};


Ui_report::State Main::state_to_ui_report_state(State state)
{
	switch (state) {
	case INVALID: return Ui_report::INVALID;
	case UNINITIALIZED: return Ui_report::UNINITIALIZED;
	case SETUP_CREATE_IMAGE:
	case SETUP_INIT_TRUST_ANCHOR:
	case SETUP_TRESOR_INIT:
	case SETUP_START_TRESOR:
	case SETUP_MKE2FS:
	case SETUP_READ_FS_SIZE: return Ui_report::INITIALIZING;
	case UNLOCKED: return Ui_report::UNLOCKED;
	case LOCKED: return Ui_report::LOCKED;
	case UNLOCK_INIT_TRUST_ANCHOR:
	case UNLOCK_START_TRESOR:
	case UNLOCK_READ_FS_SIZE: return Ui_report::UNLOCKING;
	case LOCK_PENDING:
	case START_LOCKING:
	case LOCKING: return Ui_report::LOCKING;
	}
	ASSERT_NEVER_REACHED;
}


void Main::handle_extend_fs_query_listing(Xml_node const &node)
{
	if (state != UNLOCKED && state != LOCK_PENDING)
		return;

	switch (extend_state) {
	case Extend::WAIT_FOR_TRESOR:

		if (file_starts_with(node, "extend", String<10>("succeeded")) ||
		    file_starts_with(node, "extend", String<5>("none"))) {

			extend_state = Extend::SEND_REQUEST;
			Signal_transmitter(state_handler).submit();
		} else
			error("failed to extend: tresor not ready");
		break;

	case Extend::REQUEST_IN_PROGRESS:

		if (file_starts_with(node, "extend", String<10>("succeeded"))) {
			extend_state = Extend::READ_FS_SIZE;
			Signal_transmitter(state_handler).submit();
		} else
			error("failed to extend: operation failed at tresor");
		break;

	default: break;
	}
}


void Main::handle_rekey_fs_query_listing(Xml_node const &node)
{
	if (state != UNLOCKED && state != LOCK_PENDING)
		return;

	bool ui_report_changed = false;
	switch (rekey_state) {
	case Rekey::WAIT_FOR_TRESOR:

		if (file_starts_with(node, "rekey", String<10>("succeeded")) ||
		    file_starts_with(node, "rekey", String<5>("none"))) {

			rekey_state = Rekey::SEND_REQUEST;
			Signal_transmitter(state_handler).submit();
		} else
			error("failed to rekey: tresor not ready");
		break;

	case Rekey::REQUEST_IN_PROGRESS:

		if (file_starts_with(node, "rekey", String<10>("succeeded"))) {
			ui_report.rekey->finished = true;
			ui_report_changed = true;
			rekey_state = Rekey::INACTIVE;
			Signal_transmitter(state_handler).submit();
		} else
			error("failed to rekey: operation failed at tresor");
		break;

	default: break;
	}
	if (ui_report_changed)
		generate_ui_report();
}


void Main::handle_client_fs_query_listing(Xml_node const &listing)
{
	bool ui_report_changed = false;
	switch (state) {
	case SETUP_READ_FS_SIZE:
	case UNLOCK_READ_FS_SIZE:

		with_file(listing, "data", [&] (Xml_node const &file) {
			ui_report.capacity = file.attribute_value("size", 0UL);
			ui_report_changed = true;
			set_state(UNLOCKED);
			Signal_transmitter(state_handler).submit();
		});
		break;

	case UNLOCKED:
	case LOCK_PENDING:

		if (extend_state != Extend::READ_FS_SIZE)
			break;

		with_file(listing, "data", [&] (Xml_node const &file) {
			size_t const size { file.attribute_value("size", (size_t)0) };
			if (ui_report.capacity != size) {
				ui_report.capacity = size;
				ui_report_changed = true;
				extend_state = Extend::RESIZE2FS;
				Signal_transmitter(state_handler).submit();
			} else {
				extend_state = Extend::INACTIVE;
				ui_report.extend->finished = true;
				ui_report_changed = true;
				Signal_transmitter(state_handler).submit();
			}
		});
		break;

	default: break;
	}
	if (ui_report_changed)
		generate_ui_report();
}


void Main::handle_image_fs_query_listing(Xml_node const &listing)
{
	bool ui_report_changed { false };
	switch (state) {
	case INVALID:
	{
		bool image_exists = false;
		with_file(listing, image_name, [&] (auto) { image_exists = true; });
		if (!image_exists)
			with_file(listing, DEPRECATED_IMAGE_NAME, [&] (auto) {
				image_name = DEPRECATED_IMAGE_NAME;
				image_exists = true; });

		set_state(image_exists ? LOCKED : UNINITIALIZED);
		break;
	}
	case UNLOCKED:
	case LOCK_PENDING:
	{
		size_t size { 0 };
		with_file(listing, image_name, [&] (Xml_node const &file) { size = file.attribute_value("size", 0UL); });
		if (ui_report.image_size != size) {
			ui_report.image_size = size;
			ui_report_changed = true;
		}
		break;
	}
	default: break;
	}
	if (ui_report_changed)
		generate_ui_report();
}


void Main::handle_ui_config()
{
	bool update_sandbox_cfg { false };
	bool ui_report_changed { false };
	switch (state) {
	case UNINITIALIZED:

		if(ui_config->client_fs_size >= MIN_CLIENT_FS_SIZE &&
		   ui_config->journaling_buf_size >= min_journal_buf(ui_config->client_fs_size) &&
		   ui_config->passphrase_long_enough()) {

			set_state(SETUP_CREATE_IMAGE);
			update_sandbox_cfg = true;
		}
		break;

	case LOCKED:

		if (ui_config->passphrase_long_enough()) {
			set_state(UNLOCK_INIT_TRUST_ANCHOR);
			update_sandbox_cfg = true;
		}
		break;

	case UNLOCKED:

		if (rekey_state == Rekey::INACTIVE && rekey_operation_pending()) {
			ui_report.rekey.construct(ui_config->rekey->id, false);
			rekey_state = Rekey::WAIT_FOR_TRESOR;
			update_sandbox_cfg = true;
			ui_report_changed = true;
		}
		if (extend_state == Extend::INACTIVE && extend_operation_pending()) {
			ui_report.extend.construct(ui_config->extend->id, false);
			extend_state = Extend::ADAPT_IMAGE_SIZE;
			update_sandbox_cfg = true;
		}
		if (!ui_config->passphrase_long_enough()) {
			set_state(LOCK_PENDING);
			update_sandbox_cfg = true;
			break;
		}
		break;

	default: break;
	}
	if (ui_report_changed)
		generate_ui_report();
	if (update_sandbox_cfg)
		update_sandbox_config();
}

void Main::handle_sandbox_state_extend_and_rekey(Xml_node const &sandbox_state, bool &update_sandbox_cfg, bool &ui_report_changed)
{
	switch (extend_state) {
	case Extend::ADAPT_IMAGE_SIZE:

		if (child_succeeded(truncate_file, sandbox_state)) {
			extend_state = Extend::WAIT_FOR_TRESOR;
			update_sandbox_cfg = true;
		}
		break;

	case Extend::SEND_REQUEST:

		if (child_succeeded(extend_fs_tool, sandbox_state)) {
			extend_state = Extend::REQUEST_IN_PROGRESS;
			update_sandbox_cfg = true;
		}
		break;

	case Extend::RESIZE2FS:

		if (child_succeeded(resize2fs, sandbox_state)) {
			extend_state = Extend::INACTIVE;
			ui_report.extend->finished = true;
			ui_report_changed = true;
			update_sandbox_cfg = true;
		}
		break;

	default: break;
	}
	switch (rekey_state) {
	case Rekey::SEND_REQUEST:

		if (child_succeeded(rekey_fs_tool, sandbox_state)) {
			rekey_state = Rekey::REQUEST_IN_PROGRESS;
			update_sandbox_cfg = true;
		}
		break;

	default: break;
	}
}


void Main::handle_sandbox_state()
{
	Buffered_xml sandbox_state(heap, "sandbox_state", [&] (Xml_generator &xml) { sandbox.generate_state_report(xml); });
	bool update_sandbox_cfg { false };
	bool ui_report_changed { false };
	Number_of_clients num_clients { 0 };
	sandbox_state.with_xml_node([&] (Xml_node const &sandbox_state) {

		switch (state) {
		case SETUP_INIT_TRUST_ANCHOR:

			if (child_succeeded(tresor_init_trust_anchor, sandbox_state)) {
				set_state(SETUP_TRESOR_INIT);
				update_sandbox_cfg = true;
			}
			break;

		case SETUP_CREATE_IMAGE:

			if (child_succeeded(truncate_file, sandbox_state)) {
				set_state(SETUP_INIT_TRUST_ANCHOR);
				update_sandbox_cfg = true;
			}
			break;

		case UNLOCK_INIT_TRUST_ANCHOR:
		{
			with_exit_code(tresor_init_trust_anchor, sandbox_state, [&] (int code) {
				if (code)
					unlock_retry_delay.schedule(Microseconds { 3000000 });
				else {
					set_state(UNLOCK_START_TRESOR);
					update_sandbox_cfg = true;
				}
			});
			break;
		}
		case SETUP_TRESOR_INIT:

			if (child_succeeded(tresor_init, sandbox_state)) {
				set_state(SETUP_START_TRESOR);
				update_sandbox_cfg = true;
			}
			break;

		case SETUP_START_TRESOR:

			if (child_succeeded(sync_to_tresor_vfs_init, sandbox_state)) {
				set_state(SETUP_MKE2FS);
				update_sandbox_cfg = true;
			}
			break;

		case UNLOCK_START_TRESOR:

			if (child_succeeded(sync_to_tresor_vfs_init, sandbox_state)) {
				set_state(UNLOCK_READ_FS_SIZE);
				update_sandbox_cfg = true;
			}
			break;

		case SETUP_MKE2FS:

			if (child_succeeded(mke2fs, sandbox_state)) {
				set_state(SETUP_READ_FS_SIZE);
				update_sandbox_cfg = true;
			}
			break;

		case UNLOCKED:

			handle_sandbox_state_extend_and_rekey(sandbox_state, update_sandbox_cfg, ui_report_changed);
			with_child(sandbox_state, rump_vfs, [&] (Xml_node const &child) {
				child.with_optional_sub_node("provided", [&] (Xml_node const &provided) {
					provided.for_each_sub_node("session", [&] (Xml_node const &session) {
						if (session.attribute_value("service", Service_name()) == "File_system")
							num_clients.value++; }); }); });
			break;

		case LOCK_PENDING:

			handle_sandbox_state_extend_and_rekey(sandbox_state, update_sandbox_cfg, ui_report_changed);
			if (extend_state == Extend::INACTIVE && rekey_state == Rekey::INACTIVE) {
				set_state(START_LOCKING);
				update_sandbox_cfg = true;
			}
			break;

		case START_LOCKING:

			if (child_succeeded(lock_fs_tool, sandbox_state)) {
				set_state(LOCKING);
				update_sandbox_cfg = true;
			}
			break;

		default: break;
		}
		sandbox_state.for_each_sub_node("child", [&] (Xml_node const &child) {
			children.for_each([&] (Child_state &child_state) {
				if (child_state.apply_child_state_report(child))
					update_sandbox_cfg = true; }); });
	});
	if (ui_report.num_clients.value != num_clients.value) {
		ui_report.num_clients.value = num_clients.value;
		ui_report_changed = true;
	}
	if (update_sandbox_cfg)
		update_sandbox_config();
	if (ui_report_changed)
		generate_ui_report();
}


void Main::wakeup_local_service()
{
	report_service.for_each_requested_session([&] (Report_service::Request &req) {
		auto deliver_session = [&] (Report_xml_handler &handler) {
			req.deliver_session(*new (heap)
				Report_session_component(env, handler, env.ep(), req.resources, "", req.diag));
		};
		if (req.label == "image_fs_query -> listing") deliver_session(image_fs_query_listing_handler);
		else if (req.label == "client_fs_query -> listing") deliver_session(client_fs_query_listing_handler);
		else if (req.label == "extend_fs_query -> listing") deliver_session(extend_fs_query_listing_handler);
		else if (req.label == "rekey_fs_query -> listing") deliver_session(rekey_fs_query_listing_handler);
		else if (req.label == "lock_fs_query -> listing") deliver_session(lock_fs_query_listing_handler);
		else error("failed to deliver Report session with label ", req.label);
	});
	report_service.for_each_session_to_close([&] (Report_session_component &session) {
		destroy(heap, &session);
		return Report_service::Close_response::CLOSED;
	});
}


void Main::gen_sandbox_cfg_extend_and_rekey(Xml_generator &xml) const
{
	switch (extend_state) {
	case Extend::INACTIVE: break;
	case Extend::ADAPT_IMAGE_SIZE:

		switch (ui_config->extend->tree) {
		case Ui_config::Extend::VIRTUAL_BLOCK_DEVICE:
		{
			size_t bytes = ui_config->extend->num_bytes;
			size_t effective_bytes = bytes - (bytes % BLOCK_SIZE);
			gen_truncate_file_start_node(
				xml, truncate_file, File_path("/tresor/", image_name).string(), ui_report.image_size + effective_bytes);
			break;
		}
		case Ui_config::Extend::FREE_TREE:
		{
			size_t bytes = ui_config->extend->num_bytes;
			size_t effective_bytes = bytes - (bytes % BLOCK_SIZE);
			gen_truncate_file_start_node(
				xml, truncate_file, File_path("/tresor/", image_name).string(), ui_report.image_size + effective_bytes);
			break;
		} }
		break;

	case Extend::WAIT_FOR_TRESOR: gen_extend_fs_query_start_node(xml, extend_fs_query); break;
	case Extend::SEND_REQUEST:

		switch (ui_config->extend->tree) {
		case Ui_config::Extend::VIRTUAL_BLOCK_DEVICE:
			gen_extend_fs_tool_start_node(xml, extend_fs_tool, "vbd", ui_config->extend->num_bytes / BLOCK_SIZE);
			break;
		case Ui_config::Extend::FREE_TREE:
			gen_extend_fs_tool_start_node(xml, extend_fs_tool, "ft", ui_config->extend->num_bytes / BLOCK_SIZE);
			break;
		}
		break;

	case Extend::REQUEST_IN_PROGRESS: gen_extend_fs_query_start_node(xml, extend_fs_query); break;
	case Extend::READ_FS_SIZE:

		gen_client_fs_query_start_node(xml, client_fs_query);
		break;

	case Extend::RESIZE2FS: gen_resize2fs_start_node(xml, resize2fs); break;
	}
	switch(rekey_state) {
	case Rekey::INACTIVE: break;
	case Rekey::WAIT_FOR_TRESOR: gen_rekey_fs_query_start_node(xml, rekey_fs_query); break;
	case Rekey::SEND_REQUEST: gen_rekey_fs_tool_start_node(xml, rekey_fs_tool); break;
	case Rekey::REQUEST_IN_PROGRESS: gen_rekey_fs_query_start_node(xml, rekey_fs_query); break;
	}
}


void Main::generate_sandbox_config(Xml_generator &xml) const
{
	switch (state) {
	case INVALID:

		gen_parent_provides_and_report_nodes(xml);
		gen_image_fs_query_start_node(xml, image_fs_query);
		break;

	case UNINITIALIZED: gen_parent_provides_and_report_nodes(xml); break;
	case LOCKED: gen_parent_provides_and_report_nodes(xml); break;
	case SETUP_INIT_TRUST_ANCHOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, tresor_trust_anchor_vfs, jent_avail);
		gen_tresor_init_trust_anchor_start_node(xml, tresor_init_trust_anchor, ui_config->passphrase);
		break;

	case UNLOCK_INIT_TRUST_ANCHOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, tresor_trust_anchor_vfs, jent_avail);
		gen_tresor_init_trust_anchor_start_node(
			xml, tresor_init_trust_anchor, ui_config->passphrase);
		break;

	case UNLOCK_START_TRESOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, tresor_trust_anchor_vfs, jent_avail);
		gen_tresor_vfs_start_node(xml, tresor_vfs, image_name);
		gen_sync_to_tresor_vfs_init_start_node(xml, sync_to_tresor_vfs_init);
		break;

	case SETUP_READ_FS_SIZE:
	case UNLOCK_READ_FS_SIZE:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, tresor_trust_anchor_vfs, jent_avail);
		gen_tresor_vfs_start_node(xml, tresor_vfs, image_name);
		gen_client_fs_query_start_node(xml, client_fs_query);
		break;

	case SETUP_CREATE_IMAGE:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, tresor_trust_anchor_vfs, jent_avail);
		gen_truncate_file_start_node(
			xml, truncate_file, File_path("/tresor/", image_name).string(),
			BLOCK_SIZE * tresor_num_blocks(
				NR_OF_SUPERBLOCK_SLOTS,
				TRESOR_VBD_MAX_LVL + 1, TRESOR_VBD_DEGREE, tresor_tree_num_leaves(ui_config->client_fs_size),
				TRESOR_FREE_TREE_MAX_LVL + 1, TRESOR_FREE_TREE_DEGREE, tresor_tree_num_leaves(ui_config->journaling_buf_size)));
		break;

	case SETUP_TRESOR_INIT:
	{
		Tresor::Superblock_configuration sb_config {
			Tree_configuration(TRESOR_VBD_MAX_LVL, TRESOR_VBD_DEGREE, tresor_tree_num_leaves(ui_config->client_fs_size)),
			Tree_configuration(TRESOR_FREE_TREE_MAX_LVL, TRESOR_FREE_TREE_DEGREE, tresor_tree_num_leaves(ui_config->journaling_buf_size))
		};
		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, tresor_trust_anchor_vfs, jent_avail);
		gen_tresor_init_start_node(xml, tresor_init, sb_config);
		break;
	}
	case SETUP_START_TRESOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, tresor_trust_anchor_vfs, jent_avail);
		gen_tresor_vfs_start_node(xml, tresor_vfs, image_name);
		gen_sync_to_tresor_vfs_init_start_node(xml, sync_to_tresor_vfs_init);
		break;

	case SETUP_MKE2FS:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, tresor_trust_anchor_vfs, jent_avail);
		gen_tresor_vfs_start_node(xml, tresor_vfs, image_name);
		gen_tresor_vfs_block_start_node(xml, tresor_vfs_block);
		gen_mke2fs_start_node(xml, mke2fs);
		break;

	case UNLOCKED:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, tresor_trust_anchor_vfs, jent_avail);
		gen_tresor_vfs_start_node(xml, tresor_vfs, image_name);
		gen_tresor_vfs_block_start_node(xml, tresor_vfs_block);
		gen_image_fs_query_start_node(xml, image_fs_query);
		gen_sandbox_cfg_extend_and_rekey(xml);
		if (extend_state != Extend::INACTIVE && ui_config->extend->tree == Ui_config::Extend::VIRTUAL_BLOCK_DEVICE)
			break;

		gen_child_service_policy(xml, "File_system", rump_vfs);
		gen_rump_vfs_start_node(xml, rump_vfs);
		break;

	case LOCK_PENDING:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, tresor_trust_anchor_vfs, jent_avail);
		gen_tresor_vfs_start_node(xml, tresor_vfs, image_name);
		gen_tresor_vfs_block_start_node(xml, tresor_vfs_block);
		gen_sandbox_cfg_extend_and_rekey(xml);
		break;

	case START_LOCKING:

		gen_parent_provides_and_report_nodes(xml);
		gen_child_service_policy(xml, "File_system", rump_vfs);
		gen_tresor_trust_anchor_vfs_start_node(xml, tresor_trust_anchor_vfs, jent_avail);
		gen_tresor_vfs_start_node(xml, tresor_vfs, image_name);
		gen_tresor_vfs_block_start_node(xml, tresor_vfs_block);
		gen_lock_fs_tool_start_node(xml, lock_fs_tool);
		break;

	case LOCKING:

		gen_parent_provides_and_report_nodes(xml);
		gen_child_service_policy(xml, "File_system", rump_vfs);
		gen_tresor_trust_anchor_vfs_start_node(xml, tresor_trust_anchor_vfs, jent_avail);
		gen_tresor_vfs_start_node(xml, tresor_vfs, image_name);
		gen_tresor_vfs_block_start_node(xml, tresor_vfs_block);
		gen_lock_fs_query_start_node(xml, lock_fs_query);
		break;
	}
}

void Component::construct(Genode::Env &env) { static Main main { env }; }
