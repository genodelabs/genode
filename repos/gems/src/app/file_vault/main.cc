/*
 * \brief  Graphical front end for controlling CBE devices
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
#include <base/session_object.h>
#include <base/attached_rom_dataspace.h>
#include <base/buffered_output.h>
#include <os/buffered_xml.h>
#include <os/dynamic_rom_session.h>
#include <os/vfs.h>
#include <os/reporter.h>
#include <timer_session/connection.h>

/* local includes */
#include <gui_session_component.h>
#include <report_session_component.h>
#include <child_state.h>
#include <sandbox.h>
#include <input.h>
#include <utf8.h>
#include <child_exit_state.h>
#include <menu_view_dialog.h>
#include <const_pointer.h>
#include <snapshot.h>
#include <capacity.h>

namespace File_vault {

	enum { SHOW_CONTROLS_SNAPSHOTS = 0 };
	enum { SHOW_CONTROLS_SECURITY_MASTER_KEY = 0 };
	enum { SHOW_CONTROLS_SECURITY_USER_PASSPHRASE = 0 };
	enum { RENAME_SNAPSHOT_BUFFER_JOURNALING_BUFFER = 1 };

	class Main;
}

class File_vault::Main
:
	private Sandbox::Local_service_base::Wakeup,
	private Sandbox::State_handler,
	private Gui::Input_event_handler,
	private Dynamic_rom_session::Xml_producer
{
	private:

		enum {
			MIN_CLIENT_FS_SIZE = 100 * 1024,
			STATE_STRING_CAPACITY = 64,
			CBE_BLOCK_SIZE = 4096,
			MAIN_FRAME_WIDTH = 46,
			CBE_VBD_TREE_NR_OF_LEVELS = 6,
			CBE_VBD_TREE_NR_OF_CHILDREN = 64,
			CBE_FREE_TREE_NR_OF_LEVELS = 6,
			CBE_FREE_TREE_NR_OF_CHILDREN = 64,
			CBE_NR_OF_SUPERBLOCKS = 8,
		};

		enum class State
		{
			INVALID,
			SETUP_OBTAIN_PARAMETERS,
			SETUP_CREATE_CBE_IMAGE_FILE,
			SETUP_RUN_CBE_INIT_TRUST_ANCHOR,
			SETUP_RUN_CBE_INIT,
			SETUP_START_CBE_VFS,
			SETUP_FORMAT_CBE,
			STARTUP_OBTAIN_PARAMETERS,
			STARTUP_RUN_CBE_INIT_TRUST_ANCHOR,
			STARTUP_START_CBE_VFS,
			STARTUP_DETERMINE_CLIENT_FS_SIZE,
			CONTROLS_ROOT,
			CONTROLS_SNAPSHOTS,
			CONTROLS_DIMENSIONS,
			CONTROLS_EXPAND_CLIENT_FS,
			CONTROLS_EXPAND_SNAPSHOT_BUF,
			CONTROLS_SECURITY,
			CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY,
			CONTROLS_SECURITY_MASTER_KEY,
			CONTROLS_SECURITY_USER_PASSPHRASE,
			SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE,
			SHUTDOWN_WAIT_TILL_DEINIT_REQUEST_IS_DONE
		};

		enum class Setup_obtain_params_hover
		{
			NONE,
			PASSPHRASE_INPUT,
			PASSPHRASE_SHOW_HIDE_BUTTON,
			CLIENT_FS_SIZE_INPUT,
			SNAPSHOT_BUFFER_SIZE_INPUT,
			START_BUTTON
		};

		enum class Setup_obtain_params_select
		{
			NONE,
			PASSPHRASE_INPUT,
			PASSPHRASE_SHOW_HIDE_BUTTON,
			CLIENT_FS_SIZE_INPUT,
			SNAPSHOT_BUFFER_SIZE_INPUT,
			START_BUTTON
		};

		enum class Controls_root_select
		{
			NONE,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_root_hover
		{
			NONE,
			SNAPSHOTS_EXPAND_BUTTON,
			DIMENSIONS_BUTTON,
			SECURITY_EXPAND_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_snapshots_select
		{
			NONE,
			SHUT_DOWN_BUTTON,
			CREATE_BUTTON,
			GENERATION_DISCARD_BUTTON,
		};

		enum class Controls_snapshots_hover
		{
			NONE,
			SHUT_DOWN_BUTTON,
			LEAVE_BUTTON,
			CREATE_BUTTON,
			GENERATION_LEAVE_BUTTON,
			GENERATION_DISCARD_BUTTON,
		};

		enum class Dimensions_select
		{
			NONE,
			EXPAND_CLIENT_FS_EXPAND_BUTTON,
			EXPAND_SNAP_BUF_EXPAND_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Dimensions_hover
		{
			NONE,
			LEAVE_BUTTON,
			EXPAND_CLIENT_FS_BUTTON,
			EXPAND_SNAPSHOT_BUF_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Expand_client_fs_select
		{
			NONE,
			CONTINGENT_INPUT,
			START_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Expand_client_fs_hover
		{
			NONE,
			LEAVE_BUTTON,
			CONTINGENT_INPUT,
			START_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Expand_snapshot_buf_select
		{
			NONE,
			CONTINGENT_INPUT,
			START_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Expand_snapshot_buf_hover
		{
			NONE,
			LEAVE_BUTTON,
			CONTINGENT_INPUT,
			START_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_block_encryption_key_select
		{
			NONE,
			REPLACE_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_block_encryption_key_hover
		{
			NONE,
			LEAVE_BUTTON,
			REPLACE_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_master_key_select
		{
			NONE,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_master_key_hover
		{
			NONE,
			LEAVE_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_user_passphrase_select
		{
			NONE,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_user_passphrase_hover
		{
			NONE,
			LEAVE_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_select
		{
			NONE,
			BLOCK_ENCRYPTION_KEY_EXPAND_BUTTON,
			MASTER_KEY_EXPAND_BUTTON,
			USER_PASSPHRASE_EXPAND_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_hover
		{
			NONE,
			SECURITY_EXPAND_BUTTON,
			BLOCK_ENCRYPTION_KEY_EXPAND_BUTTON,
			MASTER_KEY_EXPAND_BUTTON,
			USER_PASSPHRASE_EXPAND_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Resizing_type
		{
			NONE,
			EXPAND_CLIENT_FS,
			EXPAND_SNAPSHOT_BUF,
		};

		enum class Resizing_state
		{
			INACTIVE,
			ADAPT_CBE_IMAGE_SIZE,
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

		enum class Create_snapshot_state
		{
			INACTIVE,
			ISSUE_REQUEST_AT_DEVICE,
		};

		enum class Discard_snapshot_state
		{
			INACTIVE,
			ISSUE_REQUEST_AT_DEVICE,
		};

		using Report_service     = Sandbox::Local_service<Report::Session_component>;
		using Gui_service        = Sandbox::Local_service<Gui::Session_component>;
		using Rom_service        = Sandbox::Local_service<Dynamic_rom_session>;
		using Xml_report_handler = Report::Session_component::Xml_handler<Main>;
		using State_string       = String<STATE_STRING_CAPACITY>;
		using Snapshot_registry  = Registry<Registered<Snapshot>>;
		using Snapshot_pointer   = Const_pointer<Snapshot>;

		Env                                   &_env;
		State                                  _state                              { State::INVALID };
		Heap                                   _heap                               { _env.ram(), _env.rm() };
		Timer::Connection                      _timer                              { _env };
		Attached_rom_dataspace                 _config                             { _env, "config" };
		Root_directory                         _vfs                                { _env, _heap, _config.xml().sub_node("vfs") };
		Registry<Child_state>                  _children                           { };
		Child_state                            _menu_view                          { _children, "menu_view", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _mke2fs                             { _children, "mke2fs", Ram_quota { 100 * 1024 * 1024 }, Cap_quota { 500 } };
		Child_state                            _resize2fs                          { _children, "resize2fs", Ram_quota { 100 * 1024 * 1024 }, Cap_quota { 500 } };
		Child_state                            _cbe_vfs                            { _children, "cbe_vfs", "vfs", Ram_quota { 64 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _cbe_trust_anchor_vfs               { _children, "cbe_trust_anchor_vfs", "vfs", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _rump_vfs                           { _children, "rump_vfs", "vfs", Ram_quota { 16 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _sync_to_cbe_vfs_init               { _children, "sync_to_cbe_vfs_init", "file_vault-sync_to_cbe_vfs_init", Ram_quota { 8 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _truncate_file                      { _children, "truncate_file", "file_vault-truncate_file", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _cbe_vfs_block                      { _children, "vfs_block", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _fs_query                           { _children, "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _image_fs_query                     { _children, "image_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _client_fs_fs_query                 { _children, "client_fs_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _cbe_init_trust_anchor              { _children, "cbe_init_trust_anchor", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _cbe_image_vfs_block                { _children, "vfs_block", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _cbe_init                           { _children, "cbe_init", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _snapshots_fs_query                 { _children, "snapshots_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _resizing_fs_tool                   { _children, "resizing_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _resizing_fs_query                  { _children, "resizing_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _rekeying_fs_tool                   { _children, "rekeying_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _rekeying_fs_query                  { _children, "rekeying_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _shut_down_fs_tool                  { _children, "shut_down_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _shut_down_fs_query                 { _children, "shut_down_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _create_snap_fs_tool                { _children, "create_snap_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _discard_snap_fs_tool               { _children, "discard_snap_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Xml_report_handler                     _fs_query_listing_handler           { *this, &Main::_handle_fs_query_listing };
		Xml_report_handler                     _image_fs_query_listing_handler     { *this, &Main::_handle_image_fs_query_listing };
		Xml_report_handler                     _client_fs_fs_query_listing_handler { *this, &Main::_handle_client_fs_fs_query_listing };
		Xml_report_handler                     _snapshots_fs_query_listing_handler { *this, &Main::_handle_snapshots_fs_query_listing };
		Xml_report_handler                     _resizing_fs_query_listing_handler  { *this, &Main::_handle_resizing_fs_query_listing };
		Xml_report_handler                     _rekeying_fs_query_listing_handler  { *this, &Main::_handle_rekeying_fs_query_listing };
		Xml_report_handler                     _shut_down_fs_query_listing_handler { *this, &Main::_handle_shut_down_fs_query_listing };
		Sandbox                                _sandbox                            { _env, *this };
		Gui_service                            _gui_service                        { _sandbox, *this };
		Rom_service                            _rom_service                        { _sandbox, *this };
		Report_service                         _report_service                     { _sandbox, *this };
		Xml_report_handler                     _hover_handler                      { *this, &Main::_handle_hover };
		Constructible<Watch_handler<Main>>     _watch_handler                      { };
		Constructible<Expanding_reporter>      _clipboard_reporter                 { };
		Constructible<Attached_rom_dataspace>  _clipboard_rom                      { };
		bool                                   _initial_config                     { true };
		Signal_handler<Main>                   _config_handler                     { _env.ep(), *this, &Main::_handle_config };
		Signal_handler<Main>                   _state_handler                      { _env.ep(), *this, &Main::_handle_state };
		Dynamic_rom_session                    _dialog                             { _env.ep(), _env.ram(), _env.rm(), *this };
		Input_passphrase                       _setup_obtain_params_passphrase     { };
		Input_number_of_bytes                  _client_fs_size_input               { };
		Input_number_of_bytes                  _snapshot_buf_size_input            { };
		Setup_obtain_params_hover              _setup_obtain_params_hover          { Setup_obtain_params_hover::NONE };
		Setup_obtain_params_select             _setup_obtain_params_select         { Setup_obtain_params_select::PASSPHRASE_INPUT };
		Controls_root_hover                    _controls_root_hover                { Controls_root_hover::NONE };
		Controls_root_select                   _controls_root_select               { Controls_root_select::NONE };
		Controls_snapshots_hover               _controls_snapshots_hover           { Controls_snapshots_hover::NONE };
		Controls_snapshots_select              _controls_snapshots_select          { Controls_snapshots_select::NONE };
		Dimensions_hover                       _dimensions_hover                   { Dimensions_hover::NONE };
		Dimensions_select                      _dimensions_select                  { Dimensions_select::NONE };
		Expand_client_fs_hover                 _expand_client_fs_hover             { Expand_client_fs_hover::NONE };
		Expand_client_fs_select                _expand_client_fs_select            { Expand_client_fs_select::NONE };
		Expand_snapshot_buf_hover              _expand_snapshot_buf_hover          { Expand_snapshot_buf_hover::NONE };
		Expand_snapshot_buf_select             _expand_snapshot_buf_select         { Expand_snapshot_buf_select::NONE };
		Controls_security_hover                _controls_security_hover            { Controls_security_hover::NONE };
		Controls_security_select               _controls_security_select           { Controls_security_select::NONE };

		Controls_security_master_key_hover             _controls_security_master_key_hover            { Controls_security_master_key_hover::NONE };
		Controls_security_master_key_select            _controls_security_master_key_select           { Controls_security_master_key_select::NONE };
		Controls_security_block_encryption_key_hover   _controls_security_block_encryption_key_hover  { Controls_security_block_encryption_key_hover::NONE };
		Controls_security_block_encryption_key_select  _controls_security_block_encryption_key_select { Controls_security_block_encryption_key_select::NONE };
		Controls_security_user_passphrase_hover        _controls_security_user_passphrase_hover       { Controls_security_user_passphrase_hover::NONE };
		Controls_security_user_passphrase_select       _controls_security_user_passphrase_select      { Controls_security_user_passphrase_select::NONE };

		Resizing_state                         _resizing_state                     { Resizing_state::INACTIVE };
		Resizing_type                          _resizing_type                      { Resizing_type::NONE };
		Input_number_of_bytes                  _expand_client_fs_contingent        { };
		Input_number_of_bytes                  _expand_snapshot_buf_contingent     { };
		Rekeying_state                         _rekeying_state                     { Rekeying_state::INACTIVE };
		Create_snapshot_state                  _create_snap_state                  { Create_snapshot_state::INACTIVE };
		Discard_snapshot_state                 _discard_snap_state                 { Discard_snapshot_state::INACTIVE };
		Generation                             _discard_snap_gen                   { INVALID_GENERATION };
		Snapshot_registry                      _snapshots                          { };
		Snapshot_pointer                       _snapshots_hover                    { };
		Snapshot_pointer                       _snapshots_select                   { };
		bool                                   _snapshots_expanded                 { false };
		bool                                   _dimensions_expanded                { false };
		Timer::One_shot_timeout<Main>          _startup_retry_delay                { _timer, *this, &Main::_handle_startup_retry_delay };
		size_t                                 _cbe_image_size                     { 0 };
		size_t                                 _client_fs_size                     { 0 };
		bool                                   _nr_of_clients                      { 0 };

		static bool _has_name(Xml_node  const &node,
		                      Node_name const &name)
		{
			return node.attribute_value("name", Node_name { }) == name;
		}

		size_t _min_snapshot_buf_size() const
		{
			size_t result { _client_fs_size_input.value() >> 8 };
			if (result < MIN_CLIENT_FS_SIZE) {
				result = MIN_CLIENT_FS_SIZE;
			}
			return result;
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

		void _handle_startup_retry_delay(Duration);

		static State _state_from_string(State_string const &str);

		static State_string _state_to_string(State state);

		static State _state_from_fs_query_listing(Xml_node const &node);

		void _write_to_state_file(State state);

		void _generate_sandbox_config(Xml_generator &xml) const;

		void _handle_fs_query_listing(Xml_node const &node);

		void _handle_image_fs_query_listing(Xml_node const &node);

		void _handle_client_fs_fs_query_listing(Xml_node const &node);

		void _handle_snapshots_fs_query_listing(Xml_node const &node);

		void _handle_resizing_fs_query_listing(Xml_node const &node);

		void _handle_rekeying_fs_query_listing(Xml_node const &node);

		void _handle_shut_down_fs_query_listing(Xml_node const &node);

		void _handle_hover(Xml_node const &node);

		void _handle_config();

		void _handle_state();

		void _update_sandbox_config();

		static size_t _cbe_tree_nr_of_leaves(size_t payload_size);


		static size_t _tree_nr_of_blocks(size_t nr_of_lvls,
		                          size_t nr_of_children,
		                          size_t nr_of_leafs);

		size_t _cbe_size() const;

		static size_t _cbe_nr_of_blocks(size_t nr_of_superblocks,
		                                size_t nr_of_vbd_lvls,
		                                size_t nr_of_vbd_children,
		                                size_t nr_of_vbd_leafs,
		                                size_t nr_of_ft_lvls,
		                                size_t nr_of_ft_children,
		                                size_t nr_of_ft_leafs);

		static bool cbe_control_file_yields_state_idle(Xml_node const &fs_query_listing,
		                                               char     const *file_name);


		/***************************************************
		 ** Sandbox::Local_service_base::Wakeup interface **
		 ***************************************************/

		void wakeup_local_service() override;


		/****************************
		 ** Sandbox::State_handler **
		 ****************************/

		void handle_sandbox_state() override;


		/****************************************
		 ** Gui::Input_event_handler interface **
		 ****************************************/

		void handle_input_event(Input::Event const &event) override;


		/***************************************
		 ** Dynamic_rom_session::Xml_producer **
		 ***************************************/

		void produce_xml(Xml_generator &xml) override;

	public:

		Main(Env &env);
};

using namespace File_vault;


/**********************
 ** File_vault::Main **
 **********************/

void Main::_handle_config()
{
	_config.update();
	_initial_config = false;
}


bool Main::cbe_control_file_yields_state_idle(Xml_node const &fs_query_listing,
                                              char     const *file_name)
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
					result = String<5> { Cstring {base, size} } == "idle";
					done = true;
				});
			}
		});
	});
	return result;
}


void Main::_update_sandbox_config()
{
	Buffered_xml const config { _heap, "config", [&] (Xml_generator &xml) {
		_generate_sandbox_config(xml); } };

	config.with_xml_node([&] (Xml_node const &config) {
		_sandbox.apply_config(config); });
}


Main::State Main::_state_from_string(State_string const &str)
{
	if (str == "invalid") { return State::INVALID; }
	if (str == "setup_obtain_parameters") { return State::SETUP_OBTAIN_PARAMETERS; }
	if (str == "setup_run_cbe_init_trust_anchor") { return State::SETUP_RUN_CBE_INIT_TRUST_ANCHOR; }
	if (str == "setup_create_cbe_image_file") { return State::SETUP_CREATE_CBE_IMAGE_FILE; }
	if (str == "setup_run_cbe_init") { return State::SETUP_RUN_CBE_INIT; }
	if (str == "setup_start_cbe_vfs") { return State::SETUP_START_CBE_VFS; }
	if (str == "setup_format_cbe") { return State::SETUP_FORMAT_CBE; }
	if (str == "controls_root") { return State::CONTROLS_ROOT; }
	if (str == "controls_snapshots") { return State::CONTROLS_SNAPSHOTS; }
	if (str == "controls_dimensions") { return State::CONTROLS_DIMENSIONS; }
	if (str == "controls_expand_client_fs") { return State::CONTROLS_EXPAND_CLIENT_FS; }
	if (str == "controls_expand_snapshot_buf") { return State::CONTROLS_EXPAND_SNAPSHOT_BUF; }
	if (str == "controls_security") { return State::CONTROLS_SECURITY; }
	if (str == "controls_security_block_encryption_key") { return State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY; }
	if (str == "controls_security_master_key") { return State::CONTROLS_SECURITY_MASTER_KEY; }
	if (str == "controls_security_user_passphrase") { return State::CONTROLS_SECURITY_USER_PASSPHRASE; }
	if (str == "startup_obtain_parameters") { return State::STARTUP_OBTAIN_PARAMETERS; }
	if (str == "startup_run_cbe_init_trust_anchor") { return State::STARTUP_RUN_CBE_INIT_TRUST_ANCHOR; }
	if (str == "startup_start_cbe_vfs") { return State::STARTUP_START_CBE_VFS; }
	if (str == "startup_determine_client_fs_size") { return State::STARTUP_DETERMINE_CLIENT_FS_SIZE; }
	if (str == "shutdown_issue_deinit_request_at_cbe") { return State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE; }
	if (str == "shutdown_wait_till_deinit_request_is_done") { return State::SHUTDOWN_WAIT_TILL_DEINIT_REQUEST_IS_DONE; }
	class Invalid_state_string { };
	throw Invalid_state_string { };
}


Main::State_string Main::_state_to_string(State state)
{
	switch (state) {
	case State::INVALID:                                   return "invalid";
	case State::SETUP_OBTAIN_PARAMETERS:                   return "setup_obtain_parameters";
	case State::SETUP_RUN_CBE_INIT_TRUST_ANCHOR:           return "setup_run_cbe_init_trust_anchor";
	case State::SETUP_CREATE_CBE_IMAGE_FILE:               return "setup_create_cbe_image_file";
	case State::SETUP_RUN_CBE_INIT:                        return "setup_run_cbe_init";
	case State::SETUP_START_CBE_VFS:                       return "setup_start_cbe_vfs";
	case State::SETUP_FORMAT_CBE:                          return "setup_format_cbe";
	case State::CONTROLS_ROOT:                             return "controls_root";
	case State::CONTROLS_SNAPSHOTS:                        return "controls_snapshots";
	case State::CONTROLS_DIMENSIONS:                       return "controls_dimensions";
	case State::CONTROLS_EXPAND_CLIENT_FS:                 return "controls_expand_client_fs";
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:              return "controls_expand_snapshot_buf";
	case State::CONTROLS_SECURITY:                         return "controls_security";
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:    return "controls_security_block_encryption_key";
	case State::CONTROLS_SECURITY_MASTER_KEY:              return "controls_security_master_key";
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:         return "controls_security_user_passphrase";
	case State::STARTUP_OBTAIN_PARAMETERS:                 return "startup_obtain_parameters";
	case State::STARTUP_RUN_CBE_INIT_TRUST_ANCHOR:         return "startup_run_cbe_init_trust_anchor";
	case State::STARTUP_START_CBE_VFS:                     return "startup_start_cbe_vfs";
	case State::STARTUP_DETERMINE_CLIENT_FS_SIZE:          return "startup_determine_client_fs_size";
	case State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE:      return "shutdown_issue_deinit_request_at_cbe";
	case State::SHUTDOWN_WAIT_TILL_DEINIT_REQUEST_IS_DONE: return "shutdown_wait_till_deinit_request_is_done";
	}
	class Invalid_state { };
	throw Invalid_state { };
}


Main::State Main::_state_from_fs_query_listing(Xml_node const &node)
{
	State state { State::INVALID };
	node.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {
		node_0.with_optional_sub_node("file", [&] (Xml_node const &node_1) {
			if (_has_name(node_1, "state")) {
				state = _state_from_string(
					node_1.decoded_content<State_string>());
			}
		});
	});
	return state;
}


void Main::_write_to_state_file(State state)
{
	bool write_error = false;
	try {
		New_file new_file(_vfs, Directory::Path("/cbe/file_vault/state"));
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
	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_EXPAND_CLIENT_FS:
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:

		switch (_resizing_state) {
		case Resizing_state::WAIT_TILL_DEVICE_IS_READY:

			if (cbe_control_file_yields_state_idle(node, "extend")) {

				_resizing_state = Resizing_state::ISSUE_REQUEST_AT_DEVICE;
				Signal_transmitter(_state_handler).submit();
			}
			break;

		case Resizing_state::IN_PROGRESS_AT_DEVICE:

			if (cbe_control_file_yields_state_idle(node, "extend")) {

				switch (_resizing_type) {
				case Resizing_type::EXPAND_CLIENT_FS:

					_expand_client_fs_contingent = Input_number_of_bytes { };
					_expand_client_fs_select = Expand_client_fs_select::CONTINGENT_INPUT;
					break;

				case Resizing_type::EXPAND_SNAPSHOT_BUF:

					_expand_snapshot_buf_contingent = Input_number_of_bytes { };
					_expand_snapshot_buf_select = Expand_snapshot_buf_select::CONTINGENT_INPUT;
					break;

				default:

					class Unexpected_resizing_type { };
					throw Unexpected_resizing_type { };
					break;
				}
				_resizing_state = Resizing_state::DETERMINE_CLIENT_FS_SIZE;
				Signal_transmitter(_state_handler).submit();
			}
			break;

		default:

			break;
		}

	default:

		break;
	}
}


void Main::_handle_shut_down_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::SHUTDOWN_WAIT_TILL_DEINIT_REQUEST_IS_DONE:

		if (cbe_control_file_yields_state_idle(node, "deinitialize")) {

			_env.parent().exit(0);
		}
		break;

	default:

		break;
	}
}


void Main::_handle_rekeying_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_EXPAND_CLIENT_FS:
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:

		switch (_rekeying_state) {
		case Rekeying_state::WAIT_TILL_DEVICE_IS_READY:

			if (cbe_control_file_yields_state_idle(node, "rekey")) {

				_rekeying_state = Rekeying_state::ISSUE_REQUEST_AT_DEVICE;
				Signal_transmitter(_state_handler).submit();
			}
			break;

		case Rekeying_state::IN_PROGRESS_AT_DEVICE:

			if (cbe_control_file_yields_state_idle(node, "rekey")) {

				_rekeying_state = Rekeying_state::INACTIVE;
				Signal_transmitter(_state_handler).submit();
			}
			break;

		default:

			break;
		}
		break;

	default:

		break;
	}
}


void Main::_handle_snapshots_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_EXPAND_CLIENT_FS:
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:
	{
		bool update_dialog { false };
		node.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {

			_snapshots.for_each([&] (Snapshot const &snap)
			{
				bool snap_still_exists { false };
				node_0.for_each_sub_node("dir", [&] (Xml_node const &node_1) {

					if (snap_still_exists) {
						return;
					}
					Generation const generation {
						node_1.attribute_value(
							"name", Generation { INVALID_GENERATION }) };

					if (generation == INVALID_GENERATION) {
						warning("skipping snapshot file with invalid generation number");
						return;
					}
					if (generation == snap.generation()) {
						snap_still_exists = true;
						return;
					}
				});
				if (!snap_still_exists) {

					if (_snapshots_select.valid() &&
					    &_snapshots_select.object() == &snap) {

						_snapshots_select = Snapshot_pointer { };
					}
					if (_snapshots_hover.valid() &&
					    &_snapshots_hover.object() == &snap) {

						_snapshots_hover = Snapshot_pointer { };
					}
					destroy(&_heap, &const_cast<Snapshot&>(snap));
					update_dialog = true;
				}
			});

			node_0.for_each_sub_node("dir", [&] (Xml_node const &node_1) {

				Generation const generation {
					node_1.attribute_value(
						"name", Generation { INVALID_GENERATION }) };

				if (generation == INVALID_GENERATION) {
					warning("skipping snapshot file with invalid generation number");
					return;
				}
				bool snap_already_exists { false };
				_snapshots.for_each([&] (Snapshot const &snap)
				{
					if (generation == snap.generation()) {
						snap_already_exists = true;
					}
				});
				if (!snap_already_exists) {
					new (_heap) Registered<Snapshot>(_snapshots, generation);
					update_dialog = true;
				}
			});
		});
		if (update_dialog) {
			_dialog.trigger_update();
		}

		break;
	}
	default:

		break;
	}
}


void Main::_handle_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::INVALID:
	{
		State const state { _state_from_fs_query_listing(node) };
		switch (state) {
		case State::INVALID:

			_state = State::SETUP_OBTAIN_PARAMETERS;
			Signal_transmitter(_state_handler).submit();
			break;

		case State::STARTUP_OBTAIN_PARAMETERS:

			_state = State::STARTUP_OBTAIN_PARAMETERS;
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
	switch (_state) {
	case State::STARTUP_DETERMINE_CLIENT_FS_SIZE:

		node.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("file", [&] (Xml_node const &node_1) {

				if (_has_name(node_1, "data")) {

					_client_fs_size = node_1.attribute_value("size", (size_t)0);
					_state = State::CONTROLS_ROOT;
					Signal_transmitter(_state_handler).submit();
				}
			});
		});
		break;

	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_EXPAND_CLIENT_FS:
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:

		switch (_resizing_state) {
		case Resizing_state::DETERMINE_CLIENT_FS_SIZE:

			node.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {
				node_0.with_optional_sub_node("file", [&] (Xml_node const &node_1) {

					if (_has_name(node_1, "data")) {

						size_t const size {
							node_1.attribute_value("size", (size_t)0) };

						if (_client_fs_size != size) {

							_client_fs_size = size;
							_resizing_state = Resizing_state::RUN_RESIZE2FS;
							Signal_transmitter(_state_handler).submit();

						} else {

							_resizing_type = Resizing_type::NONE;
							_resizing_state = Resizing_state::INACTIVE;
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
}


void Main::_handle_image_fs_query_listing(Xml_node const &node)
{
	bool update_dialog { false };

	switch (_state) {
	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_EXPAND_CLIENT_FS:
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:
	{
		size_t size { 0 };
		node.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("file", [&] (Xml_node const &node_1) {
				if (_has_name(node_1, "cbe.img")) {
					size = node_1.attribute_value("size", (size_t)0);
				}
			});
		});
		if (_cbe_image_size != size) {

			_cbe_image_size = size;
			update_dialog = true;
		}
		break;
	}
	default:

		break;
	}
	if (update_dialog) {
		_dialog.trigger_update();
	}
}


void Main::_handle_state()
{
	_update_sandbox_config();
	_dialog.trigger_update();
}


Main::Main(Env &env)
:
	Xml_producer { "dialog" },
	_env         { env }
{
	_config.sigh(_config_handler);
	_handle_config();
	_update_sandbox_config();
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

void File_vault::Main::_handle_startup_retry_delay(Duration)
{
	_state = State::STARTUP_OBTAIN_PARAMETERS;
	_setup_obtain_params_passphrase = Input_passphrase { };
	_setup_obtain_params_select = Setup_obtain_params_select::PASSPHRASE_INPUT;
	_dialog.trigger_update();
	_update_sandbox_config();
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
	bool update_dialog { false };
	bool nr_of_clients { false };
	sandbox_state.with_xml_node([&] (Xml_node const &sandbox_state) {

		switch (_state) {
		case State::SETUP_RUN_CBE_INIT_TRUST_ANCHOR:

			if (_child_succeeded(sandbox_state, _cbe_init_trust_anchor)) {

				_state = State::SETUP_RUN_CBE_INIT;
				update_dialog = true;
				update_sandbox = true;
			}
			break;

		case State::SETUP_CREATE_CBE_IMAGE_FILE:

			if (_child_succeeded(sandbox_state, _truncate_file)) {

				_state = State::SETUP_RUN_CBE_INIT_TRUST_ANCHOR;
				update_dialog = true;
				update_sandbox = true;
			}
			break;

		case State::STARTUP_RUN_CBE_INIT_TRUST_ANCHOR:

			_if_child_exited(sandbox_state, _cbe_init_trust_anchor, [&] (int exit_code) {

				if (exit_code == 0) {

					_state = State::STARTUP_START_CBE_VFS;
					update_dialog = true;
					update_sandbox = true;

				} else {

					_startup_retry_delay.schedule(Microseconds { 3000000 });
				}
			});
			break;

		case State::SETUP_RUN_CBE_INIT:

			if (_child_succeeded(sandbox_state, _cbe_init)) {

				_state = State::SETUP_START_CBE_VFS;
				update_dialog = true;
				update_sandbox = true;
			}
			break;

		case State::SETUP_START_CBE_VFS:

			if (_child_succeeded(sandbox_state, _sync_to_cbe_vfs_init)) {

				_state = State::SETUP_FORMAT_CBE;
				update_dialog = true;
				update_sandbox = true;
			}
			break;

		case State::STARTUP_START_CBE_VFS:

			if (_child_succeeded(sandbox_state, _sync_to_cbe_vfs_init)) {

				_state = State::STARTUP_DETERMINE_CLIENT_FS_SIZE;
				update_dialog = true;
				update_sandbox = true;
			}
			break;

		case State::SETUP_FORMAT_CBE:

			if (_child_succeeded(sandbox_state, _mke2fs)) {

				_write_to_state_file(State::STARTUP_OBTAIN_PARAMETERS);
				_state = State::STARTUP_DETERMINE_CLIENT_FS_SIZE;
				update_dialog = true;
				update_sandbox = true;
			}
			break;

		case State::CONTROLS_ROOT:
		case State::CONTROLS_SNAPSHOTS:
		case State::CONTROLS_DIMENSIONS:
		case State::CONTROLS_EXPAND_CLIENT_FS:
		case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
		case State::CONTROLS_SECURITY:
		case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
		case State::CONTROLS_SECURITY_MASTER_KEY:
		case State::CONTROLS_SECURITY_USER_PASSPHRASE:

			if (_resizing_state == Resizing_state::INACTIVE ||
			    _resizing_type != Resizing_type::EXPAND_CLIENT_FS)
			{
				nr_of_clients =
					_child_nr_of_provided_sessions(
						sandbox_state, _rump_vfs, "File_system");
			}
			switch (_resizing_state) {
			case Resizing_state::ADAPT_CBE_IMAGE_SIZE:

				if (_child_succeeded(sandbox_state, _truncate_file)) {

					_resizing_state = Resizing_state::WAIT_TILL_DEVICE_IS_READY;
					update_dialog = true;
					update_sandbox = true;
				}
				break;

			case Resizing_state::ISSUE_REQUEST_AT_DEVICE:

				if (_child_succeeded(sandbox_state, _resizing_fs_tool)) {

					_resizing_state = Resizing_state::IN_PROGRESS_AT_DEVICE;
					update_dialog = true;
					update_sandbox = true;
				}
				break;

			case Resizing_state::RUN_RESIZE2FS:

				if (_child_succeeded(sandbox_state, _resize2fs)) {

					_resizing_type = Resizing_type::NONE;
					_resizing_state = Resizing_state::INACTIVE;
					update_dialog = true;
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
					update_dialog = true;
					update_sandbox = true;
				}
				break;

			default:

				break;
			}

			switch (_create_snap_state) {
			case Create_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

				if (_child_succeeded(sandbox_state, _create_snap_fs_tool)) {

					_create_snap_state = Create_snapshot_state::INACTIVE;
					update_dialog = true;
					update_sandbox = true;
				}
				break;

			default:

				break;
			}

			switch (_discard_snap_state) {
			case Discard_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

				if (_child_succeeded(sandbox_state, _discard_snap_fs_tool)) {

					_discard_snap_state = Discard_snapshot_state::INACTIVE;
					update_dialog = true;
					update_sandbox = true;
				}
				break;

			default:

				break;
			}

			break;

		case State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE:

			if (_child_succeeded(sandbox_state, _shut_down_fs_tool)) {

				_state = State::SHUTDOWN_WAIT_TILL_DEINIT_REQUEST_IS_DONE;
				update_dialog = true;
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
	if (_nr_of_clients != nr_of_clients) {

		_nr_of_clients = nr_of_clients;
		update_dialog = true;
	}
	if (update_dialog) {

		_dialog.trigger_update();
	}
	if (update_sandbox) {

		_update_sandbox_config();
	}
}


void File_vault::Main::produce_xml(Xml_generator &xml)
{
	switch (_state) {
	case State::INVALID:

		gen_info_frame(xml, "1", "Please wait...", MAIN_FRAME_WIDTH);
		break;

	case State::SETUP_OBTAIN_PARAMETERS:

		gen_main_frame(xml, "1", MAIN_FRAME_WIDTH, [&] (Xml_generator &xml) {

			bool gen_start_button { true };
			bool gen_image_size_info { true };
			gen_input_passphrase(
 				xml, MAIN_FRAME_WIDTH,
				_setup_obtain_params_passphrase,
				_setup_obtain_params_select == Setup_obtain_params_select::PASSPHRASE_INPUT,
				_setup_obtain_params_hover == Setup_obtain_params_hover::PASSPHRASE_SHOW_HIDE_BUTTON,
				_setup_obtain_params_select == Setup_obtain_params_select::PASSPHRASE_SHOW_HIDE_BUTTON);

			if (!_setup_obtain_params_passphrase.suitable()) {

				gen_start_button = false;
				gen_info_line(xml, "info_1", "Must have at least 8 characters");
			}
			gen_info_line(xml, "pad_1", "");
			gen_titled_text_input(
				xml, "Client FS Size", "Client FS size",
				_client_fs_size_input,
				_setup_obtain_params_select == Setup_obtain_params_select::CLIENT_FS_SIZE_INPUT);

			if (_client_fs_size_input.value() < MIN_CLIENT_FS_SIZE) {

				gen_image_size_info = false;
				gen_start_button = false;
				gen_info_line(xml, "info_2",
					String<128> {
						"Must be at least ",
						Number_of_bytes { MIN_CLIENT_FS_SIZE } }.string());

			}
			gen_info_line(xml, "pad_2", "");
			gen_titled_text_input(
				xml, "Snapshot Buffer Size",
				RENAME_SNAPSHOT_BUFFER_JOURNALING_BUFFER ?
					"Journaling buffer size" :
					"Snapshot buffer size",
				_snapshot_buf_size_input,
				_setup_obtain_params_select == Setup_obtain_params_select::SNAPSHOT_BUFFER_SIZE_INPUT);

			if (_snapshot_buf_size_input.value() < _min_snapshot_buf_size()) {

				gen_image_size_info = false;
				gen_start_button = false;
				gen_info_line(xml, "info_3",
					String<128> {
						"Must be at least ",
						Number_of_bytes { _min_snapshot_buf_size() } }.string());
			}
			if (gen_image_size_info) {

				gen_info_line(xml, "pad_3", "");
				gen_info_line(
					xml, "info_4",
					String<256> { "Image size: ", Capacity { _cbe_size() }}.string());
			}
			gen_info_line(xml, "pad_4", "");
			if (gen_start_button) {

				gen_action_button_at_bottom(
					xml, "ok", "Start",
					_setup_obtain_params_hover == Setup_obtain_params_hover::START_BUTTON,
					_setup_obtain_params_select == Setup_obtain_params_select::START_BUTTON);
			}
		});
		break;

	case State::STARTUP_OBTAIN_PARAMETERS:

		gen_main_frame(xml, "1", MAIN_FRAME_WIDTH, [&] (Xml_generator &xml) {

			bool gen_start_button { true };
			gen_input_passphrase(
				xml, MAIN_FRAME_WIDTH,
				_setup_obtain_params_passphrase,
				_setup_obtain_params_select == Setup_obtain_params_select::PASSPHRASE_INPUT,
				_setup_obtain_params_hover == Setup_obtain_params_hover::PASSPHRASE_SHOW_HIDE_BUTTON,
				_setup_obtain_params_select == Setup_obtain_params_select::PASSPHRASE_SHOW_HIDE_BUTTON);

			if (!_setup_obtain_params_passphrase.suitable()) {

				gen_start_button = false;
			}
			gen_info_line(xml, "pad_2", "");
			if (gen_start_button) {

				gen_action_button_at_bottom(
					xml, "ok", "Start",
					_setup_obtain_params_hover == Setup_obtain_params_hover::START_BUTTON,
					_setup_obtain_params_select == Setup_obtain_params_select::START_BUTTON);
			}
		});
		break;

	case State::SETUP_RUN_CBE_INIT_TRUST_ANCHOR:
	case State::SETUP_CREATE_CBE_IMAGE_FILE:
	case State::SETUP_RUN_CBE_INIT:
	case State::SETUP_START_CBE_VFS:
	case State::SETUP_FORMAT_CBE:
	case State::STARTUP_RUN_CBE_INIT_TRUST_ANCHOR:
	case State::STARTUP_START_CBE_VFS:
	case State::STARTUP_DETERMINE_CLIENT_FS_SIZE:

		gen_info_frame(xml, "1", "Please wait...", MAIN_FRAME_WIDTH);
		break;

	case State::CONTROLS_ROOT:

		gen_controls_frame(xml, "app", [&] (Xml_generator &xml) {

			xml.node("frame", [&] () {

				xml.node("vbox", [&] () {

					if (SHOW_CONTROLS_SNAPSHOTS) {

						gen_closed_menu(
							xml, "Snapshots", "",
							_controls_root_hover == Controls_root_hover::SNAPSHOTS_EXPAND_BUTTON);
					}
					gen_closed_menu(
						xml, "Dimensions", "",
						_controls_root_hover == Controls_root_hover::DIMENSIONS_BUTTON);

					gen_closed_menu(
						xml, "Security", "",
						_controls_root_hover == Controls_root_hover::SECURITY_EXPAND_BUTTON);
				});
			});
			gen_global_controls(
				xml, MAIN_FRAME_WIDTH, _cbe_image_size, _client_fs_size, _nr_of_clients,
				_controls_root_hover  == Controls_root_hover::SHUT_DOWN_BUTTON,
				_controls_root_select == Controls_root_select::SHUT_DOWN_BUTTON);
		});
		break;

	case State::CONTROLS_SNAPSHOTS:

		gen_controls_frame(xml, "app", [&] (Xml_generator &xml) {

			xml.node("frame", [&] () {

				xml.node("vbox", [&] () {

					if (_snapshots_select.valid()) {

						Snapshot const &snap { _snapshots_select.object() };
						String<64> const snap_str {
							"Generation ", snap.generation() };

						gen_opened_menu(
							xml, snap_str.string(), "",
							_controls_snapshots_hover == Controls_snapshots_hover::GENERATION_LEAVE_BUTTON,
							[&] (Xml_generator &xml)
						{
							gen_info_line(xml, "pad_1", "");
							switch(_discard_snap_state) {
							case Discard_snapshot_state::INACTIVE:

								gen_action_button(xml, "Discard", "Discard",
									_controls_snapshots_hover  == Controls_snapshots_hover::GENERATION_DISCARD_BUTTON,
									_controls_snapshots_select == Controls_snapshots_select::GENERATION_DISCARD_BUTTON);

								break;

							case Discard_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

								gen_action_button(xml, "Inactive Discard", "...",
									_controls_snapshots_hover == Controls_snapshots_hover::GENERATION_DISCARD_BUTTON,
									false);

								break;
							}
						});
					} else {

						gen_opened_menu(
							xml, "Snapshots", "",
							_controls_snapshots_hover == Controls_snapshots_hover::LEAVE_BUTTON,
							[&] (Xml_generator &xml)
						{
							xml.node("vbox", [&] () {
								xml.attribute("name", "Generations");

								_snapshots.for_each([&] (Snapshot const &snap) {

									bool const hovered {
										_snapshots_hover.valid() &&
										_snapshots_hover.object().generation() == snap.generation() };

									String<64> const snap_str {
										"Generation ", snap.generation() };

									Generation_string const gen_str { snap.generation() };

									gen_multiple_choice_entry(
										xml, gen_str.string(), snap_str.string(), hovered,
										false);
								});
							});
							gen_info_line(xml, "pad_1", "");
							switch(_create_snap_state) {
							case Create_snapshot_state::INACTIVE:

								gen_action_button(xml, "Create", "Create",
									_controls_snapshots_hover  == Controls_snapshots_hover::CREATE_BUTTON,
									_controls_snapshots_select == Controls_snapshots_select::CREATE_BUTTON);
								break;

							case Create_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

								gen_action_button(xml, "Inactive Create", "...",
									_controls_snapshots_hover  == Controls_snapshots_hover::CREATE_BUTTON,
									false);

								break;
							}
						});
					}
				});
			});
			gen_global_controls(
				xml, MAIN_FRAME_WIDTH, _cbe_image_size, _client_fs_size, _nr_of_clients,
				_controls_snapshots_hover  == Controls_snapshots_hover::SHUT_DOWN_BUTTON,
				_controls_snapshots_select == Controls_snapshots_select::SHUT_DOWN_BUTTON);
		});
		break;

	case State::CONTROLS_DIMENSIONS:

		gen_controls_frame(xml, "app", [&] (Xml_generator &xml) {

			xml.node("frame", [&] () {

				gen_opened_menu(
					xml, "Dimensions", "",
					_dimensions_hover == Dimensions_hover::LEAVE_BUTTON,
					[&] (Xml_generator &xml)
				{
					gen_closed_menu(
						xml, "Expand Client FS", "",
						_dimensions_hover == Dimensions_hover::EXPAND_CLIENT_FS_BUTTON);

					gen_closed_menu(
						xml,
						RENAME_SNAPSHOT_BUFFER_JOURNALING_BUFFER ?
							"Expand Journaling Buffer" :
							"Expand Snapshot Buffer",
						"",
						_dimensions_hover == Dimensions_hover::EXPAND_SNAPSHOT_BUF_BUTTON);
				});
			});
			gen_global_controls(
				xml, MAIN_FRAME_WIDTH, _cbe_image_size, _client_fs_size, _nr_of_clients,
				_dimensions_hover  == Dimensions_hover::SHUT_DOWN_BUTTON,
				_dimensions_select == Dimensions_select::SHUT_DOWN_BUTTON);
		});
		break;

	case State::CONTROLS_EXPAND_CLIENT_FS:

		gen_controls_frame(xml, "app", [&] (Xml_generator &xml) {

			xml.node("frame", [&] () {

				xml.node("vbox", [&] () {

					gen_opened_menu(
						xml, "Expand Client FS", "",
						_expand_client_fs_hover == Expand_client_fs_hover::LEAVE_BUTTON,
						[&] (Xml_generator &xml)
					{
						gen_info_line(xml, "pad_1", "");
						switch (_resizing_state) {
						case Resizing_state::INACTIVE:
						{
							if (_nr_of_clients > 0) {

								gen_centered_info_line(xml, "Info 1", "Not possible while in use!");
								gen_info_line(xml, "Padding 1", "");

							} else {

								gen_titled_text_input(
									xml, "Contingent", "Contingent",
									_expand_client_fs_contingent,
									_expand_client_fs_select == Expand_client_fs_select::CONTINGENT_INPUT);

								bool gen_start_button { true };
								size_t const bytes {
									_expand_client_fs_contingent.value() };

								size_t const effective_bytes {
									bytes - (bytes % CBE_BLOCK_SIZE) };

								if (effective_bytes > 0) {

									gen_info_line(
										xml, "inf_2",
										String<128> {
											"New image size: ",
											Capacity { _cbe_image_size + effective_bytes }
										}.string());

								}  else {

									gen_info_line(xml, "info_1",
										String<128> {
											"Must be at least ",
											Number_of_bytes { CBE_BLOCK_SIZE } }.string());

									gen_start_button = false;
								}
								gen_info_line(xml, "pad_2", "");
								if (gen_start_button) {

									gen_action_button_at_bottom(
										xml, "Start",
										_expand_client_fs_hover  == Expand_client_fs_hover::START_BUTTON,
										_expand_client_fs_select == Expand_client_fs_select::START_BUTTON);
								}
							}
							break;
						}
						case Resizing_state::ADAPT_CBE_IMAGE_SIZE:
						case Resizing_state::WAIT_TILL_DEVICE_IS_READY:
						case Resizing_state::ISSUE_REQUEST_AT_DEVICE:
						case Resizing_state::IN_PROGRESS_AT_DEVICE:
						case Resizing_state::DETERMINE_CLIENT_FS_SIZE:
						case Resizing_state::RUN_RESIZE2FS:

							gen_centered_info_line(xml, "inf", "Please wait...");
							gen_info_line(xml, "pad_2", "");
							break;
						}
					});
				});
			});
			gen_global_controls(
				xml, MAIN_FRAME_WIDTH, _cbe_image_size, _client_fs_size, _nr_of_clients,
				_expand_client_fs_hover  == Expand_client_fs_hover::SHUT_DOWN_BUTTON,
				_expand_client_fs_select == Expand_client_fs_select::SHUT_DOWN_BUTTON);
		});
		break;

	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:

		gen_controls_frame(xml, "app", [&] (Xml_generator &xml) {

			xml.node("frame", [&] () {

				xml.node("vbox", [&] () {

					gen_opened_menu(
						xml,
						RENAME_SNAPSHOT_BUFFER_JOURNALING_BUFFER ?
							"Expand Journaling Buffer" :
							"Expand Snapshot Buffer",
						"",
						_expand_snapshot_buf_hover == Expand_snapshot_buf_hover::LEAVE_BUTTON,
						[&] (Xml_generator &xml)
					{
						gen_info_line(xml, "pad_1", "");
						switch (_resizing_state) {
						case Resizing_state::INACTIVE:
						{
							gen_titled_text_input(
								xml, "Contingent", "Contingent",
								_expand_snapshot_buf_contingent,
								_expand_snapshot_buf_select == Expand_snapshot_buf_select::CONTINGENT_INPUT);

							bool gen_start_button { true };
							size_t const bytes {
								_expand_snapshot_buf_contingent.value() };

							size_t const effective_bytes {
								bytes - (bytes % CBE_BLOCK_SIZE) };

							if (effective_bytes > 0) {

								gen_info_line(
									xml, "inf_2",
									String<128> {
										"New image size: ",
										Capacity { _cbe_image_size + effective_bytes }
									}.string());

							}  else {

								gen_start_button = false;
								gen_info_line(xml, "info_1",
									String<128> {
										"Must be at least ",
										Number_of_bytes { CBE_BLOCK_SIZE } }.string());
							}
							gen_info_line(xml, "pad_2", "");
							if (gen_start_button) {

								gen_action_button_at_bottom(
									xml, "Start",
									_expand_snapshot_buf_hover  == Expand_snapshot_buf_hover::START_BUTTON,
									_expand_snapshot_buf_select == Expand_snapshot_buf_select::START_BUTTON);
							}
							break;
						}
						case Resizing_state::ADAPT_CBE_IMAGE_SIZE:
						case Resizing_state::WAIT_TILL_DEVICE_IS_READY:
						case Resizing_state::ISSUE_REQUEST_AT_DEVICE:
						case Resizing_state::IN_PROGRESS_AT_DEVICE:
						case Resizing_state::DETERMINE_CLIENT_FS_SIZE:
						case Resizing_state::RUN_RESIZE2FS:

							gen_centered_info_line(xml, "inf", "Please wait...");
							gen_info_line(xml, "pad_2", "");
							break;
						}
					});
				});
			});
			gen_global_controls(
				xml, MAIN_FRAME_WIDTH, _cbe_image_size, _client_fs_size, _nr_of_clients,
				_expand_snapshot_buf_hover  == Expand_snapshot_buf_hover::SHUT_DOWN_BUTTON,
				_expand_snapshot_buf_select == Expand_snapshot_buf_select::SHUT_DOWN_BUTTON);
		});
		break;

	case State::CONTROLS_SECURITY:

		gen_controls_frame(xml, "app", [&] (Xml_generator &xml) {

			xml.node("frame", [&] () {

				gen_opened_menu(
					xml, "Security", "",
					_controls_security_hover == Controls_security_hover::SECURITY_EXPAND_BUTTON,
					[&] (Xml_generator &xml)
				{
					gen_closed_menu(
						xml, "Block Encryption Key", "",
						_controls_security_hover == Controls_security_hover::BLOCK_ENCRYPTION_KEY_EXPAND_BUTTON);

					if (SHOW_CONTROLS_SECURITY_MASTER_KEY) {

						gen_closed_menu(
							xml, "Master Key", "",
							_controls_security_hover == Controls_security_hover::MASTER_KEY_EXPAND_BUTTON);
					}
					if (SHOW_CONTROLS_SECURITY_USER_PASSPHRASE) {

						gen_closed_menu(
							xml, "User Passphrase", "",
							_controls_security_hover == Controls_security_hover::USER_PASSPHRASE_EXPAND_BUTTON);
					}
				});
			});
			gen_global_controls(
				xml, MAIN_FRAME_WIDTH, _cbe_image_size, _client_fs_size, _nr_of_clients,
				_controls_security_hover  == Controls_security_hover::SHUT_DOWN_BUTTON,
				_controls_security_select == Controls_security_select::SHUT_DOWN_BUTTON);
		});
		break;

	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:

		gen_controls_frame(xml, "app", [&] (Xml_generator &xml) {

			xml.node("frame", [&] () {

				gen_opened_menu(
					xml, "Block Encryption Key", "",
					_controls_security_block_encryption_key_hover == Controls_security_block_encryption_key_hover::LEAVE_BUTTON,
					[&] (Xml_generator &xml)
				{
					gen_info_line(xml, "pad_1", "");
					switch(_rekeying_state) {
					case Rekeying_state::INACTIVE:

						gen_action_button(xml, "Rekey", "Replace",
							_controls_security_block_encryption_key_hover  == Controls_security_block_encryption_key_hover::REPLACE_BUTTON,
							_controls_security_block_encryption_key_select == Controls_security_block_encryption_key_select::REPLACE_BUTTON);

						break;

					case Rekeying_state::WAIT_TILL_DEVICE_IS_READY:
					case Rekeying_state::ISSUE_REQUEST_AT_DEVICE:
					case Rekeying_state::IN_PROGRESS_AT_DEVICE:

						gen_centered_info_line(xml, "inf", "Please wait...");
						gen_info_line(xml, "pad_2", "");
						break;
					}
					gen_info_line(xml, "pad_1", "");
				});
			});
			gen_global_controls(
				xml, MAIN_FRAME_WIDTH, _cbe_image_size, _client_fs_size, _nr_of_clients,
				_controls_security_block_encryption_key_hover  == Controls_security_block_encryption_key_hover::SHUT_DOWN_BUTTON,
				_controls_security_block_encryption_key_select == Controls_security_block_encryption_key_select::SHUT_DOWN_BUTTON);
		});
		break;

	case State::CONTROLS_SECURITY_MASTER_KEY:

		gen_controls_frame(xml, "app", [&] (Xml_generator &xml) {

			xml.node("frame", [&] () {

				gen_opened_menu(
					xml, "Master Key", "",
					_controls_security_master_key_hover == Controls_security_master_key_hover::LEAVE_BUTTON,
					[&] (Xml_generator &xml)
				{
					gen_info_line(xml, "pad_1", "");
					gen_info_line(xml, "inf_1", "The master key cannot be replaced by now.");
					gen_info_line(xml, "pad_2", "");
				});
			});
			gen_global_controls(
				xml, MAIN_FRAME_WIDTH, _cbe_image_size, _client_fs_size, _nr_of_clients,
				_controls_security_master_key_hover  == Controls_security_master_key_hover::SHUT_DOWN_BUTTON,
				_controls_security_master_key_select == Controls_security_master_key_select::SHUT_DOWN_BUTTON);
		});
		break;

	case State::CONTROLS_SECURITY_USER_PASSPHRASE:

		gen_controls_frame(xml, "app", [&] (Xml_generator &xml) {

			xml.node("frame", [&] () {

				gen_opened_menu(
					xml, "User Passphrase", "",
					_controls_security_user_passphrase_hover == Controls_security_user_passphrase_hover::LEAVE_BUTTON,
					[&] (Xml_generator &xml)
				{
					gen_info_line(xml, "pad_1", "");
					gen_info_line(xml, "inf_1", "The user passphrase cannot be replaced by now.");
					gen_info_line(xml, "pad_2", "");
				});
			});
			gen_global_controls(
				xml, MAIN_FRAME_WIDTH, _cbe_image_size, _client_fs_size, _nr_of_clients,
				_controls_security_user_passphrase_hover  == Controls_security_user_passphrase_hover::SHUT_DOWN_BUTTON,
				_controls_security_user_passphrase_select == Controls_security_user_passphrase_select::SHUT_DOWN_BUTTON);
		});
		break;

	case State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE:
	case State::SHUTDOWN_WAIT_TILL_DEINIT_REQUEST_IS_DONE:

		gen_info_frame(xml, "1", "Please wait...", MAIN_FRAME_WIDTH);
		break;
	}
}


void File_vault::Main::wakeup_local_service()
{
	_rom_service.for_each_requested_session([&] (Rom_service::Request &request) {

		if (request.label == "menu_view -> dialog")
			request.deliver_session(_dialog);
		else
			request.deny();
	});

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

		} else if (request.label == "snapshots_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _snapshots_fs_query_listing_handler, _env.ep(),
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

		} else if (request.label == "shut_down_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _shut_down_fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} else {

			error("failed to deliver Report session");
		}
	});

	_report_service.for_each_requested_session([&] (Report_service::Request &request) {

		if (request.label == "menu_view -> hover") {
			Report::Session_component &session = *new (_heap)
				Report::Session_component(_env, _hover_handler,
				                          _env.ep(),
				                          request.resources, "", request.diag);
			request.deliver_session(session);
		}
	});

	_report_service.for_each_session_to_close([&] (Report::Session_component &session) {

		destroy(_heap, &session);
		return Report_service::Close_response::CLOSED;
	});

	_gui_service.for_each_requested_session([&] (Gui_service::Request &request) {

		Gui::Session_component &session = *new (_heap)
			Gui::Session_component(_env, *this, _env.ep(),
			                       request.resources, "", request.diag);

		request.deliver_session(session);
	});

	_gui_service.for_each_upgraded_session([&] (Gui::Session_component &session,
	                                            Session::Resources const &amount) {
		session.upgrade(amount);
		return Gui_service::Upgrade_response::CONFIRMED;
	});

	_gui_service.for_each_session_to_close([&] (Gui::Session_component &session) {

		destroy(_heap, &session);
		return Gui_service::Close_response::CLOSED;
	});
}


size_t Main::_cbe_tree_nr_of_leaves(size_t payload_size)
{
	size_t nr_of_leaves { payload_size / CBE_BLOCK_SIZE };
	if (payload_size % CBE_BLOCK_SIZE) {
		nr_of_leaves++;
	}
	return nr_of_leaves;
}


void File_vault::Main::_generate_sandbox_config(Xml_generator &xml) const
{
	switch (_state) {
	case State::INVALID:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_fs_query_start_node(xml, _fs_query);
		break;

	case State::SETUP_OBTAIN_PARAMETERS:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		break;

	case State::STARTUP_OBTAIN_PARAMETERS:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		break;

	case State::SETUP_RUN_CBE_INIT_TRUST_ANCHOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_init_trust_anchor_start_node(
			xml, _cbe_init_trust_anchor, _setup_obtain_params_passphrase);

		break;

	case State::STARTUP_RUN_CBE_INIT_TRUST_ANCHOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_init_trust_anchor_start_node(
			xml, _cbe_init_trust_anchor, _setup_obtain_params_passphrase);

		break;

	case State::STARTUP_START_CBE_VFS:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_vfs_start_node(xml, _cbe_vfs);
		gen_sync_to_cbe_vfs_init_start_node(xml, _sync_to_cbe_vfs_init);
		break;

	case State::STARTUP_DETERMINE_CLIENT_FS_SIZE:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_vfs_start_node(xml, _cbe_vfs);
		gen_client_fs_fs_query_start_node(xml, _client_fs_fs_query);
		break;

	case State::SETUP_CREATE_CBE_IMAGE_FILE:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_truncate_file_start_node(
			xml, _truncate_file, "/cbe/cbe.img",
			CBE_BLOCK_SIZE *
				_cbe_nr_of_blocks(
					CBE_NR_OF_SUPERBLOCKS,
					CBE_VBD_TREE_NR_OF_LEVELS,
					CBE_VBD_TREE_NR_OF_CHILDREN,
					_cbe_tree_nr_of_leaves(_client_fs_size_input.value()),
					CBE_FREE_TREE_NR_OF_LEVELS,
					CBE_FREE_TREE_NR_OF_CHILDREN,
					_cbe_tree_nr_of_leaves(_snapshot_buf_size_input.value())));

		break;

	case State::SETUP_RUN_CBE_INIT:
	{
		Tree_geometry const vbd_tree_geom {
			CBE_VBD_TREE_NR_OF_LEVELS,
			CBE_VBD_TREE_NR_OF_CHILDREN,
			_cbe_tree_nr_of_leaves(_client_fs_size_input.value()) };

		Tree_geometry const free_tree_geom {
			CBE_VBD_TREE_NR_OF_LEVELS,
			CBE_VBD_TREE_NR_OF_CHILDREN,
			_cbe_tree_nr_of_leaves(_snapshot_buf_size_input.value()) };

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_image_vfs_block_start_node(xml, _cbe_image_vfs_block);
		gen_cbe_init_start_node(xml, _cbe_init, vbd_tree_geom, free_tree_geom);
		break;
	}
	case State::SETUP_START_CBE_VFS:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_vfs_start_node(xml, _cbe_vfs);
		gen_sync_to_cbe_vfs_init_start_node(xml, _sync_to_cbe_vfs_init);
		break;

	case State::SETUP_FORMAT_CBE:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_vfs_start_node(xml, _cbe_vfs);
		gen_cbe_vfs_block_start_node(xml, _cbe_vfs_block);
		gen_mke2fs_start_node(xml, _mke2fs);
		break;

	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_EXPAND_CLIENT_FS:
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:
	{
		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_vfs_start_node(xml, _cbe_vfs);
		gen_cbe_vfs_block_start_node(xml, _cbe_vfs_block);
		gen_snapshots_fs_query_start_node(xml, _snapshots_fs_query);
		gen_image_fs_query_start_node(xml, _image_fs_query);

		switch(_resizing_state) {
		case Resizing_state::INACTIVE:

			break;

		case Resizing_state::ADAPT_CBE_IMAGE_SIZE:

			switch (_resizing_type) {
			case Resizing_type::EXPAND_CLIENT_FS:
			{
				size_t const bytes {
					_expand_client_fs_contingent.value() };

				size_t const effective_bytes {
					bytes - (bytes % CBE_BLOCK_SIZE) };

				gen_truncate_file_start_node(
					xml, _truncate_file, "/cbe/cbe.img",
					_cbe_image_size + effective_bytes);

				break;
			}
			case Resizing_type::EXPAND_SNAPSHOT_BUF:
			{
				size_t const bytes {
					_expand_snapshot_buf_contingent.value() };

				size_t const effective_bytes {
					bytes - (bytes % CBE_BLOCK_SIZE) };

				gen_truncate_file_start_node(
					xml, _truncate_file, "/cbe/cbe.img",
					_cbe_image_size + effective_bytes);

				break;
			}
			default:

				class Unexpected_resizing_type { };
				throw Unexpected_resizing_type { };
				break;
			}
			break;

		case Resizing_state::WAIT_TILL_DEVICE_IS_READY:

			gen_resizing_fs_query_start_node(xml, _resizing_fs_query);
			break;

		case Resizing_state::ISSUE_REQUEST_AT_DEVICE:

			switch (_resizing_type) {
			case Resizing_type::EXPAND_CLIENT_FS:

				gen_resizing_fs_tool_start_node(
					xml, _resizing_fs_tool, "vbd",
					_expand_client_fs_contingent.value() / CBE_BLOCK_SIZE);

				break;

			case Resizing_type::EXPAND_SNAPSHOT_BUF:

				gen_resizing_fs_tool_start_node(
					xml, _resizing_fs_tool, "ft",
					_expand_snapshot_buf_contingent.value() / CBE_BLOCK_SIZE);

				break;

			default:

				class Unexpected_resizing_type { };
				throw Unexpected_resizing_type { };
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

		switch(_create_snap_state) {
		case Create_snapshot_state::INACTIVE:

			break;

		case Create_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

			gen_create_snap_fs_tool_start_node(xml, _create_snap_fs_tool);
			break;
		}

		switch(_discard_snap_state) {
		case Discard_snapshot_state::INACTIVE:

			break;

		case Discard_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

			gen_discard_snap_fs_tool_start_node(xml, _discard_snap_fs_tool, _discard_snap_gen);
			break;
		}

		if (_resizing_state == Resizing_state::INACTIVE ||
		    _resizing_type != Resizing_type::EXPAND_CLIENT_FS) {

			gen_policy_for_child_service(xml, "File_system", _rump_vfs);
			gen_rump_vfs_start_node(xml, _rump_vfs);
		}
		break;
	}
	case State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE:

		gen_parent_provides_and_report_nodes(xml);
		gen_policy_for_child_service(xml, "File_system", _rump_vfs);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_vfs_start_node(xml, _cbe_vfs);
		gen_cbe_vfs_block_start_node(xml, _cbe_vfs_block);
		gen_snapshots_fs_query_start_node(xml, _snapshots_fs_query);
		gen_shut_down_fs_tool_start_node(xml, _shut_down_fs_tool);
		break;

	case State::SHUTDOWN_WAIT_TILL_DEINIT_REQUEST_IS_DONE:

		gen_parent_provides_and_report_nodes(xml);
		gen_policy_for_child_service(xml, "File_system", _rump_vfs);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_vfs_start_node(xml, _cbe_vfs);
		gen_cbe_vfs_block_start_node(xml, _cbe_vfs_block);
		gen_snapshots_fs_query_start_node(xml, _snapshots_fs_query);
		gen_shut_down_fs_query_start_node(xml, _shut_down_fs_query);
		break;
	}
}


size_t Main::_tree_nr_of_blocks(size_t nr_of_lvls,
                                size_t nr_of_children,
                                size_t nr_of_leafs)
{
	size_t nr_of_blks { 0 };
	size_t nr_of_last_lvl_blks { nr_of_leafs };
	for (size_t lvl_idx { 0 }; lvl_idx < nr_of_lvls; lvl_idx++) {
		nr_of_blks += nr_of_last_lvl_blks;
		if (nr_of_last_lvl_blks % nr_of_children) {
			nr_of_last_lvl_blks = nr_of_last_lvl_blks / nr_of_children + 1;
		} else {
			nr_of_last_lvl_blks = nr_of_last_lvl_blks / nr_of_children;
		}
	}
	return nr_of_blks;
}


size_t Main::_cbe_size() const
{
	return
		_cbe_nr_of_blocks(
			CBE_NR_OF_SUPERBLOCKS,
			CBE_VBD_TREE_NR_OF_LEVELS,
			CBE_VBD_TREE_NR_OF_CHILDREN,
			_cbe_tree_nr_of_leaves(_client_fs_size_input.value()),
			CBE_FREE_TREE_NR_OF_LEVELS,
			CBE_FREE_TREE_NR_OF_CHILDREN,
			_cbe_tree_nr_of_leaves(_snapshot_buf_size_input.value()))
		* CBE_BLOCK_SIZE;
}


size_t Main::_cbe_nr_of_blocks(size_t nr_of_superblocks,
                               size_t nr_of_vbd_lvls,
                               size_t nr_of_vbd_children,
                               size_t nr_of_vbd_leafs,
                               size_t nr_of_ft_lvls,
                               size_t nr_of_ft_children,
                               size_t nr_of_ft_leafs)
{
	size_t const nr_of_vbd_blks {
		_tree_nr_of_blocks(
			nr_of_vbd_lvls,
			nr_of_vbd_children,
			nr_of_vbd_leafs) };

	size_t const nr_of_ft_blks {
		_tree_nr_of_blocks(
			nr_of_ft_lvls,
			nr_of_ft_children,
			nr_of_ft_leafs) };

	/* FIXME
	 *
	 * This would be the correct way to calculate the number of MT blocks
	 * but the CBE still uses an MT the same size as the FT for simplicity
	 * reasons. As soon as the CBE does it right we should fix also this path.
	 *
	 *	size_t const nr_of_mt_leafs {
	 *		nr_of_ft_blks - nr_of_ft_leafs };
	 *
	 *	size_t const nr_of_mt_blks {
	 *		_tree_nr_of_blocks(
	 *			nr_of_mt_lvls,
	 *			nr_of_mt_children,
	 *			nr_of_mt_leafs) };
	 */
	size_t const nr_of_mt_blks { nr_of_ft_blks };

	return
		nr_of_superblocks +
		nr_of_vbd_blks +
		nr_of_ft_blks +
		nr_of_mt_blks;
}


void File_vault::Main::handle_input_event(Input::Event const &event)
{
	bool update_dialog { false };
	bool update_sandbox_config { false };

	switch (_state) {
	case State::SETUP_OBTAIN_PARAMETERS:

		event.handle_press([&] (Input::Keycode key, Codepoint code) {

			if (key == Input::BTN_LEFT) {

				Setup_obtain_params_select const prev_select { _setup_obtain_params_select };
				Setup_obtain_params_select       next_select { Setup_obtain_params_select::NONE };

				switch (_setup_obtain_params_hover) {
				case Setup_obtain_params_hover::START_BUTTON:

					next_select = Setup_obtain_params_select::START_BUTTON;
					break;

				case Setup_obtain_params_hover::PASSPHRASE_SHOW_HIDE_BUTTON:

					next_select = Setup_obtain_params_select::PASSPHRASE_SHOW_HIDE_BUTTON;
					break;

				case Setup_obtain_params_hover::PASSPHRASE_INPUT:

					next_select = Setup_obtain_params_select::PASSPHRASE_INPUT;
					break;

				case Setup_obtain_params_hover::CLIENT_FS_SIZE_INPUT:

					next_select = Setup_obtain_params_select::CLIENT_FS_SIZE_INPUT;
					break;

				case Setup_obtain_params_hover::SNAPSHOT_BUFFER_SIZE_INPUT:

					next_select = Setup_obtain_params_select::SNAPSHOT_BUFFER_SIZE_INPUT;
					break;

				case Setup_obtain_params_hover::NONE:

					next_select = Setup_obtain_params_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_setup_obtain_params_select = next_select;
					update_dialog = true;
				}

			} else if (key == Input::KEY_ENTER) {

				if (_client_fs_size_input.value() >= MIN_CLIENT_FS_SIZE &&
				    _snapshot_buf_size_input.value() >= _min_snapshot_buf_size() &&
				    _setup_obtain_params_passphrase.suitable() &&
				    _setup_obtain_params_select != Setup_obtain_params_select::START_BUTTON) {

					_setup_obtain_params_select = Setup_obtain_params_select::START_BUTTON;
					update_dialog = true;
				}

			} else if (key == Input::KEY_TAB) {

				switch (_setup_obtain_params_select) {
				case Setup_obtain_params_select::PASSPHRASE_INPUT:

					_setup_obtain_params_select = Setup_obtain_params_select::CLIENT_FS_SIZE_INPUT;
					update_dialog = true;
					break;

				case Setup_obtain_params_select::CLIENT_FS_SIZE_INPUT:

					_setup_obtain_params_select = Setup_obtain_params_select::SNAPSHOT_BUFFER_SIZE_INPUT;
					update_dialog = true;
					break;

				case Setup_obtain_params_select::SNAPSHOT_BUFFER_SIZE_INPUT:

					_setup_obtain_params_select = Setup_obtain_params_select::PASSPHRASE_INPUT;
					update_dialog = true;
					break;

				default:

					break;
				}

			} else {

				if (_setup_obtain_params_select == Setup_obtain_params_select::PASSPHRASE_INPUT) {

					if (_setup_obtain_params_passphrase.appendable_character(code)) {

						_setup_obtain_params_passphrase.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_setup_obtain_params_passphrase.remove_last_character();
						update_dialog = true;
					}

				} else if (_setup_obtain_params_select == Setup_obtain_params_select::CLIENT_FS_SIZE_INPUT) {

					if (_client_fs_size_input.appendable_character(code)) {

						_client_fs_size_input.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_client_fs_size_input.remove_last_character();
						update_dialog = true;

					}
				} else if (_setup_obtain_params_select == Setup_obtain_params_select::SNAPSHOT_BUFFER_SIZE_INPUT) {

					if (_snapshot_buf_size_input.appendable_character(code)) {

						_snapshot_buf_size_input.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_snapshot_buf_size_input.remove_last_character();
						update_dialog = true;

					}
				} else if (_setup_obtain_params_select == Setup_obtain_params_select::SNAPSHOT_BUFFER_SIZE_INPUT) {

					if (_snapshot_buf_size_input.appendable_character(code)) {

						_snapshot_buf_size_input.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_snapshot_buf_size_input.remove_last_character();
						update_dialog = true;

					}
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT ||
			    key == Input::KEY_ENTER) {

				switch (_setup_obtain_params_select) {
				case Setup_obtain_params_select::PASSPHRASE_SHOW_HIDE_BUTTON:

					if (_setup_obtain_params_passphrase.hide()) {
						_setup_obtain_params_passphrase.hide(false);
					} else {
						_setup_obtain_params_passphrase.hide(true);
					}
					_setup_obtain_params_select = Setup_obtain_params_select::PASSPHRASE_INPUT;
					update_dialog = true;
					break;

				case Setup_obtain_params_select::START_BUTTON:

					if(_client_fs_size_input.value() >= MIN_CLIENT_FS_SIZE &&
					   _snapshot_buf_size_input.value() >= _min_snapshot_buf_size() &&
					   _setup_obtain_params_passphrase.suitable()) {

						_setup_obtain_params_select = Setup_obtain_params_select::NONE;
						_state = State::SETUP_CREATE_CBE_IMAGE_FILE;
						update_sandbox_config = true;
						update_dialog = true;
					}
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::STARTUP_OBTAIN_PARAMETERS:

		event.handle_press([&] (Input::Keycode key, Codepoint code) {

			if (key == Input::BTN_LEFT) {

				Setup_obtain_params_select const prev_select { _setup_obtain_params_select };
				Setup_obtain_params_select       next_select { Setup_obtain_params_select::NONE };

				switch (_setup_obtain_params_hover) {
				case Setup_obtain_params_hover::PASSPHRASE_SHOW_HIDE_BUTTON:

					next_select = Setup_obtain_params_select::PASSPHRASE_SHOW_HIDE_BUTTON;
					break;

				case Setup_obtain_params_hover::START_BUTTON:

					next_select = Setup_obtain_params_select::START_BUTTON;
					break;

				case Setup_obtain_params_hover::PASSPHRASE_INPUT:

					next_select = Setup_obtain_params_select::PASSPHRASE_INPUT;
					break;

				case Setup_obtain_params_hover::CLIENT_FS_SIZE_INPUT:

					class Unexpected_hover_1 { };
					throw Unexpected_hover_1 { };

				case Setup_obtain_params_hover::SNAPSHOT_BUFFER_SIZE_INPUT:

					class Unexpected_hover_2 { };
					throw Unexpected_hover_2 { };

				case Setup_obtain_params_hover::NONE:

					next_select = Setup_obtain_params_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_setup_obtain_params_select = next_select;
					update_dialog = true;
				}

			} else if (key == Input::KEY_ENTER) {

				if (_setup_obtain_params_passphrase.suitable() &&
				    _setup_obtain_params_select != Setup_obtain_params_select::START_BUTTON) {

					_setup_obtain_params_select = Setup_obtain_params_select::START_BUTTON;
					update_dialog = true;
				}

			} else {

				if (_setup_obtain_params_select == Setup_obtain_params_select::PASSPHRASE_INPUT) {

					if (_setup_obtain_params_passphrase.appendable_character(code)) {

						_setup_obtain_params_passphrase.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_setup_obtain_params_passphrase.remove_last_character();
						update_dialog = true;
					}
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT ||
			    key == Input::KEY_ENTER) {

				switch (_setup_obtain_params_select) {
				case Setup_obtain_params_select::PASSPHRASE_SHOW_HIDE_BUTTON:

					if (_setup_obtain_params_passphrase.hide()) {
						_setup_obtain_params_passphrase.hide(false);
					} else {
						_setup_obtain_params_passphrase.hide(true);
					}
					_setup_obtain_params_select = Setup_obtain_params_select::PASSPHRASE_INPUT;
					update_dialog = true;
					break;

				case Setup_obtain_params_select::START_BUTTON:

					if (_setup_obtain_params_passphrase.suitable()) {

						_setup_obtain_params_select = Setup_obtain_params_select::NONE;
						_state = State::STARTUP_RUN_CBE_INIT_TRUST_ANCHOR;
						update_sandbox_config = true;
						update_dialog = true;
					}
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::CONTROLS_ROOT:

		event.handle_press([&] (Input::Keycode key, Codepoint) {

			if (key == Input::BTN_LEFT) {

				Controls_root_select const prev_select { _controls_root_select };
				Controls_root_select       next_select { Controls_root_select::NONE };

				switch (_controls_root_hover) {
				case Controls_root_hover::SNAPSHOTS_EXPAND_BUTTON:

					_state = State::CONTROLS_SNAPSHOTS;
					update_dialog = true;
					break;

				case Controls_root_hover::DIMENSIONS_BUTTON:

					_state = State::CONTROLS_DIMENSIONS;
					update_dialog = true;
					break;

				case Controls_root_hover::SECURITY_EXPAND_BUTTON:

					_state = State::CONTROLS_SECURITY;
					update_dialog = true;
					break;

				case Controls_root_hover::SHUT_DOWN_BUTTON:

					next_select = Controls_root_select::SHUT_DOWN_BUTTON;
					break;

				case Controls_root_hover::NONE:

					next_select = Controls_root_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_controls_root_select = next_select;
					update_dialog = true;
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_controls_root_select) {
				case Controls_root_select::SHUT_DOWN_BUTTON:

					_controls_root_select = Controls_root_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::CONTROLS_SNAPSHOTS:

		event.handle_press([&] (Input::Keycode key, Codepoint) {

			if (key == Input::BTN_LEFT) {

				Controls_snapshots_select const prev_select { _controls_snapshots_select };
				Controls_snapshots_select       next_select { Controls_snapshots_select::NONE };

				switch (_controls_snapshots_hover) {
				case Controls_snapshots_hover::LEAVE_BUTTON:

					_state = State::CONTROLS_ROOT;
					update_dialog = true;
					break;

				case Controls_snapshots_hover::SHUT_DOWN_BUTTON:

					next_select = Controls_snapshots_select::SHUT_DOWN_BUTTON;
					break;

				case Controls_snapshots_hover::CREATE_BUTTON:

					next_select = Controls_snapshots_select::CREATE_BUTTON;
					break;

				case Controls_snapshots_hover::GENERATION_DISCARD_BUTTON:

					next_select = Controls_snapshots_select::GENERATION_DISCARD_BUTTON;
					break;

				case Controls_snapshots_hover::GENERATION_LEAVE_BUTTON:

					_snapshots_select = Snapshot_pointer { };
					update_dialog = true;
					break;

				case Controls_snapshots_hover::NONE:

					next_select = Controls_snapshots_select::NONE;
					break;
				}
				if (_snapshots_hover.valid()) {

					if (_snapshots_hover != _snapshots_select) {

						_snapshots_select = _snapshots_hover;
						update_dialog = true;
					} else {

						_snapshots_select = Snapshot_pointer { };
						update_dialog = true;
					}
				}
				if (next_select != prev_select) {

					_controls_snapshots_select = next_select;
					update_dialog = true;
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_controls_snapshots_select) {
				case Controls_snapshots_select::SHUT_DOWN_BUTTON:

					_controls_snapshots_select = Controls_snapshots_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				case Controls_snapshots_select::CREATE_BUTTON:

					_controls_snapshots_select = Controls_snapshots_select::NONE;
					_create_snap_state = Create_snapshot_state::ISSUE_REQUEST_AT_DEVICE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				case Controls_snapshots_select::GENERATION_DISCARD_BUTTON:

					_controls_snapshots_select = Controls_snapshots_select::NONE;

					if (_snapshots_select.valid()) {
						_discard_snap_state = Discard_snapshot_state::ISSUE_REQUEST_AT_DEVICE;
						_discard_snap_gen = _snapshots_select.object().generation();
					}
					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::CONTROLS_DIMENSIONS:

		event.handle_press([&] (Input::Keycode key, Codepoint) {

			if (key == Input::BTN_LEFT) {

				Dimensions_select const prev_select { _dimensions_select };
				Dimensions_select       next_select { Dimensions_select::NONE };

				switch (_dimensions_hover) {
				case Dimensions_hover::LEAVE_BUTTON:

					_state = State::CONTROLS_ROOT;
					update_dialog = true;
					break;

				case Dimensions_hover::EXPAND_CLIENT_FS_BUTTON:

					_state = State::CONTROLS_EXPAND_CLIENT_FS;
					_expand_client_fs_select = Expand_client_fs_select::CONTINGENT_INPUT;
					update_dialog = true;
					break;

				case Dimensions_hover::EXPAND_SNAPSHOT_BUF_BUTTON:

					_state = State::CONTROLS_EXPAND_SNAPSHOT_BUF;
					_expand_snapshot_buf_select = Expand_snapshot_buf_select::CONTINGENT_INPUT;
					update_dialog = true;
					break;

				case Dimensions_hover::SHUT_DOWN_BUTTON:

					next_select = Dimensions_select::SHUT_DOWN_BUTTON;
					break;

				case Dimensions_hover::NONE:

					next_select = Dimensions_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_dimensions_select = next_select;
					update_dialog = true;
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_dimensions_select) {
				case Dimensions_select::SHUT_DOWN_BUTTON:

					_dimensions_select = Dimensions_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::CONTROLS_EXPAND_CLIENT_FS:

		if (_nr_of_clients > 0) {

			event.handle_press([&] (Input::Keycode key, Codepoint) {

				if (key == Input::BTN_LEFT) {

					Expand_client_fs_select const prev_select { _expand_client_fs_select };
					Expand_client_fs_select       next_select { Expand_client_fs_select::NONE };

					switch (_expand_client_fs_hover) {
					case Expand_client_fs_hover::LEAVE_BUTTON:

						_state = State::CONTROLS_DIMENSIONS;
						update_dialog = true;
						break;

					case Expand_client_fs_hover::SHUT_DOWN_BUTTON:

						next_select = Expand_client_fs_select::SHUT_DOWN_BUTTON;
						break;

					case Expand_client_fs_hover::START_BUTTON:
					case Expand_client_fs_hover::CONTINGENT_INPUT:

						break;

					case Expand_client_fs_hover::NONE:

						next_select = Expand_client_fs_select::NONE;
						break;
					}
					if (next_select != prev_select) {

						_expand_client_fs_select = next_select;
						update_dialog = true;
					}

				}
			});
			event.handle_release([&] (Input::Keycode key) {

				if (key == Input::BTN_LEFT ||
					key == Input::KEY_ENTER) {

					switch (_expand_client_fs_select) {
					case Expand_client_fs_select::START_BUTTON:

						break;

					case Expand_client_fs_select::SHUT_DOWN_BUTTON:

						_expand_client_fs_select = Expand_client_fs_select::NONE;
						_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

						update_sandbox_config = true;
						update_dialog = true;
						break;

					default:

						break;
					}
				}
			});

		} else {

			event.handle_press([&] (Input::Keycode key, Codepoint code) {

				if (key == Input::BTN_LEFT) {

					Expand_client_fs_select const prev_select { _expand_client_fs_select };
					Expand_client_fs_select       next_select { Expand_client_fs_select::NONE };

					switch (_expand_client_fs_hover) {
					case Expand_client_fs_hover::LEAVE_BUTTON:

						_state = State::CONTROLS_DIMENSIONS;
						update_dialog = true;
						break;

					case Expand_client_fs_hover::SHUT_DOWN_BUTTON:

						next_select = Expand_client_fs_select::SHUT_DOWN_BUTTON;
						break;

					case Expand_client_fs_hover::START_BUTTON:

						next_select = Expand_client_fs_select::START_BUTTON;
						break;

					case Expand_client_fs_hover::CONTINGENT_INPUT:

						next_select = Expand_client_fs_select::CONTINGENT_INPUT;
						break;

					case Expand_client_fs_hover::NONE:

						next_select = Expand_client_fs_select::NONE;
						break;
					}
					if (next_select != prev_select) {

						_expand_client_fs_select = next_select;
						update_dialog = true;
					}

				} else if (key == Input::KEY_ENTER) {

					size_t const bytes {
						_expand_client_fs_contingent.value() };

					size_t const effective_bytes {
						bytes - (bytes % CBE_BLOCK_SIZE) };

					if (effective_bytes > 0) {

						_expand_client_fs_select = Expand_client_fs_select::START_BUTTON;
						update_dialog = true;
					}
				} else {

					if (_expand_client_fs_select == Expand_client_fs_select::CONTINGENT_INPUT) {

						if (_expand_client_fs_contingent.appendable_character(code)) {

							_expand_client_fs_contingent.append_character(code);
							update_dialog = true;

						} else if (code.value == CODEPOINT_BACKSPACE) {

							_expand_client_fs_contingent.remove_last_character();
							update_dialog = true;
						}
					}
				}
			});
			event.handle_release([&] (Input::Keycode key) {

				if (key == Input::BTN_LEFT ||
					key == Input::KEY_ENTER) {

					switch (_expand_client_fs_select) {
					case Expand_client_fs_select::START_BUTTON:

						_expand_client_fs_select = Expand_client_fs_select::NONE;
						_resizing_type = Resizing_type::EXPAND_CLIENT_FS;
						_resizing_state = Resizing_state::ADAPT_CBE_IMAGE_SIZE;
						update_sandbox_config = true;
						update_dialog = true;

						break;

					case Expand_client_fs_select::SHUT_DOWN_BUTTON:

						_expand_client_fs_select = Expand_client_fs_select::NONE;
						_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

						update_sandbox_config = true;
						update_dialog = true;
						break;

					default:

						break;
					}
				}
			});
		}
		break;

	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:

		event.handle_press([&] (Input::Keycode key, Codepoint code) {

			if (key == Input::BTN_LEFT) {

				Expand_snapshot_buf_select const prev_select { _expand_snapshot_buf_select };
				Expand_snapshot_buf_select       next_select { Expand_snapshot_buf_select::NONE };

				switch (_expand_snapshot_buf_hover) {
				case Expand_snapshot_buf_hover::LEAVE_BUTTON:

					_state = State::CONTROLS_DIMENSIONS;
					update_dialog = true;
					break;

				case Expand_snapshot_buf_hover::SHUT_DOWN_BUTTON:

					next_select = Expand_snapshot_buf_select::SHUT_DOWN_BUTTON;
					break;

				case Expand_snapshot_buf_hover::START_BUTTON:

					next_select = Expand_snapshot_buf_select::START_BUTTON;
					break;

				case Expand_snapshot_buf_hover::CONTINGENT_INPUT:

					next_select = Expand_snapshot_buf_select::CONTINGENT_INPUT;
					break;

				case Expand_snapshot_buf_hover::NONE:

					next_select = Expand_snapshot_buf_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_expand_snapshot_buf_select = next_select;
					update_dialog = true;
				}

			} else if (key == Input::KEY_ENTER) {

				size_t const bytes {
					_expand_snapshot_buf_contingent.value() };

				size_t const effective_bytes {
					bytes - (bytes % CBE_BLOCK_SIZE) };

				if (effective_bytes > 0) {

					_expand_snapshot_buf_select = Expand_snapshot_buf_select::START_BUTTON;
					update_dialog = true;
				}
			} else {

				if (_expand_snapshot_buf_select == Expand_snapshot_buf_select::CONTINGENT_INPUT) {

					if (_expand_snapshot_buf_contingent.appendable_character(code)) {

						_expand_snapshot_buf_contingent.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_expand_snapshot_buf_contingent.remove_last_character();
						update_dialog = true;
					}
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT ||
			    key == Input::KEY_ENTER) {

				switch (_expand_snapshot_buf_select) {
				case Expand_snapshot_buf_select::START_BUTTON:

					_expand_snapshot_buf_select = Expand_snapshot_buf_select::NONE;
					_resizing_type = Resizing_type::EXPAND_SNAPSHOT_BUF;
					_resizing_state = Resizing_state::WAIT_TILL_DEVICE_IS_READY;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				case Expand_snapshot_buf_select::SHUT_DOWN_BUTTON:

					_expand_snapshot_buf_select = Expand_snapshot_buf_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::CONTROLS_SECURITY:

		event.handle_press([&] (Input::Keycode key, Codepoint) {

			if (key == Input::BTN_LEFT) {

				Controls_security_select const prev_select { _controls_security_select };
				Controls_security_select       next_select { Controls_security_select::NONE };

				switch (_controls_security_hover) {
				case Controls_security_hover::SECURITY_EXPAND_BUTTON:

					_state = State::CONTROLS_ROOT;
					update_dialog = true;
					break;

				case Controls_security_hover::BLOCK_ENCRYPTION_KEY_EXPAND_BUTTON:

					_state = State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY;
					update_dialog = true;
					break;

				case Controls_security_hover::MASTER_KEY_EXPAND_BUTTON:

					_state = State::CONTROLS_SECURITY_MASTER_KEY;
					update_dialog = true;
					break;

				case Controls_security_hover::USER_PASSPHRASE_EXPAND_BUTTON:

					_state = State::CONTROLS_SECURITY_USER_PASSPHRASE;
					update_dialog = true;
					break;

				case Controls_security_hover::SHUT_DOWN_BUTTON:

					next_select = Controls_security_select::SHUT_DOWN_BUTTON;
					break;

				case Controls_security_hover::NONE:

					next_select = Controls_security_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_controls_security_select = next_select;
					update_dialog = true;
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_controls_security_select) {
				case Controls_security_select::SHUT_DOWN_BUTTON:

					_controls_security_select = Controls_security_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:

		event.handle_press([&] (Input::Keycode key, Codepoint) {

			if (key == Input::BTN_LEFT) {

				Controls_security_block_encryption_key_select const prev_select { _controls_security_block_encryption_key_select };
				Controls_security_block_encryption_key_select       next_select { Controls_security_block_encryption_key_select::NONE };

				switch (_controls_security_block_encryption_key_hover) {
				case Controls_security_block_encryption_key_hover::LEAVE_BUTTON:

					_state = State::CONTROLS_SECURITY;
					update_dialog = true;
					break;

				case Controls_security_block_encryption_key_hover::REPLACE_BUTTON:

					next_select = Controls_security_block_encryption_key_select::REPLACE_BUTTON;
					break;

				case Controls_security_block_encryption_key_hover::SHUT_DOWN_BUTTON:

					next_select = Controls_security_block_encryption_key_select::SHUT_DOWN_BUTTON;
					break;

				case Controls_security_block_encryption_key_hover::NONE:

					next_select = Controls_security_block_encryption_key_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_controls_security_block_encryption_key_select = next_select;
					update_dialog = true;
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_controls_security_block_encryption_key_select) {
				case Controls_security_block_encryption_key_select::REPLACE_BUTTON:

					_controls_security_block_encryption_key_select = Controls_security_block_encryption_key_select::NONE;
					_rekeying_state = Rekeying_state::WAIT_TILL_DEVICE_IS_READY;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				case Controls_security_block_encryption_key_select::SHUT_DOWN_BUTTON:

					_controls_security_block_encryption_key_select = Controls_security_block_encryption_key_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::CONTROLS_SECURITY_MASTER_KEY:

		event.handle_press([&] (Input::Keycode key, Codepoint) {

			if (key == Input::BTN_LEFT) {

				Controls_security_master_key_select const prev_select { _controls_security_master_key_select };
				Controls_security_master_key_select       next_select { Controls_security_master_key_select::NONE };

				switch (_controls_security_master_key_hover) {
				case Controls_security_master_key_hover::LEAVE_BUTTON:

					_state = State::CONTROLS_SECURITY;
					update_dialog = true;
					break;

				case Controls_security_master_key_hover::SHUT_DOWN_BUTTON:

					next_select = Controls_security_master_key_select::SHUT_DOWN_BUTTON;
					break;

				case Controls_security_master_key_hover::NONE:

					next_select = Controls_security_master_key_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_controls_security_master_key_select = next_select;
					update_dialog = true;
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_controls_security_master_key_select) {
				case Controls_security_master_key_select::SHUT_DOWN_BUTTON:

					_controls_security_master_key_select = Controls_security_master_key_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::CONTROLS_SECURITY_USER_PASSPHRASE:

		event.handle_press([&] (Input::Keycode key, Codepoint) {

			if (key == Input::BTN_LEFT) {

				Controls_security_user_passphrase_select const prev_select { _controls_security_user_passphrase_select };
				Controls_security_user_passphrase_select       next_select { Controls_security_user_passphrase_select::NONE };

				switch (_controls_security_user_passphrase_hover) {
				case Controls_security_user_passphrase_hover::LEAVE_BUTTON:

					_state = State::CONTROLS_SECURITY;
					update_dialog = true;
					break;

				case Controls_security_user_passphrase_hover::SHUT_DOWN_BUTTON:

					next_select = Controls_security_user_passphrase_select::SHUT_DOWN_BUTTON;
					break;

				case Controls_security_user_passphrase_hover::NONE:

					next_select = Controls_security_user_passphrase_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_controls_security_user_passphrase_select = next_select;
					update_dialog = true;
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_controls_security_user_passphrase_select) {
				case Controls_security_user_passphrase_select::SHUT_DOWN_BUTTON:

					_controls_security_user_passphrase_select = Controls_security_user_passphrase_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	default:

		break;
	}
	if (update_sandbox_config) {
		_update_sandbox_config();
	}
	if (update_dialog) {
		_dialog.trigger_update();
	}
}


void File_vault::Main::_handle_hover(Xml_node const &node)
{
	bool update_dialog { false };

	switch (_state) {
	case State::SETUP_OBTAIN_PARAMETERS:
	{
		Setup_obtain_params_hover const prev_hover { _setup_obtain_params_hover };
		Setup_obtain_params_hover       next_hover { Setup_obtain_params_hover::NONE };

		node.with_optional_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_optional_sub_node("vbox", [&] (Xml_node const &node_2) {

					node_2.with_optional_sub_node("float", [&] (Xml_node const &node_3) {
						if (_has_name(node_3, "ok")) {
							next_hover = Setup_obtain_params_hover::START_BUTTON;
						}
					});
					node_2.with_optional_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("frame", [&] (Xml_node const &node_4) {

							if (_has_name(node_4, "Passphrase")) {
								next_hover = Setup_obtain_params_hover::PASSPHRASE_INPUT;
							}
						});
						node_3.with_optional_sub_node("float", [&] (Xml_node const &node_4) {
							node_4.with_optional_sub_node("button", [&] (Xml_node const &node_5) {

								if (_has_name(node_5, "Show Hide")) {
									next_hover = Setup_obtain_params_hover::PASSPHRASE_SHOW_HIDE_BUTTON;
								}
							});
						});
					});
					node_2.with_optional_sub_node("frame", [&] (Xml_node const &node_3) {

						if (_has_name(node_3, "Client FS Size")) {
							next_hover = Setup_obtain_params_hover::CLIENT_FS_SIZE_INPUT;

						} if (_has_name(node_3, "Snapshot Buffer Size")) {
							next_hover = Setup_obtain_params_hover::SNAPSHOT_BUFFER_SIZE_INPUT;
						}
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_setup_obtain_params_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::STARTUP_OBTAIN_PARAMETERS:
	{
		Setup_obtain_params_hover const prev_hover { _setup_obtain_params_hover };
		Setup_obtain_params_hover       next_hover { Setup_obtain_params_hover::NONE };

		node.with_optional_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_optional_sub_node("vbox", [&] (Xml_node const &node_2) {

					node_2.with_optional_sub_node("float", [&] (Xml_node const &node_3) {
						if (_has_name(node_3, "ok")) {
							next_hover = Setup_obtain_params_hover::START_BUTTON;
						}
					});
					node_2.with_optional_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("frame", [&] (Xml_node const &node_4) {

							if (_has_name(node_4, "Passphrase")) {
								next_hover = Setup_obtain_params_hover::PASSPHRASE_INPUT;
							}
						});
						node_3.with_optional_sub_node("float", [&] (Xml_node const &node_4) {
							node_4.with_optional_sub_node("button", [&] (Xml_node const &node_5) {

								if (_has_name(node_5, "Show Hide")) {
									next_hover = Setup_obtain_params_hover::PASSPHRASE_SHOW_HIDE_BUTTON;
								}
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_setup_obtain_params_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_ROOT:
	{
		Controls_root_hover const prev_hover { _controls_root_hover };
		Controls_root_hover       next_hover { Controls_root_hover::NONE };

		node.with_optional_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_optional_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_optional_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("button", [&] (Xml_node const &node_4) {

							if (_has_name(node_4, "Shut down")) {

								next_hover = Controls_root_hover::SHUT_DOWN_BUTTON;

							}
						});
					});
					node_2.with_optional_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_optional_sub_node("vbox", [&] (Xml_node const &node_5) {

								if (_has_name(node_5, "Snapshots")) {

									next_hover = Controls_root_hover::SNAPSHOTS_EXPAND_BUTTON;

								} else if (_has_name(node_5, "Dimensions")) {

									next_hover = Controls_root_hover::DIMENSIONS_BUTTON;

								} else if (_has_name(node_5, "Security")) {

									next_hover = Controls_root_hover::SECURITY_EXPAND_BUTTON;
								}
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_controls_root_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_SNAPSHOTS:
	{
		Controls_snapshots_hover const prev_hover { _controls_snapshots_hover };
		Controls_snapshots_hover       next_hover { Controls_snapshots_hover::NONE };

		Snapshot_pointer const prev_snapshots_hover { _snapshots_hover };
		Snapshot_pointer       next_snapshots_hover { };

		node.with_optional_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_optional_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_optional_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("button", [&] (Xml_node const &node_4) {

							if (_has_name(node_4, "Shut down")) {

								next_hover = Controls_snapshots_hover::SHUT_DOWN_BUTTON;
							}
						});
					});
					node_2.with_optional_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_optional_sub_node("vbox", [&] (Xml_node const &node_5) {

								if (_snapshots_select.valid()) {

									node_5.with_optional_sub_node("hbox", [&] (Xml_node const &node_6) {

										if (_has_name(node_6, "Leave")) {

											next_hover = Controls_snapshots_hover::GENERATION_LEAVE_BUTTON;
										}
									});
									node_5.with_optional_sub_node("button", [&] (Xml_node const &node_6) {

										if (_has_name(node_6, "Discard")) {

											next_hover = Controls_snapshots_hover::GENERATION_DISCARD_BUTTON;
										}
									});

								} else {

									node_5.with_optional_sub_node("hbox", [&] (Xml_node const &node_6) {

										if (_has_name(node_6, "Leave")) {

											next_hover = Controls_snapshots_hover::LEAVE_BUTTON;
										}
									});
									node_5.with_optional_sub_node("vbox", [&] (Xml_node const &node_6) {

										if (_has_name(node_6, "Generations")) {

											node_6.with_optional_sub_node("float", [&] (Xml_node const &node_7) {

												Generation const generation {
													node_7.attribute_value(
														"name", Generation { INVALID_GENERATION }) };

												if (generation != INVALID_GENERATION) {

													_snapshots.for_each([&] (Snapshot const &snap)
													{
														if (generation == snap.generation()) {
															next_snapshots_hover = snap;
														}
													});
												}
											});
										}
									});
									node_5.with_optional_sub_node("button", [&] (Xml_node const &node_6) {

										if (_has_name(node_6, "Create")) {

											next_hover = Controls_snapshots_hover::CREATE_BUTTON;
										}
									});
								}
							});
						});
					});
				});
			});
		});
		if (next_snapshots_hover != prev_snapshots_hover) {

			_snapshots_hover = next_snapshots_hover;
			update_dialog = true;
		}
		if (next_hover != prev_hover) {

			_controls_snapshots_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_DIMENSIONS:
	{
		Dimensions_hover const prev_hover { _dimensions_hover };
		Dimensions_hover       next_hover { Dimensions_hover::NONE };

		node.with_optional_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_optional_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_optional_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("button", [&] (Xml_node const &node_4) {

							if (_has_name(node_4, "Shut down")) {

								next_hover = Dimensions_hover::SHUT_DOWN_BUTTON;
							}
						});
					});
					node_2.with_optional_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_optional_sub_node("hbox", [&] (Xml_node const &node_5) {

								if (_has_name(node_5, "Leave")) {

									next_hover = Dimensions_hover::LEAVE_BUTTON;
								}
							});
							node_4.with_optional_sub_node("vbox", [&] (Xml_node const &node_5) {

								if (_has_name(node_5, "Expand Client FS")) {

									next_hover = Dimensions_hover::EXPAND_CLIENT_FS_BUTTON;

								} else if (_has_name(node_5,
								                     RENAME_SNAPSHOT_BUFFER_JOURNALING_BUFFER ?
								                        "Expand Journaling Buffer" :
								                        "Expand Snapshot Buffer")) {

									next_hover = Dimensions_hover::EXPAND_SNAPSHOT_BUF_BUTTON;
								}
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_dimensions_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_EXPAND_CLIENT_FS:
	{
		Expand_client_fs_hover const prev_hover { _expand_client_fs_hover };
		Expand_client_fs_hover       next_hover { Expand_client_fs_hover::NONE };

		node.with_optional_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_optional_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_optional_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("button", [&] (Xml_node const &node_4) {

							if (_has_name(node_4, "Shut down")) {

								next_hover = Expand_client_fs_hover::SHUT_DOWN_BUTTON;

							}
						});
					});
					node_2.with_optional_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_optional_sub_node("vbox", [&] (Xml_node const &node_5) {
								node_5.with_optional_sub_node("hbox", [&] (Xml_node const &node_6) {

									if (_has_name(node_6, "Leave")) {

										next_hover = Expand_client_fs_hover::LEAVE_BUTTON;
									}
								});
								node_5.with_optional_sub_node("float", [&] (Xml_node const &node_6) {

									if (_has_name(node_6, "Start")) {

										next_hover = Expand_client_fs_hover::START_BUTTON;
									}
								});
								node_5.with_optional_sub_node("frame", [&] (Xml_node const &node_6) {

									if (_has_name(node_6, "Contingent")) {

										next_hover = Expand_client_fs_hover::CONTINGENT_INPUT;
									}
								});
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_expand_client_fs_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
	{
		Expand_snapshot_buf_hover const prev_hover { _expand_snapshot_buf_hover };
		Expand_snapshot_buf_hover       next_hover { Expand_snapshot_buf_hover::NONE };

		node.with_optional_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_optional_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_optional_sub_node("hbox", [&] (Xml_node const &node_3) {

						node_3.with_optional_sub_node("button", [&] (Xml_node const &node_4) {

							if (_has_name(node_4, "Shut down")) {

								next_hover = Expand_snapshot_buf_hover::SHUT_DOWN_BUTTON;

							}
						});
					});
					node_2.with_optional_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_optional_sub_node("vbox", [&] (Xml_node const &node_5) {

								node_5.with_optional_sub_node("hbox", [&] (Xml_node const &node_6) {

									if (_has_name(node_6, "Leave")) {

										next_hover = Expand_snapshot_buf_hover::LEAVE_BUTTON;
									}
								});
								node_5.with_optional_sub_node("float", [&] (Xml_node const &node_6) {

									if (_has_name(node_6, "Start")) {

										next_hover = Expand_snapshot_buf_hover::START_BUTTON;
									}
								});
								node_5.with_optional_sub_node("frame", [&] (Xml_node const &node_6) {

									if (_has_name(node_6, "Contingent")) {

										next_hover = Expand_snapshot_buf_hover::CONTINGENT_INPUT;
									}
								});
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_expand_snapshot_buf_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_SECURITY:
	{
		Controls_security_hover const prev_hover { _controls_security_hover };
		Controls_security_hover       next_hover { Controls_security_hover::NONE };

		node.with_optional_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_optional_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_optional_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("button", [&] (Xml_node const &node_4) {

							if (_has_name(node_4, "Shut down")) {

								next_hover = Controls_security_hover::SHUT_DOWN_BUTTON;

							}
						});
					});
					node_2.with_optional_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_optional_sub_node("hbox", [&] (Xml_node const &node_5) {

								if (_has_name(node_5, "Leave")) {

									next_hover = Controls_security_hover::SECURITY_EXPAND_BUTTON;
								}
							});
							node_4.with_optional_sub_node("vbox", [&] (Xml_node const &node_5) {

								if (_has_name(node_5, "Block Encryption Key")) {

									next_hover = Controls_security_hover::BLOCK_ENCRYPTION_KEY_EXPAND_BUTTON;

								} else if (_has_name(node_5, "Master Key")) {

									next_hover = Controls_security_hover::MASTER_KEY_EXPAND_BUTTON;

								} else if (_has_name(node_5, "User Passphrase")) {

									next_hover = Controls_security_hover::USER_PASSPHRASE_EXPAND_BUTTON;
								}
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_controls_security_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	{
		Controls_security_block_encryption_key_hover const prev_hover { _controls_security_block_encryption_key_hover };
		Controls_security_block_encryption_key_hover       next_hover { Controls_security_block_encryption_key_hover::NONE };

		node.with_optional_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_optional_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_optional_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("button", [&] (Xml_node const &node_4) {

							if (_has_name(node_4, "Shut down")) {

								next_hover = Controls_security_block_encryption_key_hover::SHUT_DOWN_BUTTON;

							}
						});
					});
					node_2.with_optional_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_optional_sub_node("button", [&] (Xml_node const &node_5) {

								if (_has_name(node_5, "Rekey")) {

									next_hover = Controls_security_block_encryption_key_hover::REPLACE_BUTTON;
								}
							});
							node_4.with_optional_sub_node("hbox", [&] (Xml_node const &node_5) {

								if (_has_name(node_5, "Leave")) {

									next_hover = Controls_security_block_encryption_key_hover::LEAVE_BUTTON;
								}
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_controls_security_block_encryption_key_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_SECURITY_MASTER_KEY:
	{
		Controls_security_master_key_hover const prev_hover { _controls_security_master_key_hover };
		Controls_security_master_key_hover       next_hover { Controls_security_master_key_hover::NONE };

		node.with_optional_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_optional_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_optional_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("button", [&] (Xml_node const &node_4) {

							if (_has_name(node_4, "Shut down")) {

								next_hover = Controls_security_master_key_hover::SHUT_DOWN_BUTTON;

							}
						});
					});
					node_2.with_optional_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_optional_sub_node("hbox", [&] (Xml_node const &node_5) {

								if (_has_name(node_5, "Leave")) {

									next_hover = Controls_security_master_key_hover::LEAVE_BUTTON;
								}
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_controls_security_master_key_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:
	{
		Controls_security_user_passphrase_hover const prev_hover { _controls_security_user_passphrase_hover };
		Controls_security_user_passphrase_hover       next_hover { Controls_security_user_passphrase_hover::NONE };

		node.with_optional_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_optional_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_optional_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("button", [&] (Xml_node const &node_4) {

							if (_has_name(node_4, "Shut down")) {

								next_hover = Controls_security_user_passphrase_hover::SHUT_DOWN_BUTTON;

							}
						});
					});
					node_2.with_optional_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_optional_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_optional_sub_node("hbox", [&] (Xml_node const &node_5) {

								if (_has_name(node_5, "Leave")) {

									next_hover = Controls_security_user_passphrase_hover::LEAVE_BUTTON;
								}
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_controls_security_user_passphrase_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	default:

		break;
	}
	if (update_dialog) {
		_dialog.trigger_update();
	}
}


/***********************
 ** Genode::Component **
 ***********************/

void Component::construct(Genode::Env &env)
{
	static File_vault::Main main { env };
}
