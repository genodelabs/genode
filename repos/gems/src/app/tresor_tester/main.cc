/*
 * \brief  Tool for running tests and benchmarks on Tresor library
 * \author Martin Stein
 * \date   2020-08-26
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <timer_session/connection.h>
#include <vfs/simple_env.h>

/* tresor init includes */
#include <tresor_init/configuration.h>

/* tresor includes */
#include <tresor/crypto.h>
#include <tresor/trust_anchor.h>
#include <tresor/client_data.h>
#include <tresor/block_io.h>
#include <tresor/meta_tree.h>
#include <tresor/free_tree.h>
#include <tresor/request_pool.h>
#include <tresor/vbd_initializer.h>
#include <tresor/ft_initializer.h>
#include <tresor/sb_initializer.h>
#include <tresor/sb_check.h>
#include <tresor/vbd_check.h>
#include <tresor/ft_check.h>
#include <tresor/virtual_block_device.h>
#include <tresor/superblock_control.h>

namespace Tresor_tester {

	using namespace Genode;
	using namespace Tresor;

	using Salt = uint64_t;

	class Log_node;
	class Benchmark_node;
	class Benchmark;
	class Trust_anchor_node;
	class Request_node;
	class Command;
	class Snapshot_reference;
	class Snapshot_reference_tree;
	class Client_data;
	class Main;

	template <typename T>
	T read_attribute(Xml_node const &node, char const *attr)
	{
		T value { };
		ASSERT(node.has_attribute(attr));
		ASSERT(node.attribute(attr).value(value));
		return value;
	}
}


struct Tresor_tester::Log_node
{
	using String = Genode::String<128>;

	String const string;

	NONCOPYABLE(Log_node);

	Log_node(Xml_node const &node) : string { node.attribute_value("string", String { }) } { }
};


struct Tresor_tester::Benchmark_node
{
	using Label = String<128>;

	enum Operation { START, STOP };

	Operation const op;
	bool const label_avail;
	Label const label;

	NONCOPYABLE(Benchmark_node);

	Operation read_op_attr(Xml_node const &node)
	{
		ASSERT(node.has_attribute("op"));
		if (node.attribute("op").has_value("start")) return START;
		if (node.attribute("op").has_value("stop")) return STOP;
		ASSERT_NEVER_REACHED;
	}

	Benchmark_node(Xml_node const &node)
	:
		op { read_op_attr(node) }, label_avail { op == START && node.has_attribute("label") },
		label { label_avail ? node.attribute_value("label", Label { }) : Label { } }
	{ }

	Benchmark_node(Operation op, bool label_avail, Label label) : op(op), label_avail(label_avail), label(label) { }
};


class Tresor_tester::Benchmark
{
	private:

		enum State { STARTED, STOPPED };

		Genode::Env &_env;
		Timer::Connection _timer { _env };
		State _state { STOPPED };
		Microseconds _start_time { 0 };
		Number_of_blocks _num_virt_blks_read { 0 };
		Number_of_blocks _num_virt_blks_written { 0 };
		Benchmark_node const *_start_node_ptr { };

		NONCOPYABLE(Benchmark);

	public:

		Benchmark(Genode::Env &env) : _env { env } { }

		void execute_cmd(Benchmark_node const &node)
		{
			switch (node.op) {
			case Benchmark_node::START:

				ASSERT(_state == STOPPED);
				_num_virt_blks_read = 0;
				_num_virt_blks_written = 0;
				_state = STARTED;
				_start_node_ptr = &node;
				_start_time = _timer.curr_time().trunc_to_plain_us();
				break;

			case Benchmark_node::STOP:

				ASSERT(_state == STARTED);
				uint64_t const stop_time_us { _timer.curr_time().trunc_to_plain_us().value };
				log("");
				if (_start_node_ptr->label_avail)
					log("Benchmark result \"", _start_node_ptr->label, "\"");
				else
					log("Benchmark result");

				double const passed_time_sec { (double)(stop_time_us - _start_time.value) / (double)(1000 * 1000) };
				log("   Ran ", passed_time_sec, " seconds.");

				if (_num_virt_blks_read) {

					size_t const bytes_read { _num_virt_blks_read * Tresor::BLOCK_SIZE };
					double const mibyte_read { (double)bytes_read / (double)(1024 * 1024) };
					double const mibyte_per_sec_read {
						(double)bytes_read / (double)passed_time_sec / (double)(1024 * 1024) };

					log("   Have read ", mibyte_read, " mebibyte in total.");
					log("   Have read ", mibyte_per_sec_read, " mebibyte per second.");
				}

				if (_num_virt_blks_written) {

					size_t bytes_written { _num_virt_blks_written * Tresor::BLOCK_SIZE };
					double const mibyte_written { (double)bytes_written / (double)(1024 * 1024) };
					double const mibyte_per_sec_written {
						(double)bytes_written / (double)passed_time_sec / (double)(1024 * 1024) };

					log("   Have written ", mibyte_written, " mebibyte in total.");
					log("   Have written ", mibyte_per_sec_written, " mebibyte per second.");
				}
				log("");
				_state = STOPPED;
				break;
			}
		}

		void raise_num_virt_blks_read() { _num_virt_blks_read++; }
		void raise_num_virt_blks_written() { _num_virt_blks_written++; }
};


struct Tresor_tester::Trust_anchor_node
{
	using Operation = Trust_anchor_request::Type;

	Operation const op;
	Passphrase const passphrase;

	NONCOPYABLE(Trust_anchor_node);

	Operation read_op_attr(Xml_node const &node)
	{
		ASSERT(node.has_attribute("op"));
		if (node.attribute("op").has_value("initialize"))
			return Operation::INITIALIZE;
		ASSERT_NEVER_REACHED;
	}

	Trust_anchor_node(Xml_node const &node)
	:
		op { read_op_attr(node) },
		passphrase { op == Operation::INITIALIZE ? node.attribute_value("passphrase", Passphrase()) : Passphrase() }
	{ }
};


struct Tresor_tester::Request_node
{
	using Operation = Tresor::Request::Operation;

	Operation const op;
	Virtual_block_address const vba;
	Number_of_blocks const count;
	bool const sync;
	bool const salt_avail;
	Salt const salt;
	Snapshot_id const snap_id;

	NONCOPYABLE(Request_node);

	Operation read_op_attr(Xml_node const &node)
	{
		ASSERT(node.has_attribute("op"));
		if (node.attribute("op").has_value("read")) return Operation::READ;
		if (node.attribute("op").has_value("write")) return Operation::WRITE;
		if (node.attribute("op").has_value("sync")) return Operation::SYNC;
		if (node.attribute("op").has_value("create_snapshot")) return Operation::CREATE_SNAPSHOT;
		if (node.attribute("op").has_value("discard_snapshot")) return Operation::DISCARD_SNAPSHOT;
		if (node.attribute("op").has_value("extend_ft")) return Operation::EXTEND_FT;
		if (node.attribute("op").has_value("extend_vbd")) return Operation::EXTEND_VBD;
		if (node.attribute("op").has_value("rekey")) return Operation::REKEY;
		if (node.attribute("op").has_value("deinitialize")) return Operation::DEINITIALIZE;
		ASSERT_NEVER_REACHED;
	}

	Request_node(Xml_node const &node)
	:
		op { read_op_attr(node) },
		vba { has_vba() ? read_attribute<Virtual_block_address>(node, "vba") : 0 },
		count { has_count() ? read_attribute<Number_of_blocks>(node, "count") : 0 },
		sync { read_attribute<bool>(node, "sync") },
		salt_avail { has_salt() ? node.has_attribute("salt") : false },
		salt { has_salt() && salt_avail ? read_attribute<Salt>(node, "salt") : 0 },
		snap_id { has_snap_id() ? read_attribute<Snapshot_id>(node, "id") : 0 }
	{ }

	bool has_vba() const { return op == Operation::READ || op == Operation::WRITE || op == Operation::SYNC; }

	bool has_salt() const { return op == Operation::READ || op == Operation::WRITE; }

	bool has_count() const
	{
		return op == Operation::READ || op == Operation::WRITE || op == Operation::SYNC ||
		       op == Operation::EXTEND_FT || op == Operation::EXTEND_VBD;
	}

	bool has_snap_id() const { return op == Operation::DISCARD_SNAPSHOT || op == Operation::CREATE_SNAPSHOT; }

	void print(Genode::Output &out) const { Genode::print(out, "op ", Tresor::Request::op_to_string(op)); }
};


class Tresor_tester::Command : public Module_channel
{
	public:

		enum Type { INVALID, REQUEST, TRUST_ANCHOR, BENCHMARK, CONSTRUCT, DESTRUCT, INITIALIZE, CHECK, CHECK_SNAPSHOTS, LOG };

		enum State { PENDING, IN_PROGRESS, CREATE_SNAP_COMPLETED, DISCARD_SNAP_COMPLETED, COMPLETED };

	private:

		using Type_string = String<64>;

		Tresor_tester::Main &_main;
		Type _type { INVALID };
		Module_channel_id _id { 0 };
		State _state { PENDING };
		bool _success { false };
		Generation _gen { INVALID_GENERATION };
		bool _data_mismatch { false };
		Constructible<Request_node> _request_node { };
		Constructible<Trust_anchor_node> _trust_anchor_node { };
		Constructible<Benchmark_node> _benchmark_node { };
		Constructible<Log_node> _log_node { };
		Constructible<Tresor_init::Configuration> _initialize { };

		NONCOPYABLE(Command);

		void _generated_req_completed(State_uint state_uint) override;

		static char const *_type_to_string(Type type)
		{
			switch (type) {
			case INITIALIZE: return "initialize";
			case INVALID: return "invalid";
			case REQUEST: return "request";
			case TRUST_ANCHOR: return "trust_anchor";
			case BENCHMARK: return "benchmark";
			case CONSTRUCT: return "construct";
			case DESTRUCT: return "destruct";
			case CHECK: return "check";
			case CHECK_SNAPSHOTS: return "check_snapshots";
			case LOG: return "log";
			}
			ASSERT_NEVER_REACHED;
		}

		static Type _type_from_string(Type_string str)
		{
			if (str == "initialize") { return INITIALIZE; }
			if (str == "request") { return REQUEST; }
			if (str == "trust-anchor") { return TRUST_ANCHOR; }
			if (str == "benchmark") { return BENCHMARK; }
			if (str == "construct") { return CONSTRUCT; }
			if (str == "destruct") { return DESTRUCT; }
			if (str == "check") { return CHECK; }
			if (str == "check-snapshots") { return CHECK_SNAPSHOTS; }
			if (str == "log") { return LOG; }
			ASSERT_NEVER_REACHED;
		}

	public:

		Command(Xml_node const &node, Tresor_tester::Main &main, Module_channel_id id)
		:
			Module_channel { COMMAND_POOL, id }, _main { main }, _type { _type_from_string(node.type()) }, _id { id }
		{
			switch (_type) {
			case INITIALIZE: _initialize.construct(node); break;
			case REQUEST: _request_node.construct(node); break;
			case TRUST_ANCHOR: _trust_anchor_node.construct(node); break;
			case BENCHMARK: _benchmark_node.construct(node); break;
			case LOG: _log_node.construct(node); break;
			default: break;
			}
		}

		bool may_have_data_mismatch() const
		{
			return _type == REQUEST && _request_node->op == Tresor::Request::READ && _request_node->salt_avail;
		}

		bool synchronize() const
		{
			switch (_type) {
			case REQUEST: return _request_node->sync;
			case INVALID: ASSERT_NEVER_REACHED;
			default: return true;
			}
			ASSERT_NEVER_REACHED;
		}

		void print(Genode::Output &out) const
		{
			Genode::print(out, "id ", _id, " type ", _type_to_string(_type));
			if (_type == REQUEST)
				Genode::print(out, " ", *_request_node);
		}

		Type type() const { return _type ; }
		State state() const { return _state ; }
		Module_channel_id id() const { return _id ; }
		bool success() const { return _success ; }
		bool data_mismatch() const { return _data_mismatch ; }
		Request_node const &request_node() const { return *_request_node ; }
		Trust_anchor_node const &trust_anchor_node() const { return *_trust_anchor_node; }
		Benchmark_node const &benchmark_node() const { return *_benchmark_node ; }
		Log_node const &log_node() const { return *_log_node ; }
		Tresor_init::Configuration const &initialize() const { return *_initialize ; }

		void state (State state) { _state = state; }
		void success (bool success) { _success = success; }
		void data_mismatch (bool data_mismatch) { _data_mismatch = data_mismatch; }

		void execute(bool &progress);
};


struct Tresor_tester::Snapshot_reference : public Genode::Avl_node<Snapshot_reference>
{
	Snapshot_id const id;
	Generation const gen;

	NONCOPYABLE(Snapshot_reference);

	Snapshot_reference(Snapshot_id id, Generation gen) : id { id }, gen { gen } { }

	template <typename FUNC>
	void with_ref(Snapshot_id target_id, FUNC && func) const
	{
		if (target_id != id) {
			Snapshot_reference *child_ptr { Avl_node<Snapshot_reference>::child(target_id > id) };
			if (child_ptr)
				child_ptr->with_ref(target_id, func);
			else
				ASSERT_NEVER_REACHED;
		} else
			func(*this);
	}

	void print(Genode::Output &out) const { Genode::print(out, "id ", id, " gen ", gen); }

	bool higher(Snapshot_reference *other_ptr) { return other_ptr->id > id; }
};


struct Tresor_tester::Snapshot_reference_tree : public Avl_tree<Snapshot_reference>
{
	template <typename FUNC>
	void with_ref(Snapshot_id id, FUNC && func) const
	{
		if (first())
			first()->with_ref(id, func);
		else
			ASSERT_NEVER_REACHED;
	}
};


class Tresor_tester::Client_data : public Module, public Module_channel
{
	private:

		using Request = Client_data_request;

		Main &_main;

		NONCOPYABLE(Client_data);

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return true; }

	public:

		Client_data(Main &main) : Module_channel { CLIENT_DATA, 0 }, _main { main } { add_channel(*this); }
};


class Tresor_tester::Main : private Vfs::Env::User, private Module_composition, public Module
{
	private:

		Genode::Env &_env;
		Attached_rom_dataspace _config_rom { _env, "config" };
		Heap _heap { _env.ram(), _env.rm() };
		Vfs::Simple_env _vfs_env { _env, _heap, _config_rom.xml().sub_node("vfs"), *this };
		Signal_handler<Main> _signal_handler { _env.ep(), *this, &Main::_handle_signal };
		Benchmark _benchmark { _env };
		Module_channel_id _next_command_id { 0 };
		unsigned long _num_uncompleted_cmds { 0 };
		unsigned long _num_errors { 0 };
		Tresor::Block _blk_data { };
		Snapshot_reference_tree _snap_refs { };
		Constructible<Free_tree> _free_tree { };
		Constructible<Virtual_block_device> _vbd { };
		Constructible<Superblock_control> _sb_control { };
		Constructible<Request_pool> _request_pool { };
		Constructible<Client_data> _client_data { };
		Constructible<Meta_tree> _meta_tree { };
		Trust_anchor _trust_anchor { _vfs_env, _config_rom.xml().sub_node("trust-anchor") };
		Crypto _crypto { _vfs_env, _config_rom.xml().sub_node("crypto") };
		Block_io _block_io { _vfs_env, _config_rom.xml().sub_node("block-io") };
		Pba_allocator _pba_alloc { NR_OF_SUPERBLOCK_SLOTS };
		Vbd_initializer _vbd_initializer { };
		Ft_initializer _ft_initializer { };
		Sb_initializer _sb_initializer { };
		Sb_check _sb_check { };
		Vbd_check _vbd_check { };
		Ft_check _ft_check { };
		bool _generated_req_success { false };

		NONCOPYABLE(Main);

		static void _generate_blk_data(Tresor::Block &blk_data, Virtual_block_address vba, Salt salt)
		{
			for (uint64_t idx { 0 }; idx + sizeof(vba) + sizeof(salt) <= BLOCK_SIZE; ) {

				memcpy(&blk_data.bytes[idx], &vba, sizeof(vba));
				idx += sizeof(vba);
				memcpy(&blk_data.bytes[idx], &salt, sizeof(salt));
				idx += sizeof(salt);
				vba += idx + salt;
				salt += idx + vba;
			}
		}

		template <typename FUNC>
		void _with_first_processable_cmd(FUNC && func)
		{
			bool first_uncompleted_cmd { true };
			bool done { false };
			for_each_channel<Command>([&] (Command &cmd)
			{
				if (done)
					return;

				if (cmd.state() == Command::PENDING) {
					done = true;
					if (first_uncompleted_cmd || !cmd.synchronize())
						func(cmd);
				}
				if (cmd.state() == Command::IN_PROGRESS) {
					if (cmd.synchronize())
						done = true;
					else
						first_uncompleted_cmd = false;
				}
			});
		}

		void _try_end_program()
		{
			if (_num_uncompleted_cmds == 0) {
				if (_num_errors > 0) {
					for_each_channel<Command>([&] (Command &cmd) {
						if (cmd.state() != Command::COMPLETED)
							return;

						if (cmd.success() && (!cmd.may_have_data_mismatch() || !cmd.data_mismatch()))
							return;

						log("cmd failed: ", cmd);
					});
					_env.parent().exit(-1);
				} else
					_env.parent().exit(0);
			}
		}

		void _wakeup_back_end_services() { _vfs_env.io().commit(); }

		void _handle_signal()
		{
			execute_modules();
			_try_end_program();
			_wakeup_back_end_services();
		}

		void wakeup_vfs_user() override { _signal_handler.local_submit(); }

		void execute(bool &progress) override
		{
			_with_first_processable_cmd([&] (Command &cmd) {
				cmd.execute(progress); });
		}

		void _remove_snap_ref(Snapshot_reference &ref)
		{
			_snap_refs.remove(&ref);
			ref.~Snapshot_reference();
			destroy(_heap, &ref);
		}

	public:

		Main(Genode::Env &env) : _env { env }
		{
			add_module(CRYPTO, _crypto);
			add_module(TRUST_ANCHOR, _trust_anchor);
			add_module(COMMAND_POOL, *this);
			add_module(BLOCK_IO, _block_io);
			add_module(VBD_INITIALIZER, _vbd_initializer);
			add_module(FT_INITIALIZER, _ft_initializer);
			add_module(SB_INITIALIZER, _sb_initializer);
			add_module(SB_CHECK, _sb_check);
			add_module(VBD_CHECK, _vbd_check);
			add_module(FT_CHECK, _ft_check);
			_config_rom.xml().sub_node("commands").for_each_sub_node([&] (Xml_node const &node) {
				add_channel(*new (_heap) Command(node, *this, _next_command_id++));
				_num_uncompleted_cmds++;
			});
			_handle_signal();
		}

		void mark_command_in_progress(Module_channel_id cmd_id)
		{
			with_channel<Command>(cmd_id, [&] (Command &cmd) {
				ASSERT(cmd.state() == Command::PENDING);
				cmd.state(Command::IN_PROGRESS);
			});
		}

		void mark_command_completed(Module_channel_id cmd_id, bool success)
		{
			with_channel<Command>(cmd_id, [&] (Command &cmd) {
				ASSERT(cmd.state() == Command::IN_PROGRESS);
				cmd.state(Command::COMPLETED);
				_num_uncompleted_cmds--;
				cmd.success(success);
				if (!cmd.success()) {
					warning("cmd ", cmd, " failed");
					_num_errors++;
				}
			});
		}

		Generation snap_id_to_gen(Snapshot_id id)
		{
			Generation gen { INVALID_GENERATION };
			_snap_refs.with_ref(id, [&] (Snapshot_reference const &ref) {
				gen = ref.gen; });

			return gen;
		}

		void add_snap_ref(Snapshot_id id, Generation gen) { _snap_refs.insert(new (_heap) Snapshot_reference { id, gen }); }

		void remove_snap_refs_with_same_gen(Snapshot_id id)
		{
			Generation gen { snap_id_to_gen(id) };
			while (1) {
				Snapshot_reference *ref_ptr { nullptr };
				_snap_refs.for_each([&] (Snapshot_reference const &ref) {
					if (!ref_ptr && ref.gen == gen)
						ref_ptr = const_cast<Snapshot_reference *>(&ref);
				});
				if (ref_ptr)
					_remove_snap_ref(*ref_ptr);
				else
					break;
			}
		}

		void reset_snap_refs()
		{
			while (_snap_refs.first())
				_remove_snap_ref(*_snap_refs.first());
		}

		Pba_allocator &pba_alloc() { return _pba_alloc; }

		void generate_blk_data(Request_tag tresor_req_tag, Virtual_block_address vba, Tresor::Block &blk_data)
		{
			with_channel<Command>(tresor_req_tag, [&] (Command &cmd) {
				ASSERT(cmd.type() == Command::REQUEST);
				Request_node const &req_node { cmd.request_node() };
				if (req_node.salt_avail)
					_generate_blk_data(blk_data, vba, req_node.salt);
			});
			_benchmark.raise_num_virt_blks_written();
		}

		void verify_blk_data(Request_tag tresor_req_tag, Virtual_block_address vba, Tresor::Block &blk_data)
		{
			with_channel<Command>(tresor_req_tag, [&] (Command &cmd) {
				ASSERT(cmd.type() == Command::REQUEST);
				Request_node const &req_node { cmd.request_node() };
				if (req_node.salt_avail) {
					Tresor::Block gen_blk_data { };
					_generate_blk_data(gen_blk_data, vba, req_node.salt);

					if (memcmp(&blk_data, &gen_blk_data, BLOCK_SIZE)) {
						cmd.data_mismatch(true);
						warning("client data mismatch: vba=", vba, " req_tag=", tresor_req_tag);
						_num_errors++;
					}
				}
			});
			_benchmark.raise_num_virt_blks_read();
		}

		void construct_tresor_modules()
		{
			_free_tree.construct();
			_vbd.construct();
			_sb_control.construct();
			_request_pool.construct();
			_client_data.construct(*this);
			_meta_tree.construct();
			add_module(FREE_TREE, *_free_tree);
			add_module(VIRTUAL_BLOCK_DEVICE, *_vbd);
			add_module(SUPERBLOCK_CONTROL, *_sb_control);
			add_module(REQUEST_POOL, *_request_pool);
			add_module(CLIENT_DATA, *_client_data);
			add_module(META_TREE, *_meta_tree);
		}

		void destruct_tresor_modules()
		{
			remove_module(META_TREE);
			remove_module(CLIENT_DATA);
			remove_module(REQUEST_POOL);
			remove_module(SUPERBLOCK_CONTROL);
			remove_module(VIRTUAL_BLOCK_DEVICE);
			remove_module(FREE_TREE);
			_meta_tree.destruct();
			_client_data.destruct();
			_request_pool.destruct();
			_sb_control.destruct();
			_vbd.destruct();
			_free_tree.destruct();
		}

		void check_snapshots(Command &cmd, bool &progress)
		{
			mark_command_in_progress(cmd.id());
			bool success { true };
			Snapshots_info snap_info { _sb_control->snapshots_info() };
			bool snap_gen_ok[MAX_NR_OF_SNAPSHOTS] { false };
			_snap_refs.for_each([&] (Snapshot_reference const &snap_ref) {
				bool snap_ref_ok { false };
				for (Snapshot_index idx { 0 }; idx < MAX_NR_OF_SNAPSHOTS; idx++) {
					if (snap_info.generations[idx] == snap_ref.gen) {
						snap_ref_ok = true;
						snap_gen_ok[idx] = true;
					}
				}
				if (!snap_ref_ok) {
					warning("snap (", snap_ref, ") not known to tresor");
					_num_errors++;
					success = false;
				}
			});
			for (Snapshot_index idx { 0 }; idx < MAX_NR_OF_SNAPSHOTS; idx++) {
				if (snap_info.generations[idx] != INVALID_GENERATION && !snap_gen_ok[idx]) {
					warning("snap (idx ", idx, " gen ", snap_info.generations[idx], ") not known to tester");
					_num_errors++;
					success = false;
				}
			}
			mark_command_completed(cmd.id(), success);
			progress = true;
		}

		Benchmark &benchmark() { return _benchmark; }
};


void Tresor_tester::Client_data::_request_submitted(Module_request &mod_req)
{
	Request &req { *static_cast<Request *>(&mod_req) };
	switch (req._type) {
	case Request::OBTAIN_PLAINTEXT_BLK: _main.generate_blk_data(req._req_tag, req._vba, req._blk); break;
	case Request::SUPPLY_PLAINTEXT_BLK: _main.verify_blk_data(req._req_tag, req._vba, req._blk); break;
	}
	req._success = true;
}


void Tresor_tester::Command::_generated_req_completed(State_uint state_uint)
{
	if (state_uint == CREATE_SNAP_COMPLETED)
		_main.add_snap_ref(request_node().snap_id, _gen);

	if (state_uint == DISCARD_SNAP_COMPLETED)
		_main.remove_snap_refs_with_same_gen(request_node().snap_id);

	_main.mark_command_completed(id(), _success);
}


void Tresor_tester::Command::execute(bool &progress)
{
	switch (type()) {
	case REQUEST:
	{
		Request_node const &node { request_node() };
		State state { COMPLETED };
		_gen = INVALID_GENERATION;
		if (node.op == Request::DISCARD_SNAPSHOT) {
			_gen = _main.snap_id_to_gen(node.snap_id);
			state = DISCARD_SNAP_COMPLETED;
		}
		if (node.op == Request::CREATE_SNAPSHOT)
			state = CREATE_SNAP_COMPLETED;

		generate_req<Tresor::Request>(
			state, progress, node.op, node.has_vba() ? node.vba : 0,
			0, node.has_count() ? node.count : 0, 0, id(), _gen, _success);

		_main.mark_command_in_progress(id());
		break;
	}
	case Command::TRUST_ANCHOR:
	{
		Trust_anchor_node const &node { trust_anchor_node() };
		ASSERT(node.op == Trust_anchor_request::INITIALIZE);
		generate_req<Trust_anchor::Initialize>(COMPLETED, progress, node.passphrase, _success);
		_main.mark_command_in_progress(id());
		break;
	}
	case Command::INITIALIZE:
	{
		_main.reset_snap_refs();
		Tresor_init::Configuration const &cfg { initialize() };
		generate_req<Sb_initializer_request>(COMPLETED, progress,
			(Tree_level_index)(cfg.vbd_nr_of_lvls() - 1),
			(Tree_degree)cfg.vbd_nr_of_children(), cfg.vbd_nr_of_leafs(),
			(Tree_level_index)cfg.ft_nr_of_lvls() - 1,
			(Tree_degree)cfg.ft_nr_of_children(), cfg.ft_nr_of_leafs(),
			(Tree_level_index)cfg.ft_nr_of_lvls() - 1,
			(Tree_degree)cfg.ft_nr_of_children(), cfg.ft_nr_of_leafs(), _main.pba_alloc(), _success);
		_main.mark_command_in_progress(id());
		break;
	}
	case Command::CHECK:
		generate_req<Sb_check_request>(COMPLETED, progress, _success);
		_main.mark_command_in_progress(id());
		break;
	case LOG:
		log("\n", log_node().string, "\n");
		_main.mark_command_in_progress(id());
		_main.mark_command_completed(id(), true);
		progress = true;
		break;
	case BENCHMARK:
		_main.benchmark().execute_cmd(benchmark_node());
		_main.mark_command_in_progress(id());
		_main.mark_command_completed(id(), true);
		progress = true;
		break;
	case CONSTRUCT:
		_main.construct_tresor_modules();
		_main.mark_command_in_progress(id());
		_main.mark_command_completed(id(), true);
		progress = true;
		break;
	case DESTRUCT:
		_main.destruct_tresor_modules();
		_main.mark_command_in_progress(id());
		_main.mark_command_completed(id(), true);
		progress = true;
		break;
	case CHECK_SNAPSHOTS: _main.check_snapshots(*this, progress); break;
	default: break;
	}
}


void Component::construct(Genode::Env &env) { static Tresor_tester::Main main(env); }


namespace Libc {

	struct Env;
	struct Component { void construct(Libc::Env &) { } };
}
