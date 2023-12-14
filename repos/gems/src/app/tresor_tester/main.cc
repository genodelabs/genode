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
#include <tresor/client_data_interface.h>
#include <tresor/block_io.h>
#include <tresor/meta_tree.h>
#include <tresor/free_tree.h>
#include <tresor/vbd_initializer.h>
#include <tresor/ft_initializer.h>
#include <tresor/sb_initializer.h>
#include <tresor/sb_check.h>
#include <tresor/vbd_check.h>
#include <tresor/ft_check.h>
#include <tresor/virtual_block_device.h>
#include <tresor/superblock_control.h>

namespace Tresor_tester {

	enum { VERBOSE = 0 };

	using namespace Genode;
	using namespace Tresor;

	using Salt = uint64_t;

	template <typename> class Schedule;
	class Log_node;
	class Start_benchmark_node;
	class Benchmark;
	class Command;
	class Initialize_trust_anchor_node;
	class Snapshot_reference;
	class Snapshot_reference_tree;
	class Request_node;
	class Main;
}

template <typename T>
class Tresor_tester::Schedule : Noncopyable
{
	private:

		T *_tail { };
		List<T> _list { };

	public:

		using Item = List<T>::Element;

		void add_tail(T &request)
		{
			_list.insert(&request, _tail);
			_tail = &request;
		}

		bool empty() const { return !_list.first(); }

		template <typename FN>
		void with_head(FN && fn)
		{
			if (_list.first())
				fn(*_list.first());
		}

		void remove_head()
		{
			T *head = _list.first();
			if (!head)
				return;

			_list.remove(head);
			if (_tail == head)
				_tail = _list.first();
		}

		template <typename CAN_YIELD_TO_FN>
		void try_yield_head(CAN_YIELD_TO_FN && can_yield_to)
		{
			T *head = _list.first();
			if (!head)
				return;

			T *next = head->Item::_next;
			if (!next || !can_yield_to(*next))
				return;

			remove_head();
			_list.insert(head, next);
		}
};

class Tresor_tester::Benchmark : Noncopyable
{
	public:

		using Label = String<128>;

	private:

		Timer::Connection &_timer;
		Label const _label;
		Microseconds const _start_time;
		Number_of_blocks _num_virt_blks_read { };
		Number_of_blocks _num_virt_blks_written { };

		/*
		 * Noncopyable
		 */
		Benchmark(Benchmark const &) = delete;
		Benchmark &operator = (Benchmark const &) = delete;

	public:

		Benchmark(Timer::Connection &timer, Label const &label)
		:
			_timer(timer), _label(label), _start_time(timer.curr_time().trunc_to_plain_us())
		{ }

		~Benchmark()
		{
			uint64_t const stop_time_us { _timer.curr_time().trunc_to_plain_us().value };
			log("");
			log("Benchmark result \"", _label, "\"");

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
		}

		void raise_num_virt_blks_read() { _num_virt_blks_read++; }
		void raise_num_virt_blks_written() { _num_virt_blks_written++; }
};

struct Tresor_tester::Start_benchmark_node : Noncopyable
{
	Benchmark::Label const label;

	Start_benchmark_node(Xml_node const &node) : label(node.attribute_value("label", Benchmark::Label())) { }
};

struct Tresor_tester::Log_node : Noncopyable
{
	using String = Genode::String<128>;

	String const string;

	Log_node(Xml_node const &node) : string(node.attribute_value("string", String())) { }
};

struct Tresor_tester::Initialize_trust_anchor_node : Noncopyable
{
	Passphrase const passphrase;

	Initialize_trust_anchor_node(Xml_node const &node)
	:
		passphrase(node.attribute_value("passphrase", Passphrase()))
	{ }
};

struct Tresor_tester::Request_node : Noncopyable
{
	enum Operation {
		READ, WRITE, SYNC, CREATE_SNAPSHOT, DISCARD_SNAPSHOT, REKEY, EXTEND_VBD,
		EXTEND_FREE_TREE, DEINITIALIZE, INITIALIZE };

	Operation const op;
	Virtual_block_address const vba;
	Number_of_blocks const num_blocks;
	bool const sync;
	bool const salt_avail;
	Salt const salt;
	Snapshot_id const snap_id;

	Operation read_op_attr(Xml_node const &node)
	{
		ASSERT(node.has_attribute("op"));
		if (node.attribute("op").has_value("read")) return Operation::READ;
		if (node.attribute("op").has_value("write")) return Operation::WRITE;
		if (node.attribute("op").has_value("sync")) return Operation::SYNC;
		if (node.attribute("op").has_value("create_snapshot")) return Operation::CREATE_SNAPSHOT;
		if (node.attribute("op").has_value("discard_snapshot")) return Operation::DISCARD_SNAPSHOT;
		if (node.attribute("op").has_value("extend_ft")) return Operation::EXTEND_FREE_TREE;
		if (node.attribute("op").has_value("extend_vbd")) return Operation::EXTEND_VBD;
		if (node.attribute("op").has_value("rekey")) return Operation::REKEY;
		if (node.attribute("op").has_value("deinitialize")) return Operation::DEINITIALIZE;
		ASSERT_NEVER_REACHED;
	}

	Request_node(Xml_node const &node)
	:
		op(read_op_attr(node)),
		vba(node.attribute_value("vba", (Virtual_block_address)0)),
		num_blocks(node.attribute_value("num_blocks", (Number_of_blocks)0)),
		sync(node.attribute_value("sync", false)),
		salt_avail(node.has_attribute("salt")),
		salt(node.attribute_value("salt", (Salt)0)),
		snap_id(node.attribute_value("id", (Snapshot_id)0))
	{ }
};

struct Tresor_tester::Command : Avl_node<Command>, Schedule<Command>::Item
{
	friend class Schedule<Command>;

	using Node_type = String<64>;
	using Id = uint64_t;

	enum Type {
		REQUEST, INIT_TRUST_ANCHOR, START_BENCHMARK, FINISH_BENCHMARK, CONSTRUCT, DESTRUCT, INITIALIZE, CHECK,
		CHECK_SNAPSHOTS, LOG };

	enum State { INIT, IN_PROGRESS, COMPLETE };

	/*
	 * Noncopyable
	 */
	Command(Command const &) = delete;
	Command &operator = (Command const &) = delete;

	Type const type;
	Id const id;
	State state { INIT };
	Constructible<Request_node> request_node { };
	Constructible<Initialize_trust_anchor_node> init_trust_anchor_node { };
	Constructible<Start_benchmark_node> start_benchmark_node { };
	Constructible<Log_node> log_node { };
	Constructible<Tresor_init::Configuration> initialize_config { };
	Trust_anchor::Initialize *init_trust_anchor_ptr { };
	Sb_initializer::Initialize *init_superblocks_ptr { };
	Sb_check::Check *check_superblocks_ptr { };
	Superblock_control::Initialize *init_sb_control_ptr { };
	Superblock_control::Deinitialize *deinit_sb_control_ptr { };
	Superblock_control::Create_snapshot *create_snap_ptr { };
	Superblock_control::Discard_snapshot *discard_snap_ptr { };
	Superblock_control::Write_vbas *write_vbas_ptr { };
	Superblock_control::Read_vbas *read_vbas_ptr { };
	Superblock_control::Rekey *rekey_ptr { };
	Superblock_control::Extend_vbd *extend_vbd_ptr { };
	Superblock_control::Extend_free_tree *extend_free_tree_ptr { };
	Superblock_control::Synchronize *sync_ptr { };
	Superblock::State sb_state { Superblock::INVALID };
	Generation generation { };
	bool rekey_finished { };
	bool extend_vbd_finished { };
	bool extend_free_tree_finished { };
	bool restarted { };

	char const *op_to_string() const
	{
		switch (type) {
		case INITIALIZE: return "initialize tresor container";
		case REQUEST:
			switch(request_node->op) {
			case Request_node::INITIALIZE: return "initialize superblock control";
			case Request_node::DEINITIALIZE: return "deinitialize superblock control";
			case Request_node::CREATE_SNAPSHOT: return "create snapshot";
			case Request_node::DISCARD_SNAPSHOT: return "discard snapshot";
			case Request_node::READ: return "read";
			case Request_node::WRITE: return "write";
			case Request_node::SYNC: return "sync";
			case Request_node::REKEY: return "rekey";
			case Request_node::EXTEND_VBD: return "extend virtual block device";
			case Request_node::EXTEND_FREE_TREE: return "extend free tree";
			}
			ASSERT_NEVER_REACHED;
		case INIT_TRUST_ANCHOR: return "initialize trust anchor";
		case START_BENCHMARK: return "start benchmark";
		case FINISH_BENCHMARK: return "finish benchmark";
		case CONSTRUCT: return "construct";
		case DESTRUCT: return "destruct";
		case CHECK: return "check";
		case CHECK_SNAPSHOTS: return "check_snapshots";
		case LOG: return "log";
		}
		ASSERT_NEVER_REACHED;
	}

	static Type type_from_node(Xml_node const &node)
	{
		Xml_node::Type const nt = node.type();
		if (nt == "initialize") { return INITIALIZE; }
		if (nt == "request") { return REQUEST; }
		if (nt == "initialize-trust-anchor") { return INIT_TRUST_ANCHOR; }
		if (nt == "start-benchmark") { return START_BENCHMARK; }
		if (nt == "finish-benchmark") { return FINISH_BENCHMARK; }
		if (nt == "construct") { return CONSTRUCT; }
		if (nt == "destruct") { return DESTRUCT; }
		if (nt == "check") { return CHECK; }
		if (nt == "check-snapshots") { return CHECK_SNAPSHOTS; }
		if (nt == "log") { return LOG; }
		ASSERT_NEVER_REACHED;
	}

	bool higher(Command *other_ptr) { return other_ptr->id > id; }

	Command(Xml_node const &node, Id id) : type(type_from_node(node)), id(id)
	{
		switch (type) {
		case INITIALIZE: initialize_config.construct(node); break;
		case REQUEST: request_node.construct(node); break;
		case INIT_TRUST_ANCHOR: init_trust_anchor_node.construct(node); break;
		case START_BENCHMARK: start_benchmark_node.construct(node); break;
		case LOG: log_node.construct(node); break;
		default: break;
		}
	}

	template <typename FUNC>
	void with_command(Command::Id id, FUNC && func)
	{
		if (id != this->id) {
			Command *cmd_ptr { Avl_node<Command>::child(id > this->id) };
			ASSERT(cmd_ptr);
			cmd_ptr->with_command(id, func);
		} else
			func(*this);
	}

	void print(Genode::Output &out) const { Genode::print(out, "id ", id, " op \"", op_to_string(), "\""); }
};

struct Tresor_tester::Snapshot_reference : Avl_node<Snapshot_reference>
{
	Snapshot_id const id;
	Generation const gen;

	Snapshot_reference(Snapshot_id id, Generation gen) : id(id), gen(gen) { }

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

class Tresor_tester::Main : Vfs::Env::User, Client_data_interface, Crypto_key_files_interface
{
	private:

		struct Crypto_key
		{
			Key_id const key_id;
			Vfs::Vfs_handle &encrypt_file;
			Vfs::Vfs_handle &decrypt_file;
		};

		Genode::Env &_env;
		Attached_rom_dataspace _config_rom { _env, "config" };
		Heap _heap { _env.ram(), _env.rm() };
		Vfs::Simple_env _vfs_env { _env, _heap, _config_rom.xml().sub_node("vfs"), *this };
		Signal_handler<Main> _signal_handler { _env.ep(), *this, &Main::_handle_signal };
		Tresor::Path const _crypto_path { _config_rom.xml().sub_node("crypto").attribute_value("path", Tresor::Path()) };
		Tresor::Path const _block_io_path { _config_rom.xml().sub_node("block-io").attribute_value("path", Tresor::Path()) };
		Tresor::Path const _trust_anchor_path { _config_rom.xml().sub_node("trust-anchor").attribute_value("path", Tresor::Path()) };
		Vfs::Vfs_handle &_block_io_file { open_file(_vfs_env, _block_io_path, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_crypto_add_key_file { open_file(_vfs_env, { _crypto_path, "/add_key" }, Vfs::Directory_service::OPEN_MODE_WRONLY) };
		Vfs::Vfs_handle &_crypto_remove_key_file { open_file(_vfs_env, { _crypto_path, "/remove_key" }, Vfs::Directory_service::OPEN_MODE_WRONLY) };
		Vfs::Vfs_handle &_ta_decrypt_file { open_file(_vfs_env, { _trust_anchor_path, "/decrypt" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_encrypt_file { open_file(_vfs_env, { _trust_anchor_path, "/encrypt" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_generate_key_file { open_file(_vfs_env, { _trust_anchor_path, "/generate_key" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_initialize_file { open_file(_vfs_env, { _trust_anchor_path, "/initialize" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_hash_file { open_file(_vfs_env, { _trust_anchor_path, "/hash" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Timer::Connection _timer { _env };
		Constructible<Benchmark> _benchmark { };
		Avl_tree<Command> _command_tree { };
		Schedule<Command> _command_schedule { };
		unsigned long _num_errors { 0 };
		Snapshot_reference_tree _snap_refs { };
		Constructible<Free_tree> _free_tree { };
		Constructible<Virtual_block_device> _vbd { };
		Constructible<Superblock_control> _sb_control { };
		Constructible<Meta_tree> _meta_tree { };
		Trust_anchor _trust_anchor { { _ta_decrypt_file, _ta_encrypt_file, _ta_generate_key_file, _ta_initialize_file, _ta_hash_file } };
		Crypto _crypto { {*this, _crypto_add_key_file, _crypto_remove_key_file} };
		Block_io _block_io { _block_io_file };
		Pba_allocator _pba_alloc { NR_OF_SUPERBLOCK_SLOTS };
		Vbd_initializer _vbd_initializer { };
		Ft_initializer _ft_initializer { };
		Sb_initializer _sb_initializer { };
		Vbd_check _vbd_check { };
		Ft_check _ft_check { };
		Sb_check _sb_check { };
		Constructible<Crypto_key> _crypto_keys[2] { };

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

		Constructible<Crypto_key> &_crypto_key(Key_id key_id)
		{
			for (Constructible<Crypto_key> &key : _crypto_keys)
				if (key.constructed() && key->key_id == key_id)
					return key;
			ASSERT_NEVER_REACHED;
		}

		void _wakeup_back_end_services() { _vfs_env.io().commit(); }

		void _remove_snap_ref(Snapshot_reference &ref)
		{
			_snap_refs.remove(&ref);
			ref.~Snapshot_reference();
			destroy(_heap, &ref);
		}

		void _reset_snap_refs()
		{
			while (_snap_refs.first())
				_remove_snap_ref(*_snap_refs.first());
		}

		void _remove_snap_refs_with_same_gen(Snapshot_id id)
		{
			Generation gen { _snap_id_to_gen(id) };
			while (1) {
				Snapshot_reference *ref_ptr { nullptr };
				_snap_refs.for_each([&] (Snapshot_reference const &ref) {
					if (!ref_ptr && ref.gen == gen)
						ref_ptr = const_cast<Snapshot_reference *>(&ref);
				});
				if (ref_ptr) {
					_remove_snap_ref(*ref_ptr);
				} else
					break;
			}
		}

		void _mark_command_complete(Command &cmd, bool success)
		{
			cmd.state = Command::COMPLETE;
			if (VERBOSE)
				log("finished command: ", cmd);
			if (!success) {
				_num_errors++;
				error("command failed: ", cmd);
			}
		}

		void _mark_command_in_progress(Command &cmd, bool &progress)
		{
			cmd.state = Command::IN_PROGRESS;
			progress = true;
			if (VERBOSE && !cmd.restarted) {
				log("started command: ", cmd);
				cmd.restarted = true;
			}
		}

		template <typename REQUEST>
		void _try_complete_command(Command &cmd, REQUEST &req, bool &progress)
		{
			if (!req.complete())
				return;

			_mark_command_complete(cmd, req.success());
			destroy(_heap, &req);
			progress = true;
			return;
		}

		template <typename REQUEST>
		void _try_complete_multi_request_command(Command &cmd, REQUEST &req, bool cmd_finished, bool &progress)
		{
			if (!req.complete())
				return;

			if (!req.success()) {
				_mark_command_complete(cmd, false);
				destroy(_heap, &req);
				progress = true;
				return;
			}
			destroy(_heap, &req);
			if (cmd_finished) {
				_mark_command_complete(cmd, true);
				return;
			}
			cmd.state = Command::INIT;
			progress = true;
			return;
		}

		bool _execute_command(Command &cmd)
		{
			bool progress = false;
			switch (cmd.type) {
			case Command::INIT_TRUST_ANCHOR:

				switch(cmd.state) {
				case Command::INIT:
				{
					Initialize_trust_anchor_node &node { *cmd.init_trust_anchor_node };
					cmd.init_trust_anchor_ptr = new (_heap) Trust_anchor::Initialize({node.passphrase});
					_mark_command_in_progress(cmd, progress);
					break;
				}
				case Command::IN_PROGRESS:
				{
					Trust_anchor::Initialize &req = *cmd.init_trust_anchor_ptr;
					progress |= _trust_anchor.execute(req);
					_try_complete_command(cmd, req, progress);
					break;
				}
				default: break;
				}
				break;

			case Command::INITIALIZE:

				switch(cmd.state) {
				case Command::INIT:
				{
					_reset_snap_refs();
					Tresor_init::Configuration const &cfg { *cmd.initialize_config };
					cmd.init_superblocks_ptr = new (_heap) Sb_initializer::Initialize({
						Tree_configuration {
							(Tree_level_index)(cfg.vbd_nr_of_lvls() - 1),
							(Tree_degree)cfg.vbd_nr_of_children(),
							cfg.vbd_nr_of_leafs()
						},
						Tree_configuration {
							(Tree_level_index)cfg.ft_nr_of_lvls() - 1,
							(Tree_degree)cfg.ft_nr_of_children(),
							cfg.ft_nr_of_leafs()
						},
						Tree_configuration {
							(Tree_level_index)cfg.ft_nr_of_lvls() - 1,
							(Tree_degree)cfg.ft_nr_of_children(),
							cfg.ft_nr_of_leafs()
						},
						_pba_alloc
					});
					_mark_command_in_progress(cmd, progress);
					break;
				}
				case Command::IN_PROGRESS:
				{
					Sb_initializer::Initialize &req = *cmd.init_superblocks_ptr;
					progress |= _sb_initializer.execute(req, _block_io, _trust_anchor, _vbd_initializer, _ft_initializer);
					_try_complete_command(cmd, req, progress);
					break;
				}
				default: break;
				}
				break;

			case Command::CONSTRUCT:

				switch(cmd.state) {
				case Command::INIT:

					_meta_tree.construct();
					_free_tree.construct();
					_vbd.construct();
					_sb_control.construct();
					cmd.init_sb_control_ptr = new (_heap) Superblock_control::Initialize({cmd.sb_state});
					_mark_command_in_progress(cmd, progress);
					break;

				case Command::IN_PROGRESS:
				{
					Superblock_control::Initialize &req = *cmd.init_sb_control_ptr;
					progress |= _sb_control->execute(req, _block_io, _crypto, _trust_anchor);
					_try_complete_command(cmd, req, progress);
					break;
				}
				default: break;
				}
				break;

			case Command::DESTRUCT:

				switch(cmd.state) {
				case Command::INIT:

					_meta_tree.destruct();
					_free_tree.destruct();
					_vbd.destruct();
					_sb_control.destruct();
					_mark_command_complete(cmd, true);
					progress = true;
					break;

				default: break;
				}
				break;

			case Command::START_BENCHMARK:

				switch(cmd.state) {
				case Command::INIT:

					_benchmark.construct(_timer, cmd.start_benchmark_node->label);
					_mark_command_complete(cmd, true);
					progress = true;
					break;

				default: break;
				}
				break;

			case Command::FINISH_BENCHMARK:

				switch(cmd.state) {
				case Command::INIT:

					_benchmark.destruct();
					_mark_command_complete(cmd, true);
					progress = true;
					break;

				default: break;
				}
				break;

			case Command::CHECK_SNAPSHOTS:

				switch(cmd.state) {
				case Command::INIT:
				{
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
							success = false;
						}
					});
					for (Snapshot_index idx { 0 }; idx < MAX_NR_OF_SNAPSHOTS; idx++) {
						if (snap_info.generations[idx] != INVALID_GENERATION && !snap_gen_ok[idx]) {
							warning("snap (idx ", idx, " gen ", snap_info.generations[idx], ") not known to tester");
							success = false;
						}
					}
					_mark_command_complete(cmd, success);
					progress = true;
					break;
				}
				default: break;
				}
				break;

			case Command::REQUEST:

				switch(cmd.state) {
				case Command::INIT:
				{
					Request_node const &node { *cmd.request_node };
					switch (node.op) {
					case Request_node::WRITE:
						cmd.write_vbas_ptr = new (_heap) Superblock_control::Write_vbas({node.vba, node.num_blocks, 0, cmd.id});
						break;
					case Request_node::READ:
						cmd.read_vbas_ptr = new (_heap) Superblock_control::Read_vbas({node.vba, node.num_blocks, 0, cmd.id});
						break;
					case Request_node::SYNC:
						cmd.sync_ptr = new (_heap) Superblock_control::Synchronize({});
						break;
					case Request_node::REKEY:
						cmd.rekey_ptr = new (_heap) Superblock_control::Rekey({cmd.rekey_finished});
						break;
					case Request_node::EXTEND_VBD:
						cmd.extend_vbd_ptr = new (_heap) Superblock_control::Extend_vbd({node.num_blocks, cmd.extend_vbd_finished});
						break;
					case Request_node::EXTEND_FREE_TREE:
						cmd.extend_free_tree_ptr = new (_heap) Superblock_control::Extend_free_tree({node.num_blocks, cmd.extend_free_tree_finished});
						break;
					case Request_node::DEINITIALIZE:
						cmd.deinit_sb_control_ptr = new (_heap) Superblock_control::Deinitialize({});
						break;
					case Request_node::CREATE_SNAPSHOT:
						cmd.create_snap_ptr = new (_heap) Superblock_control::Create_snapshot({cmd.generation});
						break;
					case Request_node::DISCARD_SNAPSHOT:
						cmd.discard_snap_ptr = new (_heap) Superblock_control::Discard_snapshot({_snap_id_to_gen(node.snap_id)});
						break;
					default: ASSERT_NEVER_REACHED;
					}
					_mark_command_in_progress(cmd, progress);
					break;
				}
				case Command::IN_PROGRESS:
				{
					Request_node &node = *cmd.request_node;
					switch (node.op) {
					case Request_node::WRITE:
					{
						Superblock_control::Write_vbas &req = *cmd.write_vbas_ptr;
						progress |= _sb_control->execute(req, *_vbd, *this, _block_io, *_free_tree, *_meta_tree, _crypto);
						_try_complete_command(cmd, req, progress);
						break;
					}
					case Request_node::READ:
					{
						Superblock_control::Read_vbas &req = *cmd.read_vbas_ptr;
						progress |= _sb_control->execute(req, *_vbd, *this, _block_io, _crypto);
						_try_complete_command(cmd, req, progress);
						break;
					}
					case Request_node::SYNC:
					{
						Superblock_control::Synchronize &req = *cmd.sync_ptr;
						progress |= _sb_control->execute(req, _block_io, _trust_anchor);
						_try_complete_command(cmd, req, progress);
						break;
					}
					case Request_node::REKEY:
					{
						Superblock_control::Rekey &req = *cmd.rekey_ptr;
						progress |= _sb_control->execute(req, *_vbd, *_free_tree, *_meta_tree, _block_io, _crypto, _trust_anchor);
						_try_complete_multi_request_command(cmd, req, cmd.rekey_finished, progress);
						break;
					}
					case Request_node::EXTEND_VBD:
					{
						Superblock_control::Extend_vbd &req = *cmd.extend_vbd_ptr;
						progress |= _sb_control->execute(req, *_vbd, *_free_tree, *_meta_tree, _block_io, _trust_anchor);
						_try_complete_multi_request_command(cmd, req, cmd.extend_vbd_finished, progress);
						break;
					}
					case Request_node::EXTEND_FREE_TREE:
					{
						Superblock_control::Extend_free_tree &req = *cmd.extend_free_tree_ptr;
						progress |= _sb_control->execute(req, *_free_tree, *_meta_tree, _block_io, _trust_anchor);
						_try_complete_multi_request_command(cmd, req, cmd.extend_free_tree_finished, progress);
						break;
					}
					case Request_node::DEINITIALIZE:
					{
						Superblock_control::Deinitialize &req = *cmd.deinit_sb_control_ptr;
						progress |= _sb_control->execute(req, _block_io, _crypto, _trust_anchor);
						_try_complete_command(cmd, req, progress);
						break;
					}
					case Request_node::CREATE_SNAPSHOT:
					{
						Superblock_control::Create_snapshot &req = *cmd.create_snap_ptr;
						progress |= _sb_control->execute(req, _block_io, _trust_anchor);
						if (req.complete() && req.success())
							_snap_refs.insert(new (_heap) Snapshot_reference { cmd.request_node->snap_id, cmd.generation });
						_try_complete_command(cmd, req, progress);
						break;
					}
					case Request_node::DISCARD_SNAPSHOT:
					{
						Superblock_control::Discard_snapshot &req = *cmd.discard_snap_ptr;
						progress |= _sb_control->execute(req, _block_io, _trust_anchor);
						if (req.complete() && req.success())
							_remove_snap_refs_with_same_gen(node.snap_id);
						_try_complete_command(cmd, req, progress);
						break;
					}
					default: ASSERT_NEVER_REACHED;
					}
					break;
				}
				default: break;
				}
				break;

			case Command::LOG:

				switch(cmd.state) {
				case Command::INIT:

					log("\n", cmd.log_node->string, "\n");
					_mark_command_complete(cmd, true);
					progress = true;
					break;

				default: break;
				}
				break;

			case Command::CHECK:

				switch(cmd.state) {
				case Command::INIT:

					cmd.check_superblocks_ptr = new (_heap) Sb_check::Check();
					_mark_command_in_progress(cmd, progress);
					break;

				case Command::IN_PROGRESS:
				{
					Sb_check::Check &req = *cmd.check_superblocks_ptr;
					progress |= _sb_check.execute(req, _vbd_check, _ft_check, _block_io);
					_try_complete_command(cmd, req, progress);
					break;
				}
				default: break;
				}
				break;

			default: ASSERT_NEVER_REACHED;
			}
			return progress;
		}

		bool _execute_commands()
		{
			bool progress = false;
			_command_schedule.with_head([&] (Command &head) {

				progress |= _execute_command(head);
				if (head.state == Command::COMPLETE) {
					_command_schedule.remove_head();
					return;
				}
				if (head.type != Command::REQUEST)
					return;

				Request_node::Operation op = head.request_node->op;
				if (op != Request_node::REKEY &&
				    op != Request_node::EXTEND_VBD &&
				    op != Request_node::EXTEND_FREE_TREE)
					return;

				if (head.request_node->sync)
					return;

				if (head.state != Command::INIT)
					return;

				_command_schedule.try_yield_head([&] (Command &to_cmd) {

					if (to_cmd.type != Command::REQUEST)
						return false;

					Request_node::Operation op = to_cmd.request_node->op;
					return op == Request_node::READ ||
					       op == Request_node::WRITE ||
					       op == Request_node::SYNC ||
					       op == Request_node::DISCARD_SNAPSHOT;
				});
			});
			return progress;
		}

		void _handle_signal()
		{
			while (_execute_commands());
			if (_command_schedule.empty()) {
				if (_num_errors) {
					error(_num_errors, " command", _num_errors > 1 ? "s" : "", " failed!");
					_env.parent().exit(-1);
				} else {
					log("All commands succeeded!");
					_env.parent().exit(0);
				}
			}
			_wakeup_back_end_services();
		}

		template <typename FUNC>
		void _with_command(Command::Id id, FUNC && func)
		{
			ASSERT(_command_tree.first());
			_command_tree.first()->with_command(id, func);
		}

		Generation _snap_id_to_gen(Snapshot_id id)
		{
			Generation gen { INVALID_GENERATION };
			_snap_refs.with_ref(id, [&] (Snapshot_reference const &ref) {
				gen = ref.gen; });

			return gen;
		}


		/********************
		 ** Vfs::Env::User **
		 ********************/

		void wakeup_vfs_user() override { _signal_handler.local_submit(); }


		/********************************
		 ** Crypto_key_files_interface **
		 ********************************/

		void add_crypto_key(Key_id key_id) override
		{
			for (Constructible<Crypto_key> &key : _crypto_keys)
				if (!key.constructed()) {
					key.construct(key_id,
						open_file(_vfs_env, { _crypto_path, "/keys/", key_id, "/encrypt" }, Vfs::Directory_service::OPEN_MODE_RDWR),
						open_file(_vfs_env, { _crypto_path, "/keys/", key_id, "/decrypt" }, Vfs::Directory_service::OPEN_MODE_RDWR)
					);
					return;
				}
			ASSERT_NEVER_REACHED;
		}

		void remove_crypto_key(Key_id key_id) override
		{
			Constructible<Crypto_key> &crypto_key = _crypto_key(key_id);
			_vfs_env.root_dir().close(&crypto_key->encrypt_file);
			_vfs_env.root_dir().close(&crypto_key->decrypt_file);
			crypto_key.destruct();
		}

		Vfs::Vfs_handle &encrypt_file(Key_id key_id) override { return _crypto_key(key_id)->encrypt_file; }
		Vfs::Vfs_handle &decrypt_file(Key_id key_id) override { return _crypto_key(key_id)->decrypt_file; }


		/***************************
		 ** Client_data_interface **
		 ***************************/

		void obtain_data(Obtain_data_attr const &attr) override
		{
			_with_command(attr.in_req_tag, [&] (Command &cmd) {
				ASSERT(cmd.type == Command::REQUEST);
				Request_node const &node { *cmd.request_node };
				if (node.salt_avail)
					_generate_blk_data(attr.out_blk, attr.in_vba, node.salt);
			});
			if (_benchmark.constructed())
				_benchmark->raise_num_virt_blks_written();
		}

		void supply_data(Supply_data_attr const &attr) override
		{
			_with_command(attr.in_req_tag, [&] (Command &cmd) {
				ASSERT(cmd.type == Command::REQUEST);
				Request_node const &node { *cmd.request_node };
				if (node.salt_avail) {
					Tresor::Block gen_blk_data { };
					_generate_blk_data(gen_blk_data, attr.in_vba, node.salt);

					if (memcmp(&attr.in_blk, &gen_blk_data, BLOCK_SIZE)) {
						warning("client data mismatch: vba=", attr.in_vba, " req_tag=", attr.in_req_tag);
						_num_errors++;
					}
				}
			});
			if (_benchmark.constructed())
				_benchmark->raise_num_virt_blks_read();
		}

	public:

		Main(Genode::Env &env) : _env(env)
		{
			Command::Id command_id { 0 };
			_config_rom.xml().sub_node("commands").for_each_sub_node([&] (Xml_node const &node) {
				Command *cmd_ptr = new (_heap) Command(node, command_id++);
				_command_tree.insert(cmd_ptr);
				_command_schedule.add_tail(*cmd_ptr);
			});
			_handle_signal();
		}
};


void Component::construct(Genode::Env &env) { static Tresor_tester::Main main(env); }


namespace Libc {

	struct Env;
	struct Component { void construct(Libc::Env &) { } };
}
