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
#include <util/avl_tree.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <timer_session/connection.h>
#include <block_session/connection.h>
#include <vfs/simple_env.h>

/* tresor includes */
#include <tresor/init/configuration.h>
#include <tresor/crypto.h>
#include <tresor/trust_anchor.h>
#include <tresor/client_data.h>
#include <tresor/block_io.h>
#include <tresor/meta_tree.h>
#include <tresor/free_tree.h>
#include <tresor/request_pool.h>
#include <tresor/block_allocator.h>
#include <tresor/vbd_initializer.h>
#include <tresor/ft_initializer.h>
#include <tresor/sb_initializer.h>
#include <tresor/sb_check.h>
#include <tresor/vbd_check.h>
#include <tresor/ft_check.h>
#include <tresor/virtual_block_device.h>
#include <tresor/superblock_control.h>
#include <tresor/ft_resizing.h>

/* tresor tester includes */
#include <verbose_node.h>

using namespace Genode;
using namespace Tresor;
using namespace Vfs;

namespace Tresor_tester {

	class Main;
}


template <typename T>
T read_attribute(Xml_node const &node,
                 char     const *attr)
{
	T value { };

	if (!node.has_attribute(attr)) {

		error("<", node.type(), "> node misses attribute '", attr, "'");
		class Attribute_missing { };
		throw Attribute_missing { };
	}
	if (!node.attribute(attr).value(value)) {

		error("<", node.type(), "> node has malformed '", attr, "' attribute");
		class Malformed_attribute { };
		throw Malformed_attribute { };
	}
	return value;
}


class Log_node
{
	private:

		String<128> const _string;

	public:

		Log_node(Xml_node const &node)
		:
			_string { node.attribute_value("string", String<128> { }) }
		{ }

		String<128> const &string() const { return _string; }

		void print(Genode::Output &out) const
		{
			Genode::print(out, "string=\"", _string, "\"");
		}
};


class Benchmark_node
{
	public:

		using Label = String<128>;

		enum Operation { START, STOP };

	private:

		Operation const _op;
		bool      const _label_avail;
		Label     const _label;

		Operation _read_op_attr(Xml_node const &node)
		{
			class Attribute_missing { };
			if (!node.has_attribute("op")) {
				throw Attribute_missing { };
			}
			if (node.attribute("op").has_value("start")) {
				return Operation::START;
			}
			if (node.attribute("op").has_value("stop")) {
				return Operation::STOP;
			}
			class Malformed_attribute { };
			throw Malformed_attribute { };
		}

		static char const *_op_to_string(Operation op)
		{
			switch (op) {
			case START: return "start";
			case STOP: return "stop";
			}
			return "?";
		}

	public:

		bool has_attr_label() const
		{
			return _op == Operation::START;
		}

		Benchmark_node(Xml_node const &node)
		:
			_op          { _read_op_attr(node) },
			_label_avail { has_attr_label() && node.has_attribute("label") },
			_label       { _label_avail ?
			               node.attribute_value("label", Label { }) :
			               Label { } }
		{ }

		Operation op() const { return _op; }
		bool label_avail() const { return _label_avail; }
		Label const &label() const { return _label; }

		void print(Genode::Output &out) const
		{
			Genode::print(out, "op=", _op_to_string(_op));
			if (_label_avail) {
				Genode::print(out, " label=", _label);
			}
		}
};


class Benchmark
{
	private:

		enum State { STARTED, STOPPED };

		Genode::Env                   &_env;
		Timer::Connection              _timer                   { _env };
		State                          _state                   { STOPPED };
		Microseconds                   _start_time              { 0 };
		uint64_t                       _nr_of_virt_blks_read    { 0 };
		uint64_t                       _nr_of_virt_blks_written { 0 };
		Constructible<Benchmark_node>  _start_node              { };
		uint64_t                       _id                      { 0 };

	public:

		Benchmark(Genode::Env &env) : _env { env } { }

		void submit_request(Benchmark_node const &node)
		{
			switch (node.op()) {
			case Benchmark_node::START:

				if (_state != STOPPED) {
					class Bad_state_to_start { };
					throw Bad_state_to_start { };
				}
				_id++;
				_nr_of_virt_blks_read = 0;
				_nr_of_virt_blks_written = 0;
				_state = STARTED;
				_start_node.construct(node);
				_start_time = _timer.curr_time().trunc_to_plain_us();
				break;

			case Benchmark_node::STOP:

				if (_state != STARTED) {
					class Bad_state_to_stop { };
					throw Bad_state_to_stop { };
				}
				uint64_t const stop_time_us {
					_timer.curr_time().trunc_to_plain_us().value };

				log("");
				if (_start_node->label_avail()) {
					log("Benchmark result \"", _start_node->label(), "\"");
				} else {
					log("Benchmark result (command ID ", _id, ")");
				}

				double const passed_time_sec {
					(double)(stop_time_us - _start_time.value) /
					(double)(1000 * 1000) };

				log("   Ran ", passed_time_sec, " seconds.");

				if (_nr_of_virt_blks_read != 0) {

					uint64_t const bytes_read {
						_nr_of_virt_blks_read * Tresor::BLOCK_SIZE };

					double const mibyte_read {
						(double)bytes_read / (double)(1024 * 1024) };

					double const mibyte_per_sec_read {
						(double)bytes_read / (double)passed_time_sec /
						(double)(1024 * 1024) };

					log("   Have read ", mibyte_read, " mebibyte in total.");
					log("   Have read ", mibyte_per_sec_read, " mebibyte per second.");
				}

				if (_nr_of_virt_blks_written != 0) {

					uint64_t bytes_written {
						_nr_of_virt_blks_written * Tresor::BLOCK_SIZE };

					double const mibyte_written {
						(double)bytes_written / (double)(1024 * 1024) };

					double const mibyte_per_sec_written {
						(double)bytes_written / (double)passed_time_sec /
						(double)(1024 * 1024) };

					log("   Have written ", mibyte_written, " mebibyte in total.");
					log("   Have written ", mibyte_per_sec_written, " mebibyte per second.");
				}
				log("");
				_state = STOPPED;
				break;
			}
		}

		void raise_nr_of_virt_blks_read()    { _nr_of_virt_blks_read++;    }
		void raise_nr_of_virt_blks_written() { _nr_of_virt_blks_written++; }
};


class Trust_anchor_node
{
	private:

		using Operation = Trust_anchor_request::Type;

		Operation  const _op;
		String<64> const _passphrase;

		Operation _read_op_attr(Xml_node const &node)
		{
			class Attribute_missing { };
			if (!node.has_attribute("op")) {
				throw Attribute_missing { };
			}
			if (node.attribute("op").has_value("initialize")) {
				return Operation::INITIALIZE;
			}
			class Malformed_attribute { };
			throw Malformed_attribute { };
		}

	public:

		Trust_anchor_node(Xml_node const &node)
		:
			_op               { _read_op_attr(node) },
			_passphrase       { has_attr_passphrase() ?
			                    node.attribute_value("passphrase", String<64>()) :
			                    String<64>() }
		{ }

		Operation         op()         const { return _op; }
		String<64> const &passphrase() const { return _passphrase; }

		bool has_attr_passphrase() const
		{
			return _op == Operation::INITIALIZE;
		}

		void print(Genode::Output &out) const
		{
			Genode::print(out, "op=",
				Trust_anchor_request::type_to_string(_op));

			if (has_attr_passphrase()) {
				Genode::print(out, " passphrase=", _passphrase);
			}
		}
};


class Request_node
{
	private:

		using Operation = Tresor::Request::Operation;

		Operation             const _op;
		Virtual_block_address const _vba;
		Number_of_blocks      const _count;
		bool                  const _sync;
		bool                  const _salt_avail;
		uint64_t              const _salt;
		Snapshot_id           const _snap_id;

		Operation _read_op_attr(Xml_node const &node)
		{
			class Attribute_missing { };
			if (!node.has_attribute("op")) {
				throw Attribute_missing { };
			}
			if (node.attribute("op").has_value("read")) {
				return Operation::READ;
			}
			if (node.attribute("op").has_value("write")) {
				return Operation::WRITE;
			}
			if (node.attribute("op").has_value("sync")) {
				return Operation::SYNC;
			}
			if (node.attribute("op").has_value("create_snapshot")) {
				return Operation::CREATE_SNAPSHOT;
			}
			if (node.attribute("op").has_value("discard_snapshot")) {
				return Operation::DISCARD_SNAPSHOT;
			}
			if (node.attribute("op").has_value("extend_ft")) {
				return Operation::EXTEND_FT;
			}
			if (node.attribute("op").has_value("extend_vbd")) {
				return Operation::EXTEND_VBD;
			}
			if (node.attribute("op").has_value("rekey")) {
				return Operation::REKEY;
			}
			if (node.attribute("op").has_value("deinitialize")) {
				return Operation::DEINITIALIZE;
			}
			class Malformed_attribute { };
			throw Malformed_attribute { };
		}

	public:

		Request_node(Xml_node const &node)
		:
			_op         { _read_op_attr(node) },
			_vba        { has_attr_vba() ?
			              read_attribute<uint64_t>(node, "vba") : 0 },
			_count      { has_attr_count() ?
			              read_attribute<uint64_t>(node, "count") : 0 },
			_sync       { read_attribute<bool>(node, "sync") },
			_salt_avail { has_attr_salt() ?
			              node.has_attribute("salt") : false },
			_salt       { has_attr_salt() && _salt_avail ?
			              read_attribute<uint64_t>(node, "salt") : 0 },
			_snap_id    { has_attr_snap_id() ?
			              read_attribute<Snapshot_id>(node, "id") : 0 }
		{ }

		Operation             op()         const { return _op; }
		Virtual_block_address vba()        const { return _vba; }
		Number_of_blocks      count()      const { return _count; }
		bool                  sync()       const { return _sync; }
		bool                  salt_avail() const { return _salt_avail; }
		uint64_t              salt()       const { return _salt; }
		Snapshot_id           snap_id()    const { return _snap_id; }

		bool has_attr_vba() const
		{
			return _op == Operation::READ ||
			       _op == Operation::WRITE ||
			       _op == Operation::SYNC;
		}

		bool has_attr_salt() const
		{
			return _op == Operation::READ ||
			       _op == Operation::WRITE;
		}

		bool has_attr_count() const
		{
			return _op == Operation::READ ||
			       _op == Operation::WRITE ||
			       _op == Operation::SYNC ||
			       _op == Operation::EXTEND_FT ||
			       _op == Operation::EXTEND_VBD;
		}

		bool has_attr_snap_id() const
		{
			return _op == Operation::DISCARD_SNAPSHOT ||
			       _op == Operation::CREATE_SNAPSHOT;
		}

		void print(Genode::Output &out) const
		{
			Genode::print(out, "op=", to_string(_op));
			if (has_attr_vba()) {
				Genode::print(out, " vba=", _vba);
			}
			if (has_attr_count()) {
				Genode::print(out, " count=", _count);
			}
			Genode::print(out, " sync=", _sync);
			if (_salt_avail) {
				Genode::print(out, " salt=", _salt);
			}
		}
};


class Command : public Fifo<Command>::Element
{
	public:

		enum Type
		{
			INVALID,
			REQUEST,
			TRUST_ANCHOR,
			BENCHMARK,
			CONSTRUCT,
			DESTRUCT,
			INITIALIZE,
			CHECK,
			LIST_SNAPSHOTS,
			LOG
		};

		enum State
		{
			PENDING,
			IN_PROGRESS,
			COMPLETED
		};

	private:

		Type                                   _type              { INVALID };
		uint32_t                               _id                { 0 };
		State                                  _state             { PENDING };
		bool                                   _success           { false };
		bool                                   _data_mismatch     { false };
		Constructible<Request_node>            _request_node      { };
		Constructible<Trust_anchor_node>       _trust_anchor_node { };
		Constructible<Benchmark_node>          _benchmark_node    { };
		Constructible<Log_node>                _log_node          { };
		Constructible<Tresor_init::Configuration> _initialize        { };

		char const *_state_to_string() const
		{
			switch (_state) {
			case PENDING: return "pending";
			case IN_PROGRESS: return "in_progress";
			case COMPLETED: return "completed";
			}
			return "?";
		}

		char const *_type_to_string() const
		{
			switch (_type) {
			case INITIALIZE: return "initialize";
			case INVALID: return "invalid";
			case REQUEST: return "request";
			case TRUST_ANCHOR: return "trust_anchor";
			case BENCHMARK: return "benchmark";
			case CONSTRUCT: return "construct";
			case DESTRUCT: return "destruct";
			case CHECK: return "check";
			case LIST_SNAPSHOTS: return "list_snapshots";
			case LOG: return "log";
			}
			return "?";
		}

	public:

		Command() { }

		Command(Type            type,
		        Xml_node const &node,
		        uint32_t        id)
		:
			_type { type },
			_id   { id }
		{
			switch (_type) {
			case INITIALIZE:   _initialize.construct(node);        break;
			case REQUEST:      _request_node.construct(node);      break;
			case TRUST_ANCHOR: _trust_anchor_node.construct(node); break;
			case BENCHMARK:    _benchmark_node.construct(node);    break;
			case LOG:          _log_node.construct(node);          break;
			default:                                               break;
			}
		}

		Command(Command &other)
		:
			_type    { other._type },
			_id      { other._id },
			_state   { other._state },
			_success { other._success }
		{
			switch (_type) {
			case INITIALIZE:   _initialize.construct(*other._initialize);               break;
			case REQUEST:      _request_node.construct(*other._request_node);           break;
			case TRUST_ANCHOR: _trust_anchor_node.construct(*other._trust_anchor_node); break;
			case BENCHMARK:    _benchmark_node.construct(*other._benchmark_node);       break;
			case LOG:          _log_node.construct(*other._log_node);                   break;
			default:                                                                    break;
			}
		}

		bool has_attr_data_mismatch() const
		{
			return
				_type == REQUEST &&
				_request_node->op() == Tresor::Request::Operation::READ &&
				_request_node->salt_avail();
		}

		bool synchronize() const
		{
			class Bad_type { };
			switch (_type) {
			case INITIALIZE:     return true;
			case BENCHMARK:      return true;
			case CONSTRUCT:      return true;
			case DESTRUCT:       return true;
			case CHECK:          return true;
			case TRUST_ANCHOR:   return true;
			case LIST_SNAPSHOTS: return true;
			case LOG:            return true;
			case REQUEST:        return _request_node->sync();
			case INVALID:        throw Bad_type { };
			}
			throw Bad_type { };
		}

		static Type type_from_string(String<64> str)
		{
			if (str == "initialize")     { return INITIALIZE; }
			if (str == "request")        { return REQUEST; }
			if (str == "trust-anchor")   { return TRUST_ANCHOR; }
			if (str == "benchmark")      { return BENCHMARK; }
			if (str == "construct")      { return CONSTRUCT; }
			if (str == "destruct")       { return DESTRUCT; }
			if (str == "check")          { return CHECK; }
			if (str == "list-snapshots") { return LIST_SNAPSHOTS; }
			if (str == "log")            { return LOG; }
			class Bad_string { };
			throw Bad_string { };
		}

		void print(Genode::Output &out) const
		{
			Genode::print(out, "id=", _id, " type=", _type_to_string());
			class Bad_type { };
			switch (_type) {
			case INITIALIZE:     Genode::print(out, " cfg=(", *_initialize, ")"); break;
			case REQUEST:        Genode::print(out, " cfg=(", *_request_node, ")"); break;
			case TRUST_ANCHOR:   Genode::print(out, " cfg=(", *_trust_anchor_node, ")"); break;
			case BENCHMARK:      Genode::print(out, " cfg=(", *_benchmark_node, ")"); break;
			case LOG:            Genode::print(out, " cfg=(", *_log_node, ")"); break;
			case INVALID:        break;
			case CHECK:          break;
			case CONSTRUCT:      break;
			case DESTRUCT:       break;
			case LIST_SNAPSHOTS: break;
			}
			Genode::print(out, " succ=", _success);
			if (has_attr_data_mismatch()) {
				Genode::print(out, " bad_data=", _data_mismatch);
			}
			Genode::print(out, " state=", _state_to_string());
		}

		Type                           type              () const { return _type              ; }
		State                          state             () const { return _state             ; }
		uint32_t                       id                () const { return _id                ; }
		bool                           success           () const { return _success           ; }
		bool                           data_mismatch     () const { return _data_mismatch     ; }
		Request_node            const &request_node      () const { return *_request_node     ; }
		Trust_anchor_node       const &trust_anchor_node () const { return *_trust_anchor_node; }
		Benchmark_node          const &benchmark_node    () const { return *_benchmark_node   ; }
		Log_node                const &log_node          () const { return *_log_node         ; }
		Tresor_init::Configuration const &initialize        () const { return *_initialize       ; }

		void state         (State state)        { _state = state; }
		void success       (bool success)       { _success = success; }
		void data_mismatch (bool data_mismatch) { _data_mismatch = data_mismatch; }
};


class Snapshot_reference : public Genode::Avl_node<Snapshot_reference>
{
	private:

		Snapshot_id const _id;
		Generation const _gen;

	public:

		Snapshot_reference(Snapshot_id id, Generation gen) : _id { id }, _gen { gen } { }

		Snapshot_id id() const { return _id; }
		Generation gen() const { return _gen; }

		template <typename HANDLE_MATCH_FN, typename HANDLE_NO_MATCH_FN>
		void find(Snapshot_id const id,
		          HANDLE_MATCH_FN && handle_match,
		          HANDLE_NO_MATCH_FN && handle_no_match) const
		{
			if (id != _id) {
				Snapshot_reference *child_ptr { Avl_node<Snapshot_reference>::child(id > _id) };
				if (child_ptr)
					child_ptr->find(id, handle_match, handle_no_match);
				else
					handle_no_match();
			} else
				handle_match(*this);
		}

		void print(Genode::Output &out) const
		{
			Genode::print(out, " id ", _id, " gen ", _gen);
		}

		bool higher(Snapshot_reference *other_ptr)
		{
			return other_ptr->_id > _id;
		}
};


class Snapshot_reference_tree : public Avl_tree<Snapshot_reference>
{
	public:

		template <typename HANDLE_MATCH_FN, typename HANDLE_NO_MATCH_FN>
		void find(Snapshot_id const snap_id,
		          HANDLE_MATCH_FN && handle_match,
		          HANDLE_NO_MATCH_FN && handle_no_match) const
		{
			if (first() != nullptr)
				first()->find(snap_id, handle_match, handle_no_match);
			else
				handle_no_match();
		}
};


class Command_pool : public Module {

	private:

		Allocator &_alloc;
		Verbose_node const &_verbose_node;
		Fifo<Command> _cmd_queue { };
		uint32_t _next_command_id { 0 };
		unsigned long _nr_of_uncompleted_cmds { 0 };
		unsigned long _nr_of_errors { 0 };
		Tresor::Block _blk_data { };
		Snapshot_reference_tree _snap_refs { };

		void _read_cmd_node(Xml_node const &node,
		                    Command::Type   cmd_type)
		{
			Command &cmd {
				*new (_alloc) Command(cmd_type, node, _next_command_id++) };

			_nr_of_uncompleted_cmds++;
			_cmd_queue.enqueue(cmd);

			if (_verbose_node.cmd_pool_cmd_pending()) {
				log("cmd pending: ", cmd);
			}
		}

		static void _generate_blk_data(Tresor::Block         &blk_data,
		                               Virtual_block_address  vba,
		                               uint64_t               salt)
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


		/************
		 ** Module **
		 ************/

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override
		{
			{
				Command const cmd {
					peek_pending_command(Command::TRUST_ANCHOR) };

				if (cmd.type() != Command::INVALID) {

					Trust_anchor_node const &node { cmd.trust_anchor_node() };
					switch (node.op()) {
					case Trust_anchor_request::INITIALIZE:

						Trust_anchor_request::create(
							buf_ptr, buf_size, COMMAND_POOL, cmd.id(),
							Trust_anchor_request::INITIALIZE,
							nullptr, nullptr, node.passphrase().string(),
							nullptr);

						return true;

					default: break;
					}
					class Exception_1 { };
					throw Exception_1 { };
				}
			}

			{
				Command const cmd {
					peek_pending_command(Command::INITIALIZE) };

				if (cmd.type() != Command::INVALID) {

					Tresor_init::Configuration const &cfg { cmd.initialize() };

					Sb_initializer_request::create(
						buf_ptr, buf_size, COMMAND_POOL, cmd.id(),
						Sb_initializer_request::INIT,
						(Tree_level_index)(cfg.vbd_nr_of_lvls() - 1),
						(Tree_degree)cfg.vbd_nr_of_children(),
						cfg.vbd_nr_of_leafs(),
						(Tree_level_index)cfg.ft_nr_of_lvls() - 1,
						(Tree_degree)cfg.ft_nr_of_children(),
						cfg.ft_nr_of_leafs(),
						(Tree_level_index)cfg.ft_nr_of_lvls() - 1,
						(Tree_degree)cfg.ft_nr_of_children(),
						cfg.ft_nr_of_leafs());

					return true;
				}
			}

			{
				Command const cmd {
					peek_pending_command(Command::CHECK) };

				if (cmd.type() != Command::INVALID) {

					Sb_check_request::create(
						buf_ptr, buf_size, COMMAND_POOL, cmd.id(),
						Sb_check_request::CHECK);

					return true;
				}
			}
			return false;
		}

		void _drop_generated_request(Module_request &mod_req) override
		{
			switch (mod_req.dst_module_id()) {
			case TRUST_ANCHOR:
			{
				Trust_anchor_request const &ta_req {
					*static_cast<Trust_anchor_request *>(&mod_req)};

				if (ta_req.type() != Trust_anchor_request::INITIALIZE) {
					class Exception_2 { };
					throw Exception_2 { };
				}
				mark_command_in_progress(ta_req.src_request_id());
				break;
			}
			case SB_INITIALIZER:
			{
				Sb_initializer_request const &sb_req {
					*static_cast<Sb_initializer_request *>(&mod_req)};

				if (sb_req.type() != Sb_initializer_request::INIT) {
					class Exception_4 { };
					throw Exception_4 { };
				}
				mark_command_in_progress(sb_req.src_request_id());
				break;
			}
			case SB_CHECK:
			{
				Sb_check_request const &req {
					*static_cast<Sb_check_request *>(&mod_req)};

				if (req.type() != Sb_check_request::CHECK) {
					class Exception_3 { };
					throw Exception_3 { };
				}
				mark_command_in_progress(req.src_request_id());
				break;
			}
			default:
			{
				class Exception_1 { };
				throw Exception_1 { };
			}
			}
		}

		template <typename HANDLE_MATCH_FN, typename HANDLE_NO_MATCH_FN>
		void find_cmd(Module_request_id cmd_id,
		              HANDLE_MATCH_FN && handle_match_fn,
		              HANDLE_NO_MATCH_FN && handle_no_match_fn)
		{
			bool cmd_found { false };
			_cmd_queue.for_each([&] (Command &cmd)
			{
				if (cmd_found)
					return;

				if (cmd.id() == cmd_id) {
					handle_match_fn(cmd);
					cmd_found = true;
				}
			});
			if (!cmd_found)
				handle_no_match_fn();
		}

		void generated_request_complete(Module_request &mod_req) override
		{
			switch (mod_req.dst_module_id()) {
			case TRUST_ANCHOR:
			{
				Trust_anchor_request const &ta_req {
					*static_cast<Trust_anchor_request *>(&mod_req)};

				if (ta_req.type() != Trust_anchor_request::INITIALIZE) {
					class Exception_2 { };
					throw Exception_2 { };
				}
				mark_command_completed(
					ta_req.src_request_id(), ta_req.success());
				break;
			}
			case SB_INITIALIZER:
			{
				Sb_initializer_request const &sb_req {
					*static_cast<Sb_initializer_request *>(&mod_req)};

				if (sb_req.type() != Sb_initializer_request::INIT) {
					class Exception_2 { };
					throw Exception_2 { };
				}
				mark_command_completed(
					sb_req.src_request_id(), sb_req.success());
				break;
			}
			case SB_CHECK:
			{
				Sb_check_request const &sb_req {
					*static_cast<Sb_check_request *>(&mod_req)};

				if (sb_req.type() != Sb_check_request::CHECK) {
					class Exception_2 { };
					throw Exception_2 { };
				}
				mark_command_completed(
					sb_req.src_request_id(), sb_req.success());
				break;
			}
			case REQUEST_POOL:
			{
				Request const &rp_req {
					*static_cast<Request *>(&mod_req)};

				Module_request_id const cmd_id { rp_req.src_request_id() };
				bool const success { rp_req.success() };
				if (success && rp_req.operation() == Tresor::Request::CREATE_SNAPSHOT) {
					find_cmd(cmd_id, [&] (Command &cmd)
					{
						_snap_refs.insert(new (_alloc)
							Snapshot_reference { cmd.request_node().snap_id(), rp_req.gen() });
					},
					[&] () { ASSERT_NEVER_REACHED; });
				}
				mark_command_completed(cmd_id, success);
				break;
			}
			default:
			{
				class Exception_1 { };
				throw Exception_1 { };
			}
			}
		}

	public:

		Command_pool(Allocator          &alloc,
		             Xml_node     const &config_xml,
		             Verbose_node const &verbose_node)
		:
			_alloc        { alloc },
			_verbose_node { verbose_node }
		{
			config_xml.sub_node("commands").for_each_sub_node(
				[&] (Xml_node const &node)
			{
				_read_cmd_node(node, Command::type_from_string(node.type()));
			});
		}

		Generation snap_id_to_gen(Snapshot_id id)
		{
			Generation gen { INVALID_GENERATION };
			_snap_refs.find(id, [&] (Snapshot_reference const &snap_ref)
			{
				gen = snap_ref.gen();
			},
			[&] () { ASSERT_NEVER_REACHED; });
			return gen;
		}

		Command peek_pending_command(Command::Type type) const
		{
			Reconstructible<Command> resulting_cmd { };
			bool first_uncompleted_cmd { true };
			bool exit_loop { false };
			_cmd_queue.for_each([&] (Command &curr_cmd)
			{
				if (exit_loop) {
					return;
				}
				switch (curr_cmd.state()) {
				case Command::PENDING:

					/*
					 * Stop iterating at the first uncompleted command
					 * that needs to be synchronized.
					 */
					if (curr_cmd.synchronize()) {
						if (curr_cmd.type() == type && first_uncompleted_cmd) {
							resulting_cmd.construct(curr_cmd);
						}
						exit_loop = true;
						return;
					}
					/*
					 * Select command and stop iterating if the command is of
					 * the desired type.
					 */
					if (curr_cmd.type() == type) {
						resulting_cmd.construct(curr_cmd);
						exit_loop = true;
					}
					first_uncompleted_cmd = false;
					return;

				case Command::IN_PROGRESS:

					/*
					 * Stop iterating at the first uncompleted command
					 * that needs to be synchronized.
					 */
					if (curr_cmd.synchronize()) {
						exit_loop = true;
						return;
					}
					first_uncompleted_cmd = false;
					return;

				case Command::COMPLETED:

					return;
				}
			});
			return *resulting_cmd;
		}

		void mark_command_in_progress(Module_request_id cmd_id)
		{
			find_cmd(cmd_id, [&] (Command &cmd)
			{
				ASSERT(cmd.state() == Command::PENDING);
				cmd.state(Command::IN_PROGRESS);
				if (_verbose_node.cmd_pool_cmd_in_progress())
					log("cmd in progress: ", cmd);
			},
			[&] () { ASSERT_NEVER_REACHED; });
		}

		void mark_command_completed(Module_request_id cmd_id,
		                            bool              success)
		{
			find_cmd(cmd_id, [&] (Command &cmd)
			{
				ASSERT(cmd.state() == Command::IN_PROGRESS);
				cmd.state(Command::COMPLETED);
				_nr_of_uncompleted_cmds--;
				cmd.success(success);
				if (!cmd.success()) {
					error("cmd failed");
					_nr_of_errors++;
				}
				if (_verbose_node.cmd_pool_cmd_completed())
					log("cmd completed: ", cmd);
			},
			[&] () { ASSERT_NEVER_REACHED; });
		}

		void generate_blk_data(uint64_t               tresor_req_tag,
		                       Virtual_block_address  vba,
		                       Tresor::Block         &blk_data)
		{
			find_cmd(tresor_req_tag, [&] (Command &cmd)
			{
				ASSERT(cmd.type() == Command::REQUEST);
				Request_node const &req_node { cmd.request_node() };
				if (req_node.salt_avail())
					_generate_blk_data(blk_data, vba, req_node.salt());
			},
			[&] () { ASSERT_NEVER_REACHED; });
		}

		void verify_blk_data(uint64_t               tresor_req_tag,
		                     Virtual_block_address  vba,
		                     Tresor::Block         &blk_data)
		{
			find_cmd(tresor_req_tag, [&] (Command &cmd)
			{
				ASSERT(cmd.type() == Command::REQUEST);
				Request_node const &req_node { cmd.request_node() };
				if (req_node.salt_avail()) {
					Tresor::Block gen_blk_data { };
					_generate_blk_data(gen_blk_data, vba, req_node.salt());

					if (memcmp(&blk_data, &gen_blk_data, BLOCK_SIZE)) {

						cmd.data_mismatch(true);
						error("client data mismatch");
						_nr_of_errors++;

						if (_verbose_node.client_data_mismatch()) {
							log("client data mismatch: vba=", vba,
							    " req_tag=(", tresor_req_tag, ")");
							log("client data should be: ", gen_blk_data);
							log("client data is: ", blk_data);
							class Client_data_mismatch { };
							throw Client_data_mismatch { };
						}
					}
				}
			},
			[&] () { ASSERT_NEVER_REACHED; });
		}

		void print_failed_cmds() const
		{
			_cmd_queue.for_each([&] (Command &cmd)
			{
				if (cmd.state() != Command::COMPLETED) {
					return;
				}
				if (cmd.success() &&
				    (!cmd.has_attr_data_mismatch() || !cmd.data_mismatch())) {

					return;
				}
				log("cmd failed: ", cmd);
			});
		}

		unsigned long nr_of_uncompleted_cmds() { return _nr_of_uncompleted_cmds; }
		unsigned long nr_of_errors()           { return _nr_of_errors; }
};


static Block_allocator *_block_allocator_ptr;


Genode::uint64_t block_allocator_first_block()
{
	if (!_block_allocator_ptr) {
		struct Exception_1 { };
		throw Exception_1();
	}

	return _block_allocator_ptr->first_block();
}


Genode::uint64_t block_allocator_nr_of_blks()
{
	if (!_block_allocator_ptr) {
		struct Exception_1 { };
		throw Exception_1();
	}

	return _block_allocator_ptr->nr_of_blks();
}


class Tresor_tester::Main
:
	private Vfs::Env::User,
	private Tresor::Module_composition,
	public  Tresor::Module
{
	private:

		Genode::Env                        &_env;
		Attached_rom_dataspace              _config_rom          { _env, "config" };
		Verbose_node                        _verbose_node        { _config_rom.xml() };
		Heap                                _heap                { _env.ram(), _env.rm() };
		Vfs::Simple_env                     _vfs_env             { _env, _heap, _config_rom.xml().sub_node("vfs"), *this };
		Signal_handler<Main>                _sigh                { _env.ep(), *this, &Main::_execute };
		Command_pool                        _cmd_pool            { _heap, _config_rom.xml(), _verbose_node };
		Constructible<Free_tree>            _free_tree           { };
		Constructible<Virtual_block_device> _vbd                 { };
		Constructible<Superblock_control>   _sb_control          { };
		Constructible<Request_pool>         _request_pool        { };
		Constructible<Ft_resizing>          _ft_resizing         { };
		Benchmark                           _benchmark           { _env };
		Meta_tree                           _meta_tree           { };
		Trust_anchor                        _trust_anchor        { _vfs_env, _config_rom.xml().sub_node("trust-anchor") };
		Crypto                              _crypto              { _vfs_env, _config_rom.xml().sub_node("crypto") };
		Block_io                            _block_io            { _vfs_env, _config_rom.xml().sub_node("block-io") };
		Block_allocator                     _block_allocator     { NR_OF_SUPERBLOCK_SLOTS };
		Vbd_initializer                     _vbd_initializer     { };
		Ft_initializer                      _ft_initializer      { };
		Sb_initializer                      _sb_initializer      { };
		Sb_check                            _sb_check            { };
		Vbd_check                           _vbd_check           { };
		Ft_check                            _ft_check            { };
		Client_data_request                 _client_data_request { };

		/*
		 * Noncopyable
		 */
		Main(Main const &) = delete;
		Main &operator = (Main const &) = delete;

		void _construct_tresor()
		{
			_free_tree.construct();
			add_module(FREE_TREE, *_free_tree);

			_vbd.construct();
			add_module(VIRTUAL_BLOCK_DEVICE, *_vbd);

			_sb_control.construct();
			add_module(SUPERBLOCK_CONTROL, *_sb_control);

			_request_pool.construct();
			add_module(REQUEST_POOL, *_request_pool);

			_ft_resizing.construct();
			add_module(FT_RESIZING, *_ft_resizing);
		}

		void _destruct_tresor()
		{
			remove_module(FT_RESIZING);
			_ft_resizing.destruct();

			remove_module(REQUEST_POOL);
			_request_pool.destruct();

			remove_module(SUPERBLOCK_CONTROL);
			_sb_control.destruct();

			remove_module(VIRTUAL_BLOCK_DEVICE);
			_vbd.destruct();

			remove_module(FREE_TREE);
			_free_tree.destruct();
		}

		/**
		 * Vfs::Env::User interface
		 */
		void wakeup_vfs_user() override { _sigh.local_submit(); }

		bool ready_to_submit_request() override
		{
			return _client_data_request._type == Client_data_request::INVALID;
		}

		void submit_request(Module_request &req) override
		{
			if (_client_data_request._type != Client_data_request::INVALID) {

				class Exception_1 { };
				throw Exception_1 { };
			}
			req.dst_request_id(0);
			_client_data_request = *static_cast<Client_data_request *>(&req);
			switch (_client_data_request._type) {
			case Client_data_request::OBTAIN_PLAINTEXT_BLK:

				_cmd_pool.generate_blk_data(
					_client_data_request._client_req_tag,
					_client_data_request._vba,
					*(Tresor::Block *)_client_data_request._plaintext_blk_ptr);

				_benchmark.raise_nr_of_virt_blks_written();

				if (_verbose_node.client_data_transferred())
					log("client data: vba=", _client_data_request._vba,
					    " req_tag=", _client_data_request._client_req_tag);

				_client_data_request._success = true;
				break;

			case Client_data_request::SUPPLY_PLAINTEXT_BLK:

				_cmd_pool.verify_blk_data(
					_client_data_request._client_req_tag,
					_client_data_request._vba,
					*(Tresor::Block *)_client_data_request._plaintext_blk_ptr);

				_benchmark.raise_nr_of_virt_blks_read();

				if (_verbose_node.client_data_transferred())
					log("client data: vba=", _client_data_request._vba,
					    " req_tag=", _client_data_request._client_req_tag);

				_client_data_request._success = true;
				break;

			case Client_data_request::INVALID:

				class Exception_2 { };
				throw Exception_2 { };
			}
		}

		void execute(bool &) override { }

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override
		{
			if (_client_data_request._type != Client_data_request::INVALID) {
				if (sizeof(_client_data_request) > buf_size) {
					class Exception_1 { };
					throw Exception_1 { };
				}
				Genode::memcpy(buf_ptr, &_client_data_request,
				               sizeof(_client_data_request));;
				return true;
			}
			return false;
		}

		void _drop_completed_request(Module_request &) override
		{
			if (_client_data_request._type == Client_data_request::INVALID) {
				class Exception_2 { };
				throw Exception_2 { };
			}
			_client_data_request._type = Client_data_request::INVALID;
		}

		void _cmd_pool_handle_pending_tresor_cmds(bool &progress)
		{
			while (true) {

				if (!_request_pool->ready_to_submit_request()) {
					break;
				}
				Command const cmd {
					_cmd_pool.peek_pending_command(Command::REQUEST) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				Request_node req_node { cmd.request_node() };
				Generation gen { INVALID_GENERATION };
				if (req_node.op() == Request::DISCARD_SNAPSHOT)
					gen = _cmd_pool.snap_id_to_gen(req_node.snap_id());

				Tresor::Request tresor_req {
					cmd.request_node().op(),
					false,
					req_node.has_attr_vba() ? req_node.vba() : 0,
					0,
					req_node.has_attr_count() ? req_node.count() : 0,
					0,
					cmd.id(), gen, COMMAND_POOL, cmd.id() };

				_request_pool->submit_request(tresor_req);
				if (VERBOSE_MODULE_COMMUNICATION)
					Genode::log(
						module_name(tresor_req.src_module_id()), " ",
						tresor_req.src_request_id_str(),
						" --", tresor_req, "--> ",
						module_name(tresor_req.dst_module_id()), " ",
						tresor_req.dst_request_id_str());

				_cmd_pool.mark_command_in_progress(cmd.id());
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_construct_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::CONSTRUCT) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				_construct_tresor();
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_destruct_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::DESTRUCT) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				_destruct_tresor();
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_list_snapshots_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::LIST_SNAPSHOTS) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				Snapshot_generations generations;
				_sb_control->snapshot_generations(generations);
				unsigned snap_nr { 0 };
				log("");
				log("List snapshots (command ID ", cmd.id(), ")");
				for (Generation const &gen : generations.items) {
					if (gen != INVALID_GENERATION) {
						log("   Snapshot #", snap_nr, " is generation ", gen);
						snap_nr++;
					}
				}
				log("");
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_log_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::LOG) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				log("\n", cmd.log_node().string(), "\n");
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_benchmark_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::BENCHMARK) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				_benchmark.submit_request(cmd.benchmark_node());
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _execute_command_pool(bool &progress)
		{
			if (_request_pool.constructed()) {
				_cmd_pool_handle_pending_tresor_cmds(progress);
				_cmd_pool_handle_pending_list_snapshots_cmds(progress);
			}
			_cmd_pool_handle_pending_log_cmds(progress);
			_cmd_pool_handle_pending_benchmark_cmds(progress);
			_cmd_pool_handle_pending_construct_cmds(progress);
			_cmd_pool_handle_pending_destruct_cmds(progress);

			if (_cmd_pool.nr_of_uncompleted_cmds() == 0) {

				if (_cmd_pool.nr_of_errors() > 0) {

					_cmd_pool.print_failed_cmds();
					_env.parent().exit(-1);

				} else {

					_env.parent().exit(0);
				}
			}
		}

		void _execute()
		{
			bool progress { true };
			while (progress) {

				progress = false;
				_execute_command_pool(progress);
				execute_modules(progress);
			}
			_vfs_env.io().commit();
		}

	public:

		Main(Genode::Env &env)
		:
			_env { env }
		{
			add_module(META_TREE,        _meta_tree);
			add_module(CRYPTO,           _crypto);
			add_module(TRUST_ANCHOR,     _trust_anchor);
			add_module(CLIENT_DATA,     *this);
			add_module(COMMAND_POOL,     _cmd_pool);
			add_module(BLOCK_IO,         _block_io);
			add_module(BLOCK_ALLOCATOR,  _block_allocator);
			add_module(VBD_INITIALIZER,  _vbd_initializer);
			add_module(FT_INITIALIZER,   _ft_initializer);
			add_module(SB_INITIALIZER,   _sb_initializer);
			add_module(SB_CHECK,         _sb_check);
			add_module(VBD_CHECK,        _vbd_check);
			add_module(FT_CHECK,         _ft_check);

			_block_allocator_ptr = &_block_allocator;

			_execute();
		}
};

namespace Libc {

	struct Env;

	struct Component
	{
		void construct(Libc::Env &);
	};
}


void Libc::Component::construct(Libc::Env &) { }


void Component::construct(Genode::Env &env)
{
	env.exec_static_constructors();

	static Tresor_tester::Main main(env);

	(void)block_allocator_first_block();
	(void)block_allocator_nr_of_blks();
}

extern "C" int memcmp(const void *p0, const void *p1, Genode::size_t size)
{
	return Genode::memcmp(p0, p1, size);
}
