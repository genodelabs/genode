/*
 * \brief  Front end of the partition server
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/service.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <parent/parent.h>
#include <util/bit_allocator.h>

#include "gpt.h"
#include "mbr.h"
#include "ahdi.h"
#include "disk.h"

namespace Block {
	class Main;
};


class Block::Main
{
	private:

		Main(Main const &);
		Main &operator = (Main const &);

		Partition_table & _table();

		Env &_env;

		Attached_rom_dataspace _config           { _env, "config" };
		Attached_rom_dataspace _session_requests { _env, "session_requests" };

		Heap _heap { _env.ram(), _env.rm() };

		Constructible<Expanding_reporter> _reporter { };

		struct Sync_read_handler : Sync_read::Handler
		{
			Env &_env;

			Allocator_avl    _block_alloc;
			Block_connection _block;

			Io_signal_handler<Sync_read_handler> _io_sigh;

			/*
			 * The initial signal handler can be empty as it's only used
			 * to deblock wait_and_dispatch_one_io_signal().
			 */
			void _dummy_handle() { }

			Sync_read_handler(Env &env, Heap &heap)
			:
				_env         { env },
				_block_alloc { &heap },
				_block       { env, &_block_alloc, 64u << 10 },
				_io_sigh     { env.ep(), *this, &Sync_read_handler::_dummy_handle }
			{
				_block.sigh(_io_sigh);
			}

			Session::Info info() const { return _block.info(); }

			Block_connection & connection() override { return _block; }

			void block_for_io() override {
				_env.ep().wait_and_dispatch_one_io_signal(); }
		};

		Constructible<Mbr>  _mbr  { };
		Constructible<Gpt>  _gpt  { };
		Constructible<Ahdi> _ahdi { };
		Constructible<Disk> _disk { };

		Partition_table & _partition_table { _table() };

		struct Partition_number
		{
			long value;

			bool valid() const { return value != -1L; }

			static Partition_number from_xml(Node            const &node,
			                                 Partition_table const &table)
			{
				long const num = node.attribute_value("partition", -1L);
				bool const valid = num >= 0 && table.partition_valid(num);

				return Partition_number { .value = valid ? num : -1L };
			}
		};

		/* allow up to the support number of GPT partitions */
		static constexpr unsigned MAX_SESSIONS = 128u;

		struct Forwarded_session : Parent::Server
		{
			Parent::Client _parent_client { };

			Id_space<Parent::Client>::Element client_id;
			Id_space<Parent::Server>::Element server_id;

			Partition_number partition;

			Forwarded_session(Id_space<Parent::Client> &client_space,
			                  Id_space<Parent::Server> &server_space,
			                  Parent::Server::Id server_id,
			                  Partition_number partition)
			:
				client_id { _parent_client, client_space },
				server_id { *this, server_space, server_id },
				partition { partition }
			{ }
		};

		/*
		 * Store the sessions in an array where each element is constructed
		 * on demand and queried to determine if a partition is already
		 * in use.
		 */
		Constructible<Forwarded_session> _sessions[MAX_SESSIONS] { };

		Id_space<Parent::Server> server_id_space { };

		void _handle_session_request(Node const &request);

		void _handle_session_requests()
		{
			_session_requests.update();

			Node const &requests = _session_requests.node();

			requests.for_each_sub_node([&] (Node const &request) {
				_handle_session_request(request);
			});
		}

		Signal_handler<Main> session_request_handler {
			_env.ep(), *this, &Main::_handle_session_requests };

		using Request_result = Attempt<Session_capability,
		                               Parent::Session_response>;

		Request_result _request_session(Parent::Client::Id  const &id,
		                                Session_state::Args const &args,
		                                Affinity            const  affinity,
		                                Partition_number    const  partition,
		                                bool                const  writeable_policy)
		{
			size_t tx_buf_size =
				Arg_string::find_arg(args.string(), "tx_buf_size").ulong_value(0);

			if (!tx_buf_size) {
				error("tx_buf_size invalid");
				return Parent::Session_response::DENIED;
			}

			/*
			 * Quota arguments are not altered and passed on as-is because
			 * part_block should be configured with enough slack resources if
			 * need be.
			 */

			Ram_quota const ram_quota = ram_quota_from_args(args.string());
			if (tx_buf_size > ram_quota.value) {
				error("insufficient 'ram_quota', got ", ram_quota, ", need ",
				     tx_buf_size);
				return Parent::Session_response::INSUFFICIENT_RAM;
			}

			Cap_quota const cap_quota = cap_quota_from_args(args.string());
			if (Session::CAP_QUOTA > cap_quota.value) {
				error("insufficient 'cap_quota', got ", cap_quota, ", need ",
				     tx_buf_size);
				return Parent::Session_response::INSUFFICIENT_CAPS;
			}

			/* accommodate clients not constraining writeability */
			bool const writeable_arg =
				Arg_string::find_arg(args.string(), "writeable").bool_value(true);

			Block::Constrained_view const view {
				.offset     = Block::Constrained_view::Offset {
					_partition_table.partition_lba(partition.value) },
				.num_blocks = Block::Constrained_view::Num_blocks {
					_partition_table.partition_sectors(partition.value) },
				.writeable  = writeable_policy && writeable_arg
			};

			char argbuf[Parent::Session_args::MAX_SIZE];
			copy_cstring(argbuf, args.string(), sizeof(argbuf));

			Arg_string::set_arg(argbuf, sizeof(argbuf), "offset",      view.offset.value);
			Arg_string::set_arg(argbuf, sizeof(argbuf), "num_blocks",  view.num_blocks.value);
			Arg_string::set_arg(argbuf, sizeof(argbuf), "writeable",   view.writeable);

			try {
				return _env.session<Block::Session>(id, argbuf, affinity);
			} catch (...) { return Parent::Session_response::DENIED; }
		}

		static void _with_session_request(Node const &request,
		                                  auto const &create_fn,
		                                  auto const &upgrade_fn,
		                                  auto const &close_fn)
		{
			if (!request.has_attribute("id"))
				return;

			Parent::Server::Id const server_id { request.attribute_value("id", 0UL) };

			if (request.has_type("create")) {
				request.with_sub_node("args",
					[&] (Node const &node) {
						Session_state::Args const &args =
							Session_state::Args(Node::Quoted_content(node));

						Affinity const &affinity = Affinity::from_node(request);
						create_fn(server_id, args, affinity);
					},
					[] { });
			}
			else if (request.has_type("upgrade")) {

				size_t const ram_quota = request.attribute_value("ram_quota", 0UL);
				size_t const cap_quota = request.attribute_value("cap_quota", 0UL);

				upgrade_fn(server_id, Ram_quota { ram_quota }, Cap_quota { cap_quota });
			}
			else if (request.has_type("close"))
				close_fn(server_id);
		}

	public:

		struct Ambiguous_tables   : Exception { };
		struct Invalid_config     : Exception { };

		Main(Env &env) : _env(env)
		{
			_session_requests.sigh(session_request_handler);

			_env.parent().announce("Block");

			/* handle requests that have queued before or during construction */
			_handle_session_requests();
		}
};


void Block::Main::_handle_session_request(Node const &request)
{

	auto create_fn = [&] (Parent::Server::Id  const  server_id,
	                      Session_state::Args const &args,
	                      Affinity            const &affinity) {

		Session_label const &label = label_from_args(args.string());

		auto match_fn = [&] (Node const &policy) {

			Partition_number const partition =
				Partition_number::from_xml(policy, _partition_table);

			/* sessions are not writeable by default */
			bool const writeable_policy =
				policy.attribute_value("writeable", false);

			if (!partition.valid() || _sessions[partition.value].constructed()) {
				if (partition.valid()) error("partition ", partition.value, " already in use");
				else                   error("requested partition invalid");

				_env.parent().session_response(server_id, Parent::Session_response::DENIED);
				return;
			}

			_sessions[partition.value].construct(_env.id_space(),
			                                     server_id_space, server_id,
			                                     partition);

			Forwarded_session &session = *_sessions[partition.value];

			auto success_fn = [&] (Session_capability cap) {
				_env.parent().deliver_session_cap(server_id, cap); };

			auto error_fn = [&] (Parent::Session_response response) {
				_sessions[partition.value].destruct();
				error("could not forward session for partition ", partition.value);
				_env.parent().session_response(server_id, response);
			};

			_request_session(session.client_id.id(), args,
			                 affinity, partition,
			                 writeable_policy).with_result(success_fn, error_fn);
		};
		auto no_match_fn = [&] {
			error("no policy defined for '", label, "'");
			_env.parent().session_response(server_id, Parent::Session_response::DENIED);
		};

		with_matching_policy(label, _config.node(), match_fn, no_match_fn);
	};

	auto upgrade_fn = [&] (Parent::Server::Id const server_id,
	                      Ram_quota           const ram_quota,
	                      Cap_quota           const cap_quota) {

		auto upgrade_session_fn = [&] (Forwarded_session &session) {

			String<64> const args("ram_quota=", ram_quota, ", "
			                      "cap_quota=", cap_quota);

			_env.upgrade(session.client_id.id(), args.string());
			_env.parent().session_response(server_id, Parent::Session_response::OK);
		};
		server_id_space.apply<Forwarded_session>(server_id, upgrade_session_fn);
	};

	auto close_fn = [&] (Parent::Server::Id const server_id) {

		auto close_session_fn = [&] (Forwarded_session &session) {
			_env.close(session.client_id.id());

			Partition_number const partition = session.partition;
			_sessions[partition.value].destruct();

			_env.parent().session_response(server_id, Parent::Session_response::CLOSED);
		};

		server_id_space.apply<Forwarded_session>(server_id, close_session_fn);
	};

	_with_session_request(request, create_fn, upgrade_fn, close_fn);
}


Block::Partition_table & Block::Main::_table()
{
	Node const config = _config.node();

	bool const ignore_gpt = config.attribute_value("ignore_gpt", false);
	bool const ignore_mbr = config.attribute_value("ignore_mbr", false);

	bool valid_mbr  = false;
	bool valid_gpt  = false;
	bool pmbr_found = false;
	bool valid_ahdi = false;
	bool report     = false;

	if (ignore_gpt && ignore_mbr) {
		error("invalid configuration: cannot ignore GPT as well as MBR");
		throw Invalid_config();
	}

	config.with_optional_sub_node("report", [&] (Node const &node) {
		report = node.attribute_value("partitions", false); });

	try {
		if (report)
			_reporter.construct(_env, "partitions", "partitions");
	} catch (...) {
		error("cannot construct partitions reporter: abort");
		throw;
	}

	Sync_read_handler handler(_env, _heap);

	/*
	 * Try to parse MBR as well as GPT first if not instructued
	 * to ignore either one of them.
	 */

	if (!ignore_mbr) {
		using Parse_result = Mbr::Parse_result;

		_mbr.construct(_heap, handler.info());
		switch (_mbr->parse(handler)) {
		case Parse_result::MBR:
			valid_mbr = true;
			break;
		case Parse_result::PROTECTIVE_MBR:
			pmbr_found = true;
			break;
		case Parse_result::NO_MBR:
			break;
		}
	}

	if (!ignore_gpt) {
		_gpt.construct(_heap, handler.info());
		valid_gpt = _gpt->parse(handler);
	}

	_ahdi.construct(_heap, handler.info());
	valid_ahdi = _ahdi->parse(handler);

	/*
	 * Both tables are valid (although we would have expected a PMBR in
	 * conjunction with a GPT header - hybrid operation is not supported)
	 * and we will not decide which one to use, it is up to the user.
	 */
	if (valid_mbr && valid_gpt) {
		error("ambigious tables: found valid MBR as well as valid GPT");
		throw Ambiguous_tables();
	}

	if (valid_gpt && !pmbr_found) {
		warning("will use GPT without proper protective MBR");
	}

	if (pmbr_found && ignore_gpt) {
		warning("found protective MBR but GPT is to be ignored");
	}

	auto pick_final_table = [&] () -> Partition_table & {
		if (valid_gpt)  return *_gpt;
		if (valid_mbr)  return *_mbr;
		if (valid_ahdi) return *_ahdi;

		/* fall back to entire disk in partition 0 */
		_disk.construct(handler, _heap, handler.info());

		return *_disk;
	};

	Partition_table &table = pick_final_table();

	/* generate appropriate report */
	if (_reporter.constructed()) {
		_reporter->generate([&] (Generator &g) {
			table.generate_report(g);
		});
	}

	return table;
}


void Component::construct(Genode::Env &env) { static Block::Main main(env); }
