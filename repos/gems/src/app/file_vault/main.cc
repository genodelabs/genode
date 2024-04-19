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
#include <base/buffered_output.h>
#include <os/buffered_xml.h>
#include <os/vfs.h>
#include <os/reporter.h>
#include <timer_session/connection.h>

/* tresor includes */
#include <tresor/types.h>

/* local includes */
#include <report_session_component.h>
#include <child_state.h>
#include <sandbox.h>
#include <child_exit_state.h>

namespace File_vault { class Main; }

class File_vault::Main
:
	private Sandbox::Local_service_base::Wakeup,
	private Sandbox::State_handler
{
	private:

		enum {
			STATE_STRING_CAPACITY = 64,
		};

		enum Version { INVALID, VERSION_21_05, VERSION_23_05 };

		enum class State
		{
			INVALID,
			SETUP_OBTAIN_PARAMETERS,
			SETUP_CREATE_TRESOR_IMAGE_FILE,
			SETUP_RUN_TRESOR_INIT_TRUST_ANCHOR,
			SETUP_RUN_TRESOR_INIT,
			SETUP_START_TRESOR_VFS,
			SETUP_FORMAT_TRESOR,
			SETUP_DETERMINE_CLIENT_FS_SIZE,
			UNLOCK_OBTAIN_PARAMETERS,
			UNLOCK_RUN_TRESOR_INIT_TRUST_ANCHOR,
			UNLOCK_START_TRESOR_VFS,
			UNLOCK_DETERMINE_CLIENT_FS_SIZE,
			CONTROLS,
			LOCK_ISSUE_DEINIT_REQUEST_AT_TRESOR,
			LOCK_WAIT_TILL_DEINIT_REQUEST_IS_DONE
		};

		enum class Resizing_state
		{
			INACTIVE,
			ADAPT_TRESOR_IMAGE_SIZE,
			WAIT_TILL_DEVICE_IS_READY,
			ISSUE_REQUEST_AT_DEVICE,
			IN_PROGRESS_AT_DEVICE,
			DETERMINE_CLIENT_FS_SIZE,
			RUN_RESIZE2FS,
		};

		enum class Rekeying_state
		{
			INACTIVE,
			WAIT_TILL_DEVICE_IS_READY,
			ISSUE_REQUEST_AT_DEVICE,
			IN_PROGRESS_AT_DEVICE,
		};

		using Report_service     = Sandbox::Local_service<Report::Session_component>;
		using Xml_report_handler = Report::Session_component::Xml_handler<Main>;
		using State_string       = String<STATE_STRING_CAPACITY>;

		Env                                   &_env;
		State                                  _state                              { State::INVALID };
		Heap                                   _heap                               { _env.ram(), _env.rm() };
		Timer::Connection                      _timer                              { _env };
		Attached_rom_dataspace                 _config_rom                         { _env, "config" };
		bool                                   _verbose_state                      { _config_rom.xml().attribute_value("verbose_state", false) };
		bool                                   _jent_avail                         { _config_rom.xml().attribute_value("jitterentropy_available", true) };
		Root_directory                         _vfs                                { _env, _heap, _config_rom.xml().sub_node("vfs") };
		Registry<Child_state>                  _children                           { };
		Child_state                            _mke2fs                             { _children, "mke2fs", Ram_quota { 32 * 1024 * 1024 }, Cap_quota { 500 } };
		Child_state                            _resize2fs                          { _children, "resize2fs", Ram_quota { 32 * 1024 * 1024 }, Cap_quota { 500 } };
		Child_state                            _tresor_vfs                         { _children, "tresor_vfs", "vfs", Ram_quota { 32 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _tresor_trust_anchor_vfs            { _children, "tresor_trust_anchor_vfs", "vfs", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _rump_vfs                           { _children, "rump_vfs", "vfs", Ram_quota { 32 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _sync_to_tresor_vfs_init            { _children, "sync_to_tresor_vfs_init", "file_vault-sync_to_tresor_vfs_init", Ram_quota { 8 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _truncate_file                      { _children, "truncate_file", "file_vault-truncate_file", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _tresor_vfs_block                   { _children, "vfs_block", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _fs_query                           { _children, "fs_query", Ram_quota { 2 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _image_fs_query                     { _children, "image_fs_query", "fs_query", Ram_quota { 2 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _client_fs_fs_query                 { _children, "client_fs_fs_query", "fs_query", Ram_quota { 2 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _tresor_init_trust_anchor           { _children, "tresor_init_trust_anchor", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 300 } };
		Child_state                            _tresor_init                        { _children, "tresor_init", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _resizing_fs_tool                   { _children, "resizing_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _resizing_fs_query                  { _children, "resizing_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _rekeying_fs_tool                   { _children, "rekeying_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _rekeying_fs_query                  { _children, "rekeying_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _lock_fs_tool                       { _children, "lock_fs_tool", "fs_tool", Ram_quota { 6 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _lock_fs_query                      { _children, "lock_fs_query", "fs_query", Ram_quota { 2 * 1024 * 1024 }, Cap_quota { 100 } };
		Xml_report_handler                     _fs_query_listing_handler           { *this, &Main::_handle_fs_query_listing };
		Xml_report_handler                     _image_fs_query_listing_handler     { *this, &Main::_handle_image_fs_query_listing };
		Xml_report_handler                     _client_fs_fs_query_listing_handler { *this, &Main::_handle_client_fs_fs_query_listing };
		Xml_report_handler                     _resizing_fs_query_listing_handler  { *this, &Main::_handle_resizing_fs_query_listing };
		Xml_report_handler                     _rekeying_fs_query_listing_handler  { *this, &Main::_handle_rekeying_fs_query_listing };
		Xml_report_handler                     _lock_fs_query_listing_handler      { *this, &Main::_handle_lock_fs_query_listing };
		Sandbox                                _sandbox                            { _env, *this };
		Report_service                         _report_service                     { _sandbox, *this };
		Signal_handler<Main>                   _state_handler                      { _env.ep(), *this, &Main::_handle_state };
		Resizing_state                         _resizing_state                     { Resizing_state::INACTIVE };
		Rekeying_state                         _rekeying_state                     { Rekeying_state::INACTIVE };
		Timer::One_shot_timeout<Main>          _unlock_retry_delay                 { _timer, *this, &Main::_handle_unlock_retry_delay };
		File_path                              _tresor_image_file_name             { "tresor.img" };
		Attached_rom_dataspace                 _ui_config_rom                      { _env, "ui_config" };
		Signal_handler<Main>                   _ui_config_handler                  { _env.ep(), *this, &Main::_handle_ui_config };
		Constructible<Ui_config>               _ui_config                          { };
		Ui_report                              _ui_report                          { };
		Expanding_reporter                     _ui_report_reporter                 { _env, "ui_report", "ui_report" };

		static bool _has_name(Xml_node  const &node,
		                      Node_name const &name)
		{
			return node.attribute_value("name", Node_name { }) == name;
		}

		size_t _min_journaling_buf_size() const
		{
			size_t result { _ui_config->client_fs_size >> 8 };
			if (result < MIN_CLIENT_FS_SIZE) {
				result = MIN_CLIENT_FS_SIZE;
			}
			return result;
		}

		bool _ui_setup_obtain_params_suitable() const
		{
			return
				_ui_config->client_fs_size >= MIN_CLIENT_FS_SIZE &&
				_ui_config->journaling_buf_size >= _min_journaling_buf_size() &&
				_ui_config->passphrase_long_enough();
		}

		template <typename FUNCTOR>
		static void _if_child_exited(Xml_node    const &sandbox_state,
		                             Child_state const &child_state,
		                             FUNCTOR     const &functor)
		{
			Child_exit_state const exit_state { sandbox_state, child_state.start_name() };

			if (!exit_state.exists()) {
				class Child_doesnt_exist { };
				throw Child_doesnt_exist { };
			}
			if (exit_state.exited()) {

				functor(exit_state.code());
			}
		}

		static bool _child_succeeded(Xml_node    const &sandbox_state,
		                             Child_state const &child_state);

		static size_t _child_nr_of_provided_sessions(Xml_node    const &sandbox_state,
		                                             Child_state const &child_state,
		                                             String<64>  const &service_name);

		void _handle_unlock_retry_delay(Duration);

		static State
		_state_and_version_from_string(State_string const &state_str,
		                               Version            &version);

		static State_string _state_to_string(State state);

		static State
		_state_and_version_from_fs_query_listing(Xml_node const &node,
		                                         Version        &version);

		void _write_to_state_file(State state);

		void _generate_sandbox_config(Xml_generator &xml) const;

		void _handle_fs_query_listing(Xml_node const &node);

		void _handle_image_fs_query_listing(Xml_node const &node);

		void _handle_client_fs_fs_query_listing(Xml_node const &node);

		void _handle_resizing_fs_query_listing(Xml_node const &node);

		void _handle_rekeying_fs_query_listing(Xml_node const &node);

		void _handle_lock_fs_query_listing(Xml_node const &node);

		void _handle_ui_config();

		void _handle_ui_config_and_report();

		void _handle_state();

		void _update_sandbox_config();

		void _adapt_to_version(Version version);

		Ui_report::State _reported_state() const;

		void _generate_ui_report()
		{
			_ui_report_reporter.generate([&] (Xml_generator &xml) { _ui_report.generate(xml); });
		}

		void _set_state(State state)
		{
			Ui_report::State old_reported_state { _reported_state() };
			_state = state;
			_ui_report.state = _reported_state();
			if (old_reported_state != _ui_report.state)
				_generate_ui_report();
		}

		bool _rekey_operation_pending() const;

		bool _extend_operation_pending() const;

		template <size_t N>
		static bool listing_file_starts_with(Xml_node  const &fs_query_listing,
		                                     char      const *file_name,
		                                     String<N> const &str)
		{
			bool result { false };
			bool done   { false };
			fs_query_listing.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {
				node_0.for_each_sub_node("file", [&] (Xml_node const &node_1) {
					if (done) {
						return;
					}
					if (node_1.attribute_value("name", String<16>()) == file_name) {
						node_1.with_raw_content([&] (char const *base, size_t size) {
							result = String<N> { Cstring {base, size} } == str;
							done = true;
						});
					}
				});
			});
			return result;
		}


		/***************************************************
		 ** Sandbox::Local_service_base::Wakeup interface **
		 ***************************************************/

		void wakeup_local_service() override;


		/****************************
		 ** Sandbox::State_handler **
		 ****************************/

		void handle_sandbox_state() override;


	public:

		Main(Env &env);
};

using namespace File_vault;


/**********************
 ** File_vault::Main **
 **********************/

void Main::_handle_ui_config()
{
	_ui_config_rom.update();
	_ui_config.construct(_ui_config_rom.xml());
	_handle_ui_config_and_report();
}


void Main::_update_sandbox_config()
{
	Buffered_xml const config { _heap, "config", [&] (Xml_generator &xml) {
		_generate_sandbox_config(xml); } };

	config.with_xml_node([&] (Xml_node const &config) {
		_sandbox.apply_config(config); });
}


void Main::_adapt_to_version(Version version)
{
	switch (version) {
	case VERSION_21_05:

		_tresor_image_file_name = "cbe.img";
		break;

	case VERSION_23_05:

		break;

	default:

		class Invalid_version { };
		throw Invalid_version { };
	}
}


Main::State Main::_state_and_version_from_string(State_string const &str,
                                                 Version            &version)
{
	version = VERSION_23_05;
	if (str == "invalid") { return State::INVALID; }
	if (str == "setup_obtain_parameters") { return State::SETUP_OBTAIN_PARAMETERS; }
	if (str == "setup_run_tresor_init_trust_anchor") { return State::SETUP_RUN_TRESOR_INIT_TRUST_ANCHOR; }
	if (str == "setup_create_tresor_image_file") { return State::SETUP_CREATE_TRESOR_IMAGE_FILE; }
	if (str == "setup_run_tresor_init") { return State::SETUP_RUN_TRESOR_INIT; }
	if (str == "setup_start_tresor_vfs") { return State::SETUP_START_TRESOR_VFS; }
	if (str == "setup_format_tresor") { return State::SETUP_FORMAT_TRESOR; }
	if (str == "setup_determine_client_fs_size") { return State::SETUP_DETERMINE_CLIENT_FS_SIZE; }
	if (str == "controls") { return State::CONTROLS; }
	if (str == "controls_root") { return State::CONTROLS; }
	if (str == "controls_dimensions") { return State::CONTROLS; }
	if (str == "controls_expand_client_fs") { return State::CONTROLS; }
	if (str == "controls_expand_snapshot_buf") { return State::CONTROLS; }
	if (str == "controls_security") { return State::CONTROLS; }
	if (str == "controls_security_block_encryption_key") { return State::CONTROLS; }
	if (str == "controls_security_master_key") { return State::CONTROLS; }
	if (str == "controls_security_user_passphrase") { return State::CONTROLS; }
	if (str == "unlock_obtain_parameters") { return State::UNLOCK_OBTAIN_PARAMETERS; }
	if (str == "unlock_run_tresor_init_trust_anchor") { return State::UNLOCK_RUN_TRESOR_INIT_TRUST_ANCHOR; }
	if (str == "unlock_start_tresor_vfs") { return State::UNLOCK_START_TRESOR_VFS; }
	if (str == "unlock_determine_client_fs_size") { return State::UNLOCK_DETERMINE_CLIENT_FS_SIZE; }
	if (str == "lock_issue_deinit_request_at_tresor") { return State::LOCK_ISSUE_DEINIT_REQUEST_AT_TRESOR; }
	if (str == "lock_wait_till_deinit_request_is_done") { return State::LOCK_WAIT_TILL_DEINIT_REQUEST_IS_DONE; }

	version = VERSION_21_05;
	if (str == "startup_obtain_parameters") { return State::UNLOCK_OBTAIN_PARAMETERS; }

	class Invalid_state_string { };
	throw Invalid_state_string { };
}


Main::State_string Main::_state_to_string(State state)
{
	switch (state) {
	case State::INVALID:                                return "invalid";
	case State::SETUP_OBTAIN_PARAMETERS:                return "setup_obtain_parameters";
	case State::SETUP_RUN_TRESOR_INIT_TRUST_ANCHOR:     return "setup_run_tresor_init_trust_anchor";
	case State::SETUP_CREATE_TRESOR_IMAGE_FILE:         return "setup_create_tresor_image_file";
	case State::SETUP_RUN_TRESOR_INIT:                  return "setup_run_tresor_init";
	case State::SETUP_START_TRESOR_VFS:                 return "setup_start_tresor_vfs";
	case State::SETUP_FORMAT_TRESOR:                    return "setup_format_tresor";
	case State::SETUP_DETERMINE_CLIENT_FS_SIZE:         return "setup_determine_client_fs_size";
	case State::CONTROLS:                               return "controls";
	case State::UNLOCK_OBTAIN_PARAMETERS:               return "unlock_obtain_parameters";
	case State::UNLOCK_RUN_TRESOR_INIT_TRUST_ANCHOR:    return "unlock_run_tresor_init_trust_anchor";
	case State::UNLOCK_START_TRESOR_VFS:                return "unlock_start_tresor_vfs";
	case State::UNLOCK_DETERMINE_CLIENT_FS_SIZE:        return "unlock_determine_client_fs_size";
	case State::LOCK_ISSUE_DEINIT_REQUEST_AT_TRESOR:    return "lock_issue_deinit_request_at_tresor";
	case State::LOCK_WAIT_TILL_DEINIT_REQUEST_IS_DONE:  return "lock_wait_till_deinit_request_is_done";
	}
	class Invalid_state { };
	throw Invalid_state { };
}


Ui_report::State Main::_reported_state() const
{
	switch (_state) {
	case State::INVALID:                                return Ui_report::INVALID;
	case State::SETUP_OBTAIN_PARAMETERS:                return Ui_report::UNINITIALIZED;
	case State::SETUP_CREATE_TRESOR_IMAGE_FILE:         return Ui_report::INITIALIZING;
	case State::SETUP_RUN_TRESOR_INIT_TRUST_ANCHOR:     return Ui_report::INITIALIZING;
	case State::SETUP_RUN_TRESOR_INIT:                  return Ui_report::INITIALIZING;
	case State::SETUP_START_TRESOR_VFS:                 return Ui_report::INITIALIZING;
	case State::SETUP_FORMAT_TRESOR:                    return Ui_report::INITIALIZING;
	case State::SETUP_DETERMINE_CLIENT_FS_SIZE:         return Ui_report::INITIALIZING;
	case State::CONTROLS:                               return Ui_report::UNLOCKED;
	case State::UNLOCK_OBTAIN_PARAMETERS:               return Ui_report::LOCKED;
	case State::UNLOCK_RUN_TRESOR_INIT_TRUST_ANCHOR:    return Ui_report::UNLOCKING;
	case State::UNLOCK_START_TRESOR_VFS:                return Ui_report::UNLOCKING;
	case State::UNLOCK_DETERMINE_CLIENT_FS_SIZE:        return Ui_report::UNLOCKING;
	case State::LOCK_ISSUE_DEINIT_REQUEST_AT_TRESOR:    return Ui_report::LOCKING;
	case State::LOCK_WAIT_TILL_DEINIT_REQUEST_IS_DONE:  return Ui_report::LOCKING;
	}
	class Invalid_state { };
	throw Invalid_state { };
}


Main::State
Main::_state_and_version_from_fs_query_listing(Xml_node const &node,
                                               Version        &version)
{
	State state { State::INVALID };
	bool state_file_found { false };
	node.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {
		node_0.with_optional_sub_node("file", [&] (Xml_node const &node_1) {
			if (_has_name(node_1, "state")) {
				state_file_found = true;
				state =_state_and_version_from_string(
					node_1.decoded_content<State_string>(), version);
				
			}
		});
	});
	if (!state_file_found)
		version = VERSION_23_05;

	return state;
}


void Main::_write_to_state_file(State state)
{
	bool write_error = false;
	try {
		New_file new_file(_vfs, Directory::Path("/tresor/file_vault/state"));
		auto write = [&] (char const *str)
		{
			switch (new_file.append(str, strlen(str))) {
			case New_file::Append_result::OK:

				break;

			case New_file::Append_result::WRITE_ERROR:

				write_error = true;
				break;
			}
		};
		Buffered_output<STATE_STRING_CAPACITY, decltype(write)> output(write);
		print(output, _state_to_string(state));
	}
	catch (New_file::Create_failed) {

		class Create_state_file_failed { };
		throw Create_state_file_failed { };
	}
	if (write_error) {

		class Write_state_file_failed { };
		throw Write_state_file_failed { };
	}
}


void Main::_handle_resizing_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::CONTROLS:

		switch (_resizing_state) {
		case Resizing_state::WAIT_TILL_DEVICE_IS_READY:

			if (listing_file_starts_with(node, "extend", String<10>("succeeded")) ||
			    listing_file_starts_with(node, "extend", String<5>("none"))) {

				_resizing_state = Resizing_state::ISSUE_REQUEST_AT_DEVICE;
				Signal_transmitter(_state_handler).submit();
			} else
				error("failed to extend: tresor not ready");

			break;

		case Resizing_state::IN_PROGRESS_AT_DEVICE:

			if (listing_file_starts_with(node, "extend", String<10>("succeeded"))) {
				_resizing_state = Resizing_state::DETERMINE_CLIENT_FS_SIZE;
				Signal_transmitter(_state_handler).submit();
			} else
				error("failed to extend: operation failed at tresor");
			break;

		default: break;
		}

	default: break;
	}
}


void Main::_handle_lock_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::LOCK_WAIT_TILL_DEINIT_REQUEST_IS_DONE:

		if (listing_file_starts_with(node, "deinitialize", String<10>("succeeded"))) {
			_set_state(State::UNLOCK_OBTAIN_PARAMETERS);
			Signal_transmitter(_state_handler).submit();
		} else
			error("failed to deinitialize: operation failed at tresor");
		break;

	default: break;
	}
}


void Main::_handle_rekeying_fs_query_listing(Xml_node const &node)
{
	bool generate_ui_report = false;
	switch (_state) {
	case State::CONTROLS:

		switch (_rekeying_state) {
		case Rekeying_state::WAIT_TILL_DEVICE_IS_READY:

			if (listing_file_starts_with(node, "rekey", String<10>("succeeded")) ||
			    listing_file_starts_with(node, "rekey", String<5>("none"))) {

				_rekeying_state = Rekeying_state::ISSUE_REQUEST_AT_DEVICE;
				Signal_transmitter(_state_handler).submit();

			} else
				error("failed to rekey: tresor not ready");

			break;

		case Rekeying_state::IN_PROGRESS_AT_DEVICE:

			if (listing_file_starts_with(node, "rekey", String<10>("succeeded"))) {

				_ui_report.rekey->finished = true;
				generate_ui_report = true;
				_rekeying_state = Rekeying_state::INACTIVE;
				Signal_transmitter(_state_handler).submit();

			} else
				error("failed to rekey: operation failed at tresor");

			break;

		default:

			break;
		}
		break;

	default:

		break;
	}
	if (generate_ui_report)
		_generate_ui_report();
}


void Main::_handle_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::INVALID:
	{
		Version version { INVALID };
		State const state { _state_and_version_from_fs_query_listing(node, version) };
		_adapt_to_version(version);

		switch (state) {
		case State::INVALID:

			_set_state(State::SETUP_OBTAIN_PARAMETERS);
			Signal_transmitter(_state_handler).submit();
			break;

		case State::UNLOCK_OBTAIN_PARAMETERS:

			_set_state(State::UNLOCK_OBTAIN_PARAMETERS);
			Signal_transmitter(_state_handler).submit();
			break;

		default:

			class Unexpected_state { };
			throw Unexpected_state { };
		}
		break;
	}
	default:

		break;
	}
}


void Main::_handle_client_fs_fs_query_listing(Xml_node const &node)
{
	bool generate_ui_report = false;
	switch (_state) {
	case State::SETUP_DETERMINE_CLIENT_FS_SIZE:
	case State::UNLOCK_DETERMINE_CLIENT_FS_SIZE:

		node.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("file", [&] (Xml_node const &node_1) {

				if (_has_name(node_1, "data")) {

					_ui_report.capacity = node_1.attribute_value("size", 0UL);
					generate_ui_report = true;
					_set_state(State::CONTROLS);
					Signal_transmitter(_state_handler).submit();
				}
			});
		});
		break;

	case State::CONTROLS:

		switch (_resizing_state) {
		case Resizing_state::DETERMINE_CLIENT_FS_SIZE:

			node.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {
				node_0.with_optional_sub_node("file", [&] (Xml_node const &node_1) {

					if (_has_name(node_1, "data")) {

						size_t const size {
							node_1.attribute_value("size", (size_t)0) };

						if (_ui_report.capacity != size) {

							_ui_report.capacity = size;
							generate_ui_report = true;
							_resizing_state = Resizing_state::RUN_RESIZE2FS;
							Signal_transmitter(_state_handler).submit();

						} else {

							_resizing_state = Resizing_state::INACTIVE;
							_ui_report.extend->finished = true;
							generate_ui_report = true;
							Signal_transmitter(_state_handler).submit();
						}
					}
				});
			});
			break;

		default:

			break;
		}

	default:

		break;
	}
	if (generate_ui_report)
		_generate_ui_report();
}


void Main::_handle_image_fs_query_listing(Xml_node const &node)
{
	bool generate_ui_report { false };

	switch (_state) {
	case State::CONTROLS:
	{
		size_t size { 0 };
		node.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("file", [&] (Xml_node const &node_1) {
				if (_has_name(node_1, _tresor_image_file_name)) {
					size = node_1.attribute_value("size", (size_t)0);
				}
			});
		});
		if (_ui_report.image_size != size) {
			_ui_report.image_size = size;
			generate_ui_report = true;
		}
		break;
	}
	default:

		break;
	}
	if (generate_ui_report)
		_generate_ui_report();
}


void Main::_handle_state()
{
	_update_sandbox_config();
	_handle_ui_config_and_report();
}


bool Main::_rekey_operation_pending() const
{
	if (!_ui_config->rekey.constructed())
		return false;

	if (!_ui_report.rekey.constructed())
		return true;

	return _ui_report.rekey->id.value != _ui_config->rekey->id.value;
}


bool Main::_extend_operation_pending() const
{
	if (!_ui_config->extend.constructed())
		return false;

	if (!_ui_report.extend.constructed())
		return true;

	return _ui_report.extend->id.value != _ui_config->extend->id.value;
}


void Main::_handle_ui_config_and_report()
{
	bool update_sandbox_config { false };
	bool generate_ui_report { false };

	switch (_state) {
	case State::SETUP_OBTAIN_PARAMETERS:

		if(_ui_setup_obtain_params_suitable()) {

			_set_state(State::SETUP_CREATE_TRESOR_IMAGE_FILE);
			update_sandbox_config = true;
		}
		break;

	case State::UNLOCK_OBTAIN_PARAMETERS:

		if (_ui_config->passphrase_long_enough()) {

			_set_state(State::UNLOCK_RUN_TRESOR_INIT_TRUST_ANCHOR);
			update_sandbox_config = true;
		}
		break;

	case State::CONTROLS:

		if (!_ui_config->passphrase_long_enough()) {

			_set_state(State::LOCK_ISSUE_DEINIT_REQUEST_AT_TRESOR);
			update_sandbox_config = true;
			break;
		}
		if (_rekeying_state == Rekeying_state::INACTIVE && _rekey_operation_pending()) {

			_ui_report.rekey.construct(_ui_config->rekey->id, false);
			_rekeying_state = Rekeying_state::WAIT_TILL_DEVICE_IS_READY;
			update_sandbox_config = true;
			generate_ui_report = true;
		}
		if (_resizing_state == Resizing_state::INACTIVE && _extend_operation_pending()) {
			_ui_report.extend.construct(_ui_config->extend->id, false);
			_resizing_state = Resizing_state::ADAPT_TRESOR_IMAGE_SIZE;
			update_sandbox_config = true;
		}
		break;

	default: break;
	}
	if (generate_ui_report)
		_generate_ui_report();
	if (update_sandbox_config)
		_update_sandbox_config();
}


Main::Main(Env &env) : _env(env)
{
	_ui_config_rom.sigh(_ui_config_handler);
	_update_sandbox_config();
	_handle_ui_config();
	_set_state(State::INVALID);
}


size_t
File_vault::Main::
_child_nr_of_provided_sessions(Xml_node    const &sandbox_state,
                               Child_state const &child_state,
                               String<64>  const &service_name)
{
	size_t result { 0 };
	sandbox_state.for_each_sub_node("child", [&] (Xml_node child) {

		if (child.attribute_value("name", String<128> { }) == child_state.start_name()) {

			child.with_optional_sub_node("provided", [&] (Xml_node const &provided) {
				provided.for_each_sub_node("session", [&] (Xml_node const &session) {

					if (session.attribute_value("service", String<64> { }) == service_name) {
						result++;
					}
				});
			});
		}
	});
	return result;
}


bool File_vault::Main::_child_succeeded(Xml_node    const &sandbox_state,
                                        Child_state const &child_state)
{
	Child_exit_state const exit_state { sandbox_state, child_state.start_name() };

	if (!exit_state.exists()) {
		class Child_doesnt_exist { };
		throw Child_doesnt_exist { };
	}
	if (exit_state.exited()) {

		if (exit_state.code() != 0) {
			class Child_exited_with_error { };
			throw Child_exited_with_error { };
		}
		return true;
	}
	return false;
}

void File_vault::Main::_handle_unlock_retry_delay(Duration)
{
	_set_state(State::UNLOCK_OBTAIN_PARAMETERS);
	_ui_config->passphrase = Passphrase();
	Signal_transmitter(_state_handler).submit();
}


void File_vault::Main::handle_sandbox_state()
{
	Buffered_xml sandbox_state {
		_heap, "sandbox_state",
		[&] (Xml_generator &xml) {
			_sandbox.generate_state_report(xml);
		}
	};
	bool update_sandbox { false };
	bool generate_ui_report { false };
	Number_of_clients nr_of_clients { 0 };
	sandbox_state.with_xml_node([&] (Xml_node const &sandbox_state) {

		switch (_state) {
		case State::SETUP_RUN_TRESOR_INIT_TRUST_ANCHOR:

			if (_child_succeeded(sandbox_state, _tresor_init_trust_anchor)) {
				_set_state(State::SETUP_RUN_TRESOR_INIT);
				update_sandbox = true;
			}
			break;

		case State::SETUP_CREATE_TRESOR_IMAGE_FILE:

			if (_child_succeeded(sandbox_state, _truncate_file)) {

				_set_state(State::SETUP_RUN_TRESOR_INIT_TRUST_ANCHOR);
				update_sandbox = true;
			}
			break;

		case State::UNLOCK_RUN_TRESOR_INIT_TRUST_ANCHOR:

			_if_child_exited(sandbox_state, _tresor_init_trust_anchor, [&] (int exit_code) {

				if (exit_code == 0) {

					_set_state(State::UNLOCK_START_TRESOR_VFS);
					update_sandbox = true;

				} else
					_unlock_retry_delay.schedule(Microseconds { 3000000 });
			});
			break;

		case State::SETUP_RUN_TRESOR_INIT:

			if (_child_succeeded(sandbox_state, _tresor_init)) {

				_set_state(State::SETUP_START_TRESOR_VFS);
				update_sandbox = true;
			}
			break;

		case State::SETUP_START_TRESOR_VFS:

			if (_child_succeeded(sandbox_state, _sync_to_tresor_vfs_init)) {

				_set_state(State::SETUP_FORMAT_TRESOR);
				update_sandbox = true;
			}
			break;

		case State::UNLOCK_START_TRESOR_VFS:

			if (_child_succeeded(sandbox_state, _sync_to_tresor_vfs_init)) {

				_set_state(State::UNLOCK_DETERMINE_CLIENT_FS_SIZE);
				update_sandbox = true;
			}
			break;

		case State::SETUP_FORMAT_TRESOR:

			if (_child_succeeded(sandbox_state, _mke2fs)) {

				_write_to_state_file(State::UNLOCK_OBTAIN_PARAMETERS);
				_set_state(State::SETUP_DETERMINE_CLIENT_FS_SIZE);
				update_sandbox = true;
			}
			break;

		case State::CONTROLS:

			if (_resizing_state == Resizing_state::INACTIVE ||
			    _ui_config->extend->tree != Ui_config::Extend::VIRTUAL_BLOCK_DEVICE)
			{
				nr_of_clients.value =
					_child_nr_of_provided_sessions(
						sandbox_state, _rump_vfs, "File_system");
			}
			switch (_resizing_state) {
			case Resizing_state::ADAPT_TRESOR_IMAGE_SIZE:

				if (_child_succeeded(sandbox_state, _truncate_file)) {

					_resizing_state = Resizing_state::WAIT_TILL_DEVICE_IS_READY;
					update_sandbox = true;
				}
				break;

			case Resizing_state::ISSUE_REQUEST_AT_DEVICE:

				if (_child_succeeded(sandbox_state, _resizing_fs_tool)) {

					_resizing_state = Resizing_state::IN_PROGRESS_AT_DEVICE;
					update_sandbox = true;
				}
				break;

			case Resizing_state::RUN_RESIZE2FS:

				if (_child_succeeded(sandbox_state, _resize2fs)) {

					_resizing_state = Resizing_state::INACTIVE;
					_ui_report.extend->finished = true;
					generate_ui_report = true;
					update_sandbox = true;
				}
				break;

			default:

				break;
			}

			switch (_rekeying_state) {
			case Rekeying_state::ISSUE_REQUEST_AT_DEVICE:

				if (_child_succeeded(sandbox_state, _rekeying_fs_tool)) {

					_rekeying_state = Rekeying_state::IN_PROGRESS_AT_DEVICE;
					update_sandbox = true;
				}
				break;

			default:

				break;
			}

			break;

		case State::LOCK_ISSUE_DEINIT_REQUEST_AT_TRESOR:

			if (_child_succeeded(sandbox_state, _lock_fs_tool)) {

				if (_ui_report.rekey.constructed()) {
					_ui_report.rekey->finished = true;
					_rekeying_state = Rekeying_state::INACTIVE;
					generate_ui_report = true;
				}
				if (_ui_report.extend.constructed()) {
					_ui_report.extend->finished = true;
					_resizing_state = Resizing_state::INACTIVE;
					generate_ui_report = true;
				}
				_set_state(State::LOCK_WAIT_TILL_DEINIT_REQUEST_IS_DONE);
				update_sandbox = true;
			}
			break;

		default:

			break;
		}
		sandbox_state.for_each_sub_node("child", [&] (Xml_node const &child_node) {
			_children.for_each([&] (Child_state &child_state) {
				if (child_state.apply_child_state_report(child_node)) {
					update_sandbox = true;
				}
			});
		});
	});
	if (_ui_report.num_clients.value != nr_of_clients.value) {
		_ui_report.num_clients.value = nr_of_clients.value;
		generate_ui_report = true;
	}
	if (update_sandbox)
		_update_sandbox_config();

	if (generate_ui_report)
		_generate_ui_report();
}


void File_vault::Main::wakeup_local_service()
{
	_report_service.for_each_requested_session([&] (Report_service::Request &request) {

		if (request.label == "fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} if (request.label == "image_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _image_fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} else if (request.label == "client_fs_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _client_fs_fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} else if (request.label == "resizing_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _resizing_fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} else if (request.label == "rekeying_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _rekeying_fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} else if (request.label == "lock_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _lock_fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} else {

			error("failed to deliver Report session with label ", request.label);
		}
	});

	_report_service.for_each_session_to_close([&] (Report::Session_component &session) {

		destroy(_heap, &session);
		return Report_service::Close_response::CLOSED;
	});
}


void File_vault::Main::_generate_sandbox_config(Xml_generator &xml) const
{
	switch (_state) {
	case State::INVALID:

		gen_parent_provides_and_report_nodes(xml);
		gen_fs_query_start_node(xml, _fs_query);
		break;

	case State::SETUP_OBTAIN_PARAMETERS:

		gen_parent_provides_and_report_nodes(xml);
		break;

	case State::UNLOCK_OBTAIN_PARAMETERS:

		gen_parent_provides_and_report_nodes(xml);
		break;

	case State::SETUP_RUN_TRESOR_INIT_TRUST_ANCHOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_init_trust_anchor_start_node(
			xml, _tresor_init_trust_anchor, _ui_config->passphrase);

		break;

	case State::UNLOCK_RUN_TRESOR_INIT_TRUST_ANCHOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_init_trust_anchor_start_node(
			xml, _tresor_init_trust_anchor, _ui_config->passphrase);

		break;

	case State::UNLOCK_START_TRESOR_VFS:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_vfs_start_node(xml, _tresor_vfs, _tresor_image_file_name);
		gen_sync_to_tresor_vfs_init_start_node(xml, _sync_to_tresor_vfs_init);
		break;

	case State::SETUP_DETERMINE_CLIENT_FS_SIZE:
	case State::UNLOCK_DETERMINE_CLIENT_FS_SIZE:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_vfs_start_node(xml, _tresor_vfs, _tresor_image_file_name);
		gen_client_fs_fs_query_start_node(xml, _client_fs_fs_query);
		break;

	case State::SETUP_CREATE_TRESOR_IMAGE_FILE:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_truncate_file_start_node(
			xml, _truncate_file,
			File_path { "/tresor/", _tresor_image_file_name }.string(),
			BLOCK_SIZE *
				tresor_num_blocks(
					NR_OF_SUPERBLOCK_SLOTS,
					TRESOR_VBD_MAX_LVL + 1,
					TRESOR_VBD_DEGREE,
					tresor_tree_num_leaves(_ui_config->client_fs_size),
					TRESOR_FREE_TREE_MAX_LVL + 1,
					TRESOR_FREE_TREE_DEGREE,
					tresor_tree_num_leaves(_ui_config->journaling_buf_size)));

		break;

	case State::SETUP_RUN_TRESOR_INIT:
	{
		Tresor::Superblock_configuration sb_config {
			Tree_configuration { TRESOR_VBD_MAX_LVL, TRESOR_VBD_DEGREE, tresor_tree_num_leaves(_ui_config->client_fs_size) },
			Tree_configuration { TRESOR_FREE_TREE_MAX_LVL, TRESOR_FREE_TREE_DEGREE, tresor_tree_num_leaves(_ui_config->journaling_buf_size) }
		};
		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_init_start_node(xml, _tresor_init, sb_config);
		break;
	}
	case State::SETUP_START_TRESOR_VFS:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_vfs_start_node(xml, _tresor_vfs, _tresor_image_file_name);
		gen_sync_to_tresor_vfs_init_start_node(xml, _sync_to_tresor_vfs_init);
		break;

	case State::SETUP_FORMAT_TRESOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_vfs_start_node(xml, _tresor_vfs, _tresor_image_file_name);
		gen_tresor_vfs_block_start_node(xml, _tresor_vfs_block);
		gen_mke2fs_start_node(xml, _mke2fs);
		break;

	case State::CONTROLS:
	{
		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_vfs_start_node(xml, _tresor_vfs, _tresor_image_file_name);
		gen_tresor_vfs_block_start_node(xml, _tresor_vfs_block);
		gen_image_fs_query_start_node(xml, _image_fs_query);

		switch(_resizing_state) {
		case Resizing_state::INACTIVE: break;
		case Resizing_state::ADAPT_TRESOR_IMAGE_SIZE:

			switch (_ui_config->extend->tree) {
			case Ui_config::Extend::VIRTUAL_BLOCK_DEVICE:
			{
				size_t const bytes { _ui_config->extend->num_bytes };
				size_t const effective_bytes { bytes - (bytes % BLOCK_SIZE) };
				gen_truncate_file_start_node(
					xml, _truncate_file,
					File_path { "/tresor/", _tresor_image_file_name }.string(),
					_ui_report.image_size + effective_bytes);

				break;
			}
			case Ui_config::Extend::FREE_TREE:
			{
				size_t const bytes { _ui_config->extend->num_bytes };
				size_t const effective_bytes { bytes - (bytes % BLOCK_SIZE) };
				gen_truncate_file_start_node(
					xml, _truncate_file,
					File_path { "/tresor/", _tresor_image_file_name }.string(),
					_ui_report.image_size + effective_bytes);

				break;
			} }
			break;

		case Resizing_state::WAIT_TILL_DEVICE_IS_READY:

			gen_resizing_fs_query_start_node(xml, _resizing_fs_query);
			break;

		case Resizing_state::ISSUE_REQUEST_AT_DEVICE:

			switch (_ui_config->extend->tree) {
			case Ui_config::Extend::VIRTUAL_BLOCK_DEVICE:

				gen_resizing_fs_tool_start_node(
					xml, _resizing_fs_tool, "vbd",
					_ui_config->extend->num_bytes / BLOCK_SIZE);
				break;

			case Ui_config::Extend::FREE_TREE:

				gen_resizing_fs_tool_start_node(
					xml, _resizing_fs_tool, "ft",
					_ui_config->extend->num_bytes / BLOCK_SIZE);
				break;
			}
			break;

		case Resizing_state::IN_PROGRESS_AT_DEVICE:

			gen_resizing_fs_query_start_node(xml, _resizing_fs_query);
			break;

		case Resizing_state::DETERMINE_CLIENT_FS_SIZE:

			gen_client_fs_fs_query_start_node(xml, _client_fs_fs_query);
			break;

		case Resizing_state::RUN_RESIZE2FS:

			gen_resize2fs_start_node(xml, _resize2fs);
			break;
		}

		switch(_rekeying_state) {
		case Rekeying_state::INACTIVE:

			break;

		case Rekeying_state::WAIT_TILL_DEVICE_IS_READY:

			gen_rekeying_fs_query_start_node(xml, _rekeying_fs_query);
			break;

		case Rekeying_state::ISSUE_REQUEST_AT_DEVICE:

			gen_rekeying_fs_tool_start_node(xml, _rekeying_fs_tool);
			break;

		case Rekeying_state::IN_PROGRESS_AT_DEVICE:

			gen_rekeying_fs_query_start_node(xml, _rekeying_fs_query);
			break;
		}
		if (_resizing_state == Resizing_state::INACTIVE ||
		    _ui_config->extend->tree != Ui_config::Extend::VIRTUAL_BLOCK_DEVICE) {

			gen_policy_for_child_service(xml, "File_system", _rump_vfs);
			gen_rump_vfs_start_node(xml, _rump_vfs);
		}
		break;
	}
	case State::LOCK_ISSUE_DEINIT_REQUEST_AT_TRESOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_policy_for_child_service(xml, "File_system", _rump_vfs);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_vfs_start_node(xml, _tresor_vfs, _tresor_image_file_name);
		gen_tresor_vfs_block_start_node(xml, _tresor_vfs_block);
		gen_lock_fs_tool_start_node(xml, _lock_fs_tool);
		break;

	case State::LOCK_WAIT_TILL_DEINIT_REQUEST_IS_DONE:

		gen_parent_provides_and_report_nodes(xml);
		gen_policy_for_child_service(xml, "File_system", _rump_vfs);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_vfs_start_node(xml, _tresor_vfs, _tresor_image_file_name);
		gen_tresor_vfs_block_start_node(xml, _tresor_vfs_block);
		gen_lock_fs_query_start_node(xml, _lock_fs_query);
		break;
	}
}


void Component::construct(Genode::Env &env) { static File_vault::Main main { env }; }
