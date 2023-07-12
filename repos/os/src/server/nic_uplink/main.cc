/*
 * \brief  Component construct and main component object
 * \author Martin Stein
 * \date   2023-07-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/component.h>
#include <base/heap.h>
#include <root/component.h>
#include <base/attached_rom_dataspace.h>

/* nic_uplink includes */
#include <quota.h>
#include <assertion.h>

/* nic_router includes */
#include <session_creation.h>
#include <communication_buffer.h>
#include <list.h>

/* os includes */
#include <net/ethernet.h>
#include <nic/packet_allocator.h>
#include <uplink_session/rpc_object.h>
#include <nic_session/rpc_object.h>

using namespace Genode;
using namespace Net;

namespace Nic_uplink {

	class Main;
}

using namespace Nic_uplink;

namespace Net {

	enum { PKT_STREAM_QUEUE_SIZE = 1024 };

	template <typename... ARGS>
	void log_if(bool condition, ARGS &&... args)
	{
		if (condition)
			log(args...);
	}

	using Packet_descriptor = Genode::Packet_descriptor;
	using Packet_stream_policy = Genode::Packet_stream_policy<Packet_descriptor, PKT_STREAM_QUEUE_SIZE, PKT_STREAM_QUEUE_SIZE, char>;
	using Packet_stream_sink = Genode::Packet_stream_sink<Packet_stream_policy>;
	using Packet_stream_source = Genode::Packet_stream_source<Packet_stream_policy>;

	class Network_interface;
	class Uplink_session_component_base;
	class Uplink_session_component;
	class Uplink_session_root;
	class Nic_session_component_base;
	class Nic_session_component;
	class Nic_session_root;
	using Nic_session_list_item = List_element<Nic_session_component>;
	using Nic_session_list = Net::List<Nic_session_list_item>;
}


class Net::Network_interface
{
	public:

		using Label = String<32>;

	private:

		Packet_stream_sink &_sink;
		Packet_stream_source &_source;
		Label const _label;
		bool const _verbose;

	public:

		Network_interface(Packet_stream_sink &sink,
		                  Packet_stream_source &source,
		                  Label const &label,
		                  bool verbose)
		:
			_sink { sink },
			_source { source },
			_label { label },
			_verbose { verbose }
		{ }

		virtual ~Network_interface() { }

		template <typename GENERATE_PKT>
		void send_packet(size_t pkt_size, GENERATE_PKT && generate_pkt)
		{
			if (!_source.ready_to_submit()) {
				log_if(_verbose, "[", _label, "] failed to send packet");
				return;
			}
			_source.alloc_packet_attempt(pkt_size).with_result(
				[&] (Packet_descriptor pkt)
				{
					void *pkt_base { _source.packet_content(pkt) };
					generate_pkt(Byte_range_ptr { (char *)pkt_base, pkt_size });
					Size_guard size_guard(pkt_size);
					log_if(_verbose, "[", _label, "] snd ", Ethernet_frame::cast_from(pkt_base, size_guard));
					_source.try_submit_packet(pkt);
				},
				[&] (Packet_stream_source::Alloc_packet_error)
				{
					log_if(_verbose, "[", _label, "] failed to alloc packet");
				}
			);
		}

		void forward_packet(Byte_range_ptr const &src);

		template <typename HANDLE_PKT>
		void handle_received_packets(HANDLE_PKT && handle_pkt) const
		{
			while (_source.ack_avail()) {
				_source.release_packet(_source.try_get_acked_packet());
			}
			while (_sink.packet_avail()) {
				Packet_descriptor const pkt { _sink.get_packet() };
				handle_pkt(Byte_range_ptr { _sink.packet_content(pkt), pkt.size() });
				if (!_sink.try_ack_packet(pkt))
					log_if(_verbose, "[", _label, "] failed to ack packet");
			}
		}

		void wakeup_source() { _source.wakeup(); };

		void wakeup_sink() { _sink.wakeup(); }
};


class Net::Uplink_session_component_base
{
	protected:

		Session_env &_session_env;
		Heap _alloc;
		Nic::Packet_allocator _packet_alloc;
		Communication_buffer _tx_buf;
		Communication_buffer _rx_buf;

	public:

		Uplink_session_component_base(Session_env &session_env,
		                              size_t const tx_buf_size,
		                              size_t const rx_buf_size);
};


class Net::Uplink_session_component
:
	private Uplink_session_component_base,
	public ::Uplink::Session_rpc_object
{
	private:

		Main &_main;
		Ram_dataspace_capability const _ram_ds;
		Network_interface _net_if;
		Signal_handler<Uplink_session_component> _pkt_stream_signal_handler;

		void _handle_pkt_stream_signal();

	public:

		Uplink_session_component(Session_env &session_env,
		                         size_t const tx_buf_size,
		                         size_t const rx_buf_size,
		                         Ram_dataspace_capability const ram_ds,
		                         Main &main);

		void forward_packet(Byte_range_ptr const &src) { _net_if.forward_packet(src); }

		void wakeup_source() { _net_if.wakeup_source(); };

		void wakeup_sink() { _net_if.wakeup_sink(); }


		/***************
		 ** Accessors **
		 ***************/

		Ram_dataspace_capability ram_ds() const { return _ram_ds; };
		Session_env const &session_env() const { return _session_env; };
};


class Net::Uplink_session_root
:
	public Root_component<Uplink_session_component>
{
	private:

		Env &_env;
		Quota &_shared_quota;
		Main &_main;


		/********************
		 ** Root_component **
		 ********************/

		Uplink_session_component *_create_session(char const *args) override;
		void _destroy_session(Uplink_session_component *session) override;

	public:

		Uplink_session_root(Env &env,
		                    Allocator &alloc,
		                    Quota &shared_quota,
		                    Main &main);
};


class Net::Nic_session_component_base
{
	protected:

		Session_env &_session_env;
		Heap _alloc;
		Nic::Packet_allocator _packet_alloc;
		Communication_buffer _tx_buf;
		Communication_buffer _rx_buf;

	public:

		Nic_session_component_base(Session_env &session_env,
		                           size_t const tx_buf_size,
		                           size_t const rx_buf_size);
};


class Net::Nic_session_component
:
	private Nic_session_component_base,
	public ::Nic::Session_rpc_object
{
	private:

		Main &_main;
		Ram_dataspace_capability const _ram_ds;
		Network_interface _net_if;
		Signal_handler<Nic_session_component> _pkt_stream_signal_handler;
		Signal_context_capability _link_state_sigh { };
		Nic_session_list_item _list_item { this };

		void _handle_pkt_stream_signal();

	public:

		Nic_session_component(Session_env &session_env,
		                      size_t const tx_buf_size,
		                      size_t const rx_buf_size,
		                      Ram_dataspace_capability const ram_ds,
		                      Main &main);

		void forward_packet(Byte_range_ptr const &src) { _net_if.forward_packet(src); }

		void submit_link_state_signal();

		void wakeup_source() { _net_if.wakeup_source(); };

		void wakeup_sink() { _net_if.wakeup_sink(); }


		/******************
		 ** Nic::Session **
		 ******************/

		Mac_address mac_address() override;
		bool link_state() override;
		void link_state_sigh(Signal_context_capability sigh) override;


		/***************
		 ** Accessors **
		 ***************/

		Ram_dataspace_capability ram_ds() const { return _ram_ds; };
		Session_env const &session_env() const { return _session_env; };

		template <typename FUNC>
		void with_list_item(FUNC && func) { func(_list_item); }
};


class Net::Nic_session_root
:
	public Root_component<Nic_session_component>
{
	private:

		Env &_env;
		Quota &_shared_quota;
		Main &_main;


		/********************
		 ** Root_component **
		 ********************/

		Nic_session_component *_create_session(char const *args) override;
		void _destroy_session(Nic_session_component *session) override;

	public:

		Nic_session_root(Env &env,
		                 Allocator &alloc,
		                 Quota &shared_quota,
		                 Main &main);
};


class Nic_uplink::Main
{
	private:

		Env &_env;
		Quota _shared_quota { };
		Heap _heap { &_env.ram(), &_env.rm() };
		Uplink_session_component *_uplink_session_ptr { nullptr };
		Nic_session_list _nic_session_list { };
		Uplink_session_root _uplink_session_root { _env, _heap, _shared_quota, *this };
		Nic_session_root _nic_session_root { _env, _heap, _shared_quota, *this };
		bool _nic_service_announced { false };
		Mac_address _uplink_mac { };
		bool _uplink_mac_valid { false };
		Attached_rom_dataspace _config_rom { _env, "config" };
		bool const _verbose { _config_rom.xml().attribute_value("verbose", false) };

		Main(Main const &) = delete;

		Main & operator = (Main const &) = delete;

	public:

		Main(Env &env);

		bool verbose() const { return _verbose; }

		bool ready_to_manage_uplink_session() const { return !_uplink_session_ptr; }

		void manage_uplink_session(Uplink_session_component &session,
		                           Mac_address const &mac);

		template <typename FUNC>
		void with_uplink_session(FUNC && func)
		{
			if (_uplink_session_ptr)
				func(*_uplink_session_ptr, _uplink_mac);
		}

		void dissolve_uplink_session(Uplink_session_component &session);

		void manage_nic_session(Nic_session_component &session);

		template <typename FUNC>
		void for_each_nic_session(FUNC && func)
		{
			_nic_session_list.for_each([&] (Nic_session_list_item &list_item)
			{
				func(*list_item.object());
			});
		}

		void dissolve_nic_session(Nic_session_component &session);
};


/*************************************
 ** Net::Nic_session_component_base **
 *************************************/

Nic_session_component_base::Nic_session_component_base(Session_env &session_env,
                                                       size_t const tx_buf_size,
                                                       size_t const rx_buf_size)
:
	_session_env { session_env },
	_alloc { _session_env, _session_env },
	_packet_alloc { &_alloc },
	_tx_buf { _session_env, tx_buf_size },
	_rx_buf { _session_env, rx_buf_size }
{ }


/********************************
 ** Net::Nic_session_component **
 ********************************/

Net::Nic_session_component::Nic_session_component(Session_env &session_env,
                                                  size_t const tx_buf_size,
                                                  size_t const rx_buf_size,
                                                  Ram_dataspace_capability const ram_ds,
                                                  Main &main)
:
	Nic_session_component_base { session_env, tx_buf_size,rx_buf_size },
	Session_rpc_object {
		_session_env, _tx_buf.ds(), _rx_buf.ds(), &_packet_alloc,
		_session_env.ep().rpc_ep() },
	_main { main },
	_ram_ds { ram_ds },
	_net_if { *_tx.sink(), *_rx.source(), "nic", _main.verbose() },
	_pkt_stream_signal_handler { session_env.ep(), *this, &Nic_session_component::_handle_pkt_stream_signal }
{
	/* install packet stream signal handlers */
	_tx.sigh_packet_avail(_pkt_stream_signal_handler);
	_rx.sigh_ack_avail(_pkt_stream_signal_handler);

	/*
	 * We do not install ready_to_submit because submission is only triggered by
	 * incoming packets (and dropped if the submit queue is full).
	 * The ack queue should never be full otherwise we'll be leaking packets.
	 */
}


void Net::Nic_session_component::_handle_pkt_stream_signal()
{
	_net_if.handle_received_packets([&] (Byte_range_ptr const &src) {

		Size_guard size_guard { src.num_bytes };
		Ethernet_frame &eth { Ethernet_frame::cast_from(src.start, size_guard) };
		log_if(_main.verbose(), "[nic] rcv ", eth);

		_main.with_uplink_session([&] (Uplink_session_component &uplink_session,
		                               Mac_address const &)
		{
			uplink_session.forward_packet(src);
		});
	});
	_main.with_uplink_session([&] (Uplink_session_component &uplink_session,
	                               Mac_address const &)
	{
		uplink_session.wakeup_source();
	});
	wakeup_sink();
}


Mac_address Net::Nic_session_component::mac_address()
{
	Mac_address result { };
	_main.with_uplink_session([&] (Uplink_session_component &,
	                               Mac_address const &uplink_mac)
	{
		result = uplink_mac;
	});
	return result;
}


bool Net::Nic_session_component::link_state()
{
	bool result { false };
	_main.with_uplink_session([&] (Uplink_session_component &,
	                               Mac_address const &)
	{
		result = true;
	});
	return result;
}


void Net::Nic_session_component::link_state_sigh(Signal_context_capability sigh)
{
	_link_state_sigh = sigh;
}


void Net::Nic_session_component::submit_link_state_signal()
{
	Signal_transmitter { _link_state_sigh }.submit();
}


/****************************
 ** Net::Network_interface **
 ****************************/

void Net::Network_interface::forward_packet(Byte_range_ptr const &src)
{
	send_packet(src.num_bytes, [&] (Byte_range_ptr const &dst) {
		memcpy(dst.start, src.start, dst.num_bytes);
	});
}


/****************************************
 ** Net::Uplink_session_component_base **
 ****************************************/

Net::Uplink_session_component_base::
Uplink_session_component_base(Session_env &session_env,
                              size_t const tx_buf_size,
                              size_t const rx_buf_size)
:
	_session_env { session_env },
	_alloc { _session_env, _session_env },
	_packet_alloc { &_alloc },
	_tx_buf { _session_env, tx_buf_size },
	_rx_buf { _session_env, rx_buf_size }
{ }


/***********************************
 ** Net::Uplink_session_component **
 ***********************************/

void Net::Uplink_session_component::_handle_pkt_stream_signal()
{
	_net_if.handle_received_packets([&] (Byte_range_ptr const &src)
	{
		Size_guard size_guard { src.num_bytes };
		Ethernet_frame &eth { Ethernet_frame::cast_from(src.start, size_guard) };
		log_if(_main.verbose(), "[uplink] rcv ", eth);

		_main.for_each_nic_session([&] (Nic_session_component &nic_session)
		{
			nic_session.forward_packet(src);
		});
	});
	_main.for_each_nic_session([&] (Nic_session_component &nic_session)
	{
		nic_session.wakeup_source();
	});
	wakeup_sink();
}


Net::Uplink_session_component::Uplink_session_component(Session_env &session_env,
                                                        size_t const tx_buf_size,
                                                        size_t const rx_buf_size,
                                                        Ram_dataspace_capability const ram_ds,
                                                        Main &main)
:
	Uplink_session_component_base { session_env, tx_buf_size,rx_buf_size },
	Session_rpc_object {
		_session_env, _tx_buf.ds(), _rx_buf.ds(), &_packet_alloc,
		_session_env.ep().rpc_ep() },
	_main { main },
	_ram_ds { ram_ds },
	_net_if { *_tx.sink(), *_rx.source(), "uplink", _main.verbose() },
	_pkt_stream_signal_handler { session_env.ep(), *this, &Uplink_session_component::_handle_pkt_stream_signal }
{
	/* install packet stream signal handlers */
	_tx.sigh_packet_avail(_pkt_stream_signal_handler);
	_rx.sigh_ack_avail(_pkt_stream_signal_handler);

	/*
	 * We do not install ready_to_submit because submission is only triggered
	 * by incoming packets (and dropped if the submit queue is full).
	 * The ack queue should never be full otherwise we'll be leaking packets.
	 */
}


/******************************
 ** Net::Uplink_session_root **
 ******************************/

Net::Uplink_session_root::Uplink_session_root(Env &env,
                                              Allocator &alloc,
                                              Quota &shared_quota,
                                              Main &main)
:
	Root_component<Uplink_session_component> { &env.ep().rpc_ep(), &alloc },
	_env { env },
	_shared_quota { shared_quota },
	_main { main }
{ }


Uplink_session_component *
Net::Uplink_session_root::_create_session(char const *args)
{
	Session_creation<Uplink_session_component> session_creation { };
	if (!_main.ready_to_manage_uplink_session()) {
		log_if(_main.verbose(), "[uplink] failed to manage new session");
		throw Service_denied();
	}
	try {
		return session_creation.execute(
			_env, _shared_quota, args,
			[&] (Session_env &session_env, void *session_at, Ram_dataspace_capability ram_ds)
			{
				enum { MAC_STR_LENGTH = 19 };
				char mac_str [MAC_STR_LENGTH];
				Arg mac_arg { Arg_string::find_arg(args, "mac_address") };

				if (!mac_arg.valid()) {
					log_if(_main.verbose(), "[uplink] failed to find 'mac_address' arg");
					throw Service_denied();
				}
				mac_arg.string(mac_str, MAC_STR_LENGTH, "");
				Mac_address mac { };
				ascii_to(mac_str, mac);
				if (mac == Mac_address { }) {
					log_if(_main.verbose(), "[uplink] malformed 'mac_address' arg");
					throw Service_denied();
				}
				Uplink_session_component &session {
					*construct_at<Uplink_session_component>(
						session_at,
						session_env,
						Arg_string::find_arg(args, "tx_buf_size").ulong_value(0),
						Arg_string::find_arg(args, "rx_buf_size").ulong_value(0),
						ram_ds, _main) };

				_main.manage_uplink_session(session, mac);
				return &session;
			});
	}
	catch (Region_map::Invalid_dataspace) {
		log_if(_main.verbose(), "[uplink] failed to attach RAM");
		throw Service_denied();
	}
	catch (Region_map::Region_conflict) {
		log_if(_main.verbose(), "[uplink] failed to attach RAM");
		throw Service_denied();
	}
	catch (Out_of_ram) {
		log_if(_main.verbose(), "[uplink] insufficient session RAM quota");
		throw Insufficient_ram_quota();
	}
	catch (Out_of_caps) {
		log_if(_main.verbose(), "[uplink] insufficient session CAP quota");
		throw Insufficient_cap_quota();
	}
}

void Net::Uplink_session_root::_destroy_session(Uplink_session_component *session_ptr)
{
	_main.dissolve_uplink_session(*session_ptr);

	/* read out initial dataspace and session env and destruct session */
	Ram_dataspace_capability ram_ds { session_ptr->ram_ds() };
	Session_env const &session_env { session_ptr->session_env() };
	session_ptr->~Uplink_session_component();

	/* copy session env to stack and detach/free all session data */
	Session_env session_env_stack { session_env };
	session_env_stack.detach(session_ptr);
	session_env_stack.detach(&session_env);
	session_env_stack.free(ram_ds);

	/* check for leaked quota */
	if (session_env_stack.ram_guard().used().value)
		log_if(_main.verbose(), "[uplink] session leaks RAM quota of ",
		      session_env_stack.ram_guard().used().value, " byte(s)");
	if (session_env_stack.cap_guard().used().value)
		log_if(_main.verbose(), "[uplink] session leaks CAP quota of ",
		    session_env_stack.cap_guard().used().value, " cap(s)");
}


/***************************
 ** Net::Nic_session_root **
 ***************************/

Net::Nic_session_root::Nic_session_root(Env &env,
                                        Allocator &alloc,
                                        Quota &shared_quota,
                                        Main &main)
:
	Root_component<Nic_session_component> { &env.ep().rpc_ep(), &alloc },
	_env { env },
	_shared_quota { shared_quota },
	_main { main }
{ }


Nic_session_component *Net::Nic_session_root::_create_session(char const *args)
{
	Session_creation<Nic_session_component> session_creation { };
	try {
		return session_creation.execute(
			_env, _shared_quota, args,
			[&] (Session_env &session_env, void *session_at, Ram_dataspace_capability ram_ds)
			{
				Nic_session_component &session {
					*construct_at<Nic_session_component>(
						session_at, session_env,
						Arg_string::find_arg(args, "tx_buf_size").ulong_value(0),
						Arg_string::find_arg(args, "rx_buf_size").ulong_value(0),
						ram_ds, _main) };

				_main.manage_nic_session(session);
				return &session;
			}
		);
	}
	catch (Region_map::Invalid_dataspace) {
		log_if(_main.verbose(), "[nic] failed to attach RAM");
		throw Service_denied();
	}
	catch (Region_map::Region_conflict) {
		log_if(_main.verbose(), "[nic] failed to attach RAM");
		throw Service_denied();
	}
	catch (Out_of_ram) {
		log_if(_main.verbose(), "[nic] insufficient session RAM quota");
		throw Insufficient_ram_quota();
	}
	catch (Out_of_caps) {
		log_if(_main.verbose(), "[nic] insufficient session CAP quota");
		throw Insufficient_cap_quota();
	}
}

void Net::Nic_session_root::_destroy_session(Nic_session_component *session_ptr)
{
	_main.dissolve_nic_session(*session_ptr);

	/* read out initial dataspace and session env and destruct session */
	Ram_dataspace_capability ram_ds { session_ptr->ram_ds() };
	Session_env const &session_env { session_ptr->session_env() };
	session_ptr->~Nic_session_component();

	/* copy session env to stack and detach/free all session data */
	Session_env session_env_stack { session_env };
	session_env_stack.detach(session_ptr);
	session_env_stack.detach(&session_env);
	session_env_stack.free(ram_ds);

	/* check for leaked quota */
	if (session_env_stack.ram_guard().used().value)
		log_if(_main.verbose(), "[nic] session leaks RAM quota of ",
		    session_env_stack.ram_guard().used().value, " byte(s)");
	if (session_env_stack.cap_guard().used().value)
		log_if(_main.verbose(), "[nic] session leaks CAP quota of ",
		    session_env_stack.cap_guard().used().value, " cap(s)");
}


/**********************
 ** Nic_uplink::Main **
 **********************/

Nic_uplink::Main::Main(Env &env)
:
	_env { env }
{
	_env.parent().announce(_env.ep().manage(_uplink_session_root));
}


void Nic_uplink::Main::manage_uplink_session(Uplink_session_component &session,
                                             Mac_address const &mac)
{
	ASSERT(!_uplink_session_ptr);

	if (_uplink_mac_valid)
		ASSERT(_uplink_mac == mac);

	_uplink_session_ptr = &session;
	_uplink_mac = mac;

	if (!_nic_service_announced) {
		_env.parent().announce(_env.ep().manage(_nic_session_root));
		_nic_service_announced = true;
	}
	for_each_nic_session([&] (Nic_session_component &nic_session)
	{
		nic_session.submit_link_state_signal();
	});
	log_if(_verbose, "[uplink] session created! mac=", _uplink_mac);
}


void Nic_uplink::Main::manage_nic_session(Nic_session_component &session)
{
	session.with_list_item([&] (Nic_session_list_item &item)
	{
		_nic_session_list.insert(&item);
	});
	log_if(_verbose, "[nic] session created!");
}


void Nic_uplink::Main::dissolve_uplink_session(Uplink_session_component &session)
{
	ASSERT(_uplink_session_ptr == &session);
	_uplink_session_ptr = nullptr;
	for_each_nic_session([&] (Nic_session_component &nic_session)
	{
		nic_session.submit_link_state_signal();
	});
	log_if(_verbose, "[uplink] session dissolved!");
}


void Nic_uplink::Main::dissolve_nic_session(Nic_session_component &session)
{
	session.with_list_item([&] (Nic_session_list_item &item)
	{
		_nic_session_list.remove(&item);
	});
	log_if(_verbose, "[nic] session dissolved!");
}


/***********************
 ** Genode::Component **
 ***********************/

void Component::construct(Env &env) { static Nic_uplink::Main main { env }; }
