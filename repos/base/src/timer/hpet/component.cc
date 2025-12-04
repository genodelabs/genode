/*
 * \brief  Timer driver for the HPET
 * \author Alexander Boettcher
 * \date   2025-07-05
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_io_mem_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/session_object.h>
#include <util/mmio.h>
#include <irq_session/connection.h>
#include <root/component.h>
#include <timer_session/timer_session.h>
#include <trace/timestamp.h>

/* base-internal includes */
#include <base/internal/alarm_registry.h>

namespace Timer {

	using namespace Genode;

	struct Clock;
	struct Device;
	struct Alarm;
	struct Root;
	struct Session_component;
	struct Main;

	using Alarms = Alarm_registry<Alarm, Clock>;
}


struct Timer::Clock
{
	uint64_t us;

	static constexpr uint64_t MASK = ~0ULL;

	uint64_t value() const { return us; }

	void print(Output &out) const { Genode::print(out, us / 1000); }
};


class Timer::Device : Noncopyable
{
	private:

		struct Hpet : Genode::Mmio<1024>
		{
			struct LowCap : Register<0x00, 32> {
				struct Timers     : Bitfield< 8, 5> { };
				struct Cnt_size   : Bitfield<13, 1> { };
				struct Legacy_cap : Bitfield<15, 1> { };
				struct Vendor_id  : Bitfield<16,16> { };
			};
			struct Clk_period : Register<0x04, 32> { };
			struct Config     : Register<0x10, 32> {
				struct Enable : Bitfield<0x0, 1> { };
				struct Legacy : Bitfield<0x1, 1> { };
			};
			struct Irq_status : Register<0x20, 32> { };
			struct Counter    : Register<0xf0, 64> { };

			struct Timer0 : Register<0x100, 32> {
				struct Edge_level     : Bitfield< 1,  1> { };
				struct Irq_enable     : Bitfield< 2,  1> { };
				struct Irq_type       : Bitfield< 3,  1> { };
				struct Irq_type_cap   : Bitfield< 4,  1> { };
				struct Irq_route      : Bitfield< 9,  5> { };
				struct Irq_fsb_enable : Bitfield<14,  1> { };
				struct Irq_fsb_cap    : Bitfield<15,  1> { };
			};
			struct Timer0_route_cap : Register<0x104, 32> { };
			struct Timer0_cnt       : Register<0x108, 64> { };
			struct Timer0_fsb_value : Register<0x110, 32> { };
			struct Timer0_fsb_addr  : Register<0x114, 32> { };

			struct Timer1     : Register<0x120, 32> {
				struct Irq_type_cap   : Bitfield< 4,  1> { };
			};
			struct Timer1_route_cap : Register<0x124, 32> { };

			struct Timer2     : Register<0x140, 32> {
				struct Edge_level     : Bitfield< 1,  1> { };
				struct Irq_enable     : Bitfield< 2,  1> { };
				struct Irq_type       : Bitfield< 3,  1> { };
				struct Irq_type_cap   : Bitfield< 4,  1> { };
				struct Irq_route      : Bitfield< 9,  5> { };
				struct Irq_fsb_enable : Bitfield<14,  1> { };
				struct Irq_fsb_cap    : Bitfield<15,  1> { };
			};
			struct Timer2_route_cap : Register<0x144, 32> { };
			struct Timer2_cnt       : Register<0x148, 64> { };
			struct Timer2_fsb_value : Register<0x150, 32> { };
			struct Timer2_fsb_addr  : Register<0x154, 32> { };

			Hpet(Byte_range_ptr const &range) : Mmio(range) { }
		};

	public:

		struct Wakeup_dispatcher : Interface
		{
			virtual void dispatch_device_wakeup() = 0;
		};

		struct Deadline : Clock { bool infinite { }; };

	private:

		struct Counter { uint64_t value; };

		typedef Attached_io_mem_dataspace Io_dataspace;

		Counter  _counter_max  { };
		Counter  _counter_last { };
		unsigned _wrap_count   { };
		unsigned _hpet_gsi     { };
		unsigned _timer_id     { };
		bool     _level        { };
		bool     _msi          { };

		Env               &_env;
		Wakeup_dispatcher &_dispatcher;

		Io_dataspace   _io_mem;
		Hpet           _hpet { { _io_mem.local_addr<char>(), 1024 } };

		Constructible<Irq_connection> _timer_irq { };

		Signal_handler<Device> _handler { _env.ep(), *this,
			                              &Device::_handle_timeout };

		uint64_t freq_mhz() const
		{
			static auto const mhz = 1000000000000000ULL / 1000ULL / 1000ULL
			                      / _hpet.read<Hpet::Clk_period>();
			return mhz;
		}

		uint64_t _convert_counter_to_us(Counter const &diff) const {
			return diff.value / freq_mhz(); }

		Counter _convert_us_to_counter(uint64_t const us) const {
			return { .value = min(us * freq_mhz(), _counter_max.value) }; }

		void _handle_timeout()
		{
			if (_level)
				_hpet.write<Hpet::Irq_status>(1u << _timer_id);

			_dispatcher.dispatch_device_wakeup();

			if (_level)
				_timer_irq->ack_irq();

		}

		bool _set_counter(Counter const &cnt)
		{
			if (_timer_id == 0) _hpet.write<Hpet::Timer0_cnt>(cnt.value);
			if (_timer_id == 2) _hpet.write<Hpet::Timer2_cnt>(cnt.value);

			auto counter =_hpet.read<Hpet::Counter>();

			return counter < cnt.value;
		}

		void _advance_current_time()
		{
			Counter const current { _hpet.read<Hpet::Counter>() };
			bool    const wrapped = current.value < _counter_last.value;

			if (wrapped)
				_wrap_count ++;

			_counter_last = current;
		}

		uint64_t curr_time_us()
		{
			uint64_t us = _convert_counter_to_us(_counter_last);

			if (_wrap_count)
				us += _wrap_count * _convert_counter_to_us(_counter_max);

			return us;
		}

		unsigned _determine_hpet_irq()
		{
			/* femptoseconds 10^15 */
			auto period_fs  = _hpet.read<Hpet::Clk_period>();
			auto timer_cnt  = _hpet.read<Hpet::LowCap::Timers>() + 1;
			auto cnt_size   = _hpet.read<Hpet::LowCap::Cnt_size>();
			auto legacy_cap = _hpet.read<Hpet::LowCap::Legacy_cap>();
			auto vendor_id  = _hpet.read<Hpet::LowCap::Vendor_id>();

			_counter_max = Counter(cnt_size ? ~0ULL : ~0UL);
			_timer_id    = timer_cnt >= 2 ? 2 : 0;

			log("timers=", timer_cnt, ", clock ", period_fs, "fs ",
			    cnt_size ? ", 64bit" : ", 32bit", " counter ",
			    legacy_cap ? ", legacy routing support" : "",
			    " vendor=", Hex(vendor_id));

			log("timer0 GSI options ", Hex(_hpet.read<Hpet::Timer0_route_cap>()),
				_hpet.read<Hpet::Timer1::Irq_type_cap>() ? ", supports periodic" : "");

			if (timer_cnt > 1)
				log("timer1 GSI options ", Hex(_hpet.read<Hpet::Timer1_route_cap>()),
				    _hpet.read<Hpet::Timer1::Irq_type_cap>() ? ", supports periodic" : "");

			if (timer_cnt > 2)
				log("timer2 GSI options ", Hex(_hpet.read<Hpet::Timer2_route_cap>()),
				    _hpet.read<Hpet::Timer2::Irq_type_cap>() ? ", supports periodic" : "");

			log("time wraps after ",
			    _convert_counter_to_us(_counter_max) / 1000 / 1000 / 60 / 60
			                                         / 24 / 365, " years");

			auto route_cap = _hpet.read<Hpet::Timer0_route_cap>();

			if (_timer_id == 2) {
				route_cap = _hpet.read<Hpet::Timer2_route_cap>();
			}

			auto ioapic_gsi = 0u;

			for (int i = 0; i < 64; i++) {
				if ((1ul << i) & route_cap) {
					ioapic_gsi = i;
					break;
				}
			}

			log("irqs=", _hpet.read<Hpet::Config::Enable>() ? "on " : "off ",
			    "legacy=", _hpet.read<Hpet::Config::Legacy>() ? "on" : "off");

			if (0) {
				log("enable legacy routing support");
				_hpet.write<Hpet::Config::Legacy>(1);
			}

			auto fsb_cap = _hpet.read<Hpet::Timer0::Irq_fsb_cap>();
			if (_timer_id == 2)
				fsb_cap  = _hpet.read<Hpet::Timer2::Irq_fsb_cap>();

			/* heuristic */
			_msi = vendor_id == 0x8086 && fsb_cap;
			if (!_msi && ioapic_gsi >= 16) _level = true;

			_hpet_gsi = ioapic_gsi;

			return _hpet_gsi;
		}

	public:

		Device(Env &env, Wakeup_dispatcher &dispatcher, Xml_node const &config)
		:
			_env(env), _dispatcher(dispatcher),
			_io_mem(_env, config.attribute_value("mmio", 0xfed00000ul), 4096)
		{
			auto const gsi = _determine_hpet_irq();

			if (_msi) {
				unsigned bdf = config.attribute_value("bdf", (0x1e << 3) | 6);
				auto base = config.attribute_value("pci_base", 0xe0000000ul);

				_timer_irq.construct(_env, gsi, base +
				                     0x1000 * 8 * 256 * ((bdf >> 8) & 0xff) +
				                     0x1000 * 8       * ((bdf >> 3) & 0x1f) +
				                     0x1000 *           ((bdf >> 0) & 0x07),
				                     Irq_session::TYPE_MSI, bdf);
			}
			else
				_timer_irq.construct(_env, gsi,
				                     _level ? Irq_session::Trigger::TRIGGER_LEVEL
				                            : Irq_session::Trigger::TRIGGER_EDGE);

			_timer_irq->sigh(_handler);

			auto info = _timer_irq->info();

			if (info.type == Irq_session::Info::Type::MSI) {
				log("timer ", _timer_id, ": using MSI ", Hex(info.address),
				    " ", Hex(info.value), " edge triggered");

				if (_timer_id == 0) {
					_hpet.write<Hpet::Timer0_fsb_value>(unsigned(info.value));
					_hpet.write<Hpet::Timer0_fsb_addr> (unsigned(info.address));
					_hpet.write<Hpet::Timer0::Irq_fsb_enable>(1);
				}
				if (_timer_id == 2) {
					_hpet.write<Hpet::Timer2_fsb_value>(unsigned(info.value));
					_hpet.write<Hpet::Timer2_fsb_addr> (unsigned(info.address));
					_hpet.write<Hpet::Timer2::Irq_fsb_enable>(1);
				}
			} else {

				auto fsb_cap   = _hpet.read<Hpet::Timer0::Irq_fsb_cap>();
				auto route_cap = _hpet.read<Hpet::Timer0_route_cap>();
				if (_timer_id == 2) {
					fsb_cap   = _hpet.read<Hpet::Timer2::Irq_fsb_cap>();
					route_cap = _hpet.read<Hpet::Timer2_route_cap>();
				}

				log("Using timer ", _timer_id, fsb_cap ? ", MSI capable" : "",
				    " -> using GSI ", _hpet_gsi, _level ? " level" : " edge",
				    " triggered");

				if (route_cap & (1u << _hpet_gsi)) {

					if (_timer_id == 0)
						_hpet.write<Hpet::Timer0::Irq_route>(_hpet_gsi);
					else
					if (_timer_id == 2)
						_hpet.write<Hpet::Timer2::Irq_route>(_hpet_gsi);
					else {
						error("timer ", _timer_id, " not supported");
						sleep_forever();
					}
				} else {
					error("GSI ", _hpet_gsi, " not available");
					sleep_forever();
				}
			}

			if (_timer_id == 0) {
				_hpet.write<Hpet::Timer0::Edge_level>(_level ? 1 : 0);
				_hpet.write<Hpet::Timer0::Irq_enable>(1);
			}
			if (_timer_id == 2) {
				_hpet.write<Hpet::Timer2::Edge_level>(_level ? 1 : 0);
				_hpet.write<Hpet::Timer2::Irq_enable>(1);
			}

			_hpet.write<Hpet::Config::Enable>(1);

			_handle_timeout();
		}

		Clock now()
		{
			_advance_current_time();

			return Clock { .us = curr_time_us() };
		}

		bool update_deadline(Deadline const abs)
		{
			static auto const wrap_us = _convert_counter_to_us(_counter_max);

			if (abs.infinite)
				return true;

			auto const now_us = now().us;

			if (abs.us <= now_us)
				return false;

			auto deadline = abs;
			if (abs.us >= wrap_us)
				deadline.us = abs.us % wrap_us;

			auto const hpet_cnt = _convert_us_to_counter(deadline.us);

			return _set_counter(hpet_cnt);
		}

		void notify() { _handler.local_submit(); }
};


struct Timer::Alarm : Alarms::Element
{
	Session_component &session;

	Alarm(Alarms &alarms, Session_component &session, Clock time)
	:
		Alarms::Element(alarms, *this, time), session(session)
	{ }

	void print(Output &out) const;
};


static Timer::Device::Deadline next_deadline(Timer::Alarms &alarms)
{
	using namespace Timer;

	return alarms.soonest(Clock { 0 }).convert<Device::Deadline>(
		[&] (Clock soonest) -> Device::Deadline {

			/* scan alarms for a cluster nearby the soonest */
			uint64_t const MAX_DELAY_US = 250;
			Device::Deadline result { soonest.us };
			alarms.for_each_in_range(soonest, Clock { soonest.us + MAX_DELAY_US },
			[&] (Alarm const &alarm) {
				result.us = max(result.us, alarm.time.us); });

			return result;
		},
		[&] (Alarms::None) {
			Device::Deadline deadline { };
			deadline.infinite = true;
			return deadline;
		});
}


struct Timer::Session_component : Session_object<Timer::Session, Session_component>
{
	Alarms &_alarms;
	Device &_device;

	Signal_context_capability _sigh { };

	Clock const _creation_time = _device.now();

	uint64_t _local_now_us() const { return _device.now().us - _creation_time.us; }

	struct Period { uint64_t us; };

	Constructible<Period> _period { };
	Constructible<Alarm>  _alarm  { };

	Session_component(Env             &env,
	                  Resources const &resources,
	                  Label     const &label,
	                  Alarms          &alarms,
	                  Device          &device)
	:
		Session_object(env.ep(), resources, label),
		_alarms(alarms), _device(device)
	{ }

	/**
	 * Called by Device::Wakeup_dispatcher
	 */
	void handle_wakeup()
	{
		if (_sigh.valid())
			Signal_transmitter(_sigh).submit();

		if (_period.constructed()) {
			Clock const next = _alarm.constructed()
			                 ? Clock { _alarm->time.us  + _period->us }
			                 : Clock { _device.now().us + _period->us };

			_alarm.construct(_alarms, *this, next);

		} else /* response of 'trigger_once' */ {
			_alarm.destruct();
		}
	}

	/******************************
	 ** Timer::Session interface **
	 ******************************/

	void trigger_once(uint64_t rel_us) override
	{
		_period.destruct();
		_alarm.destruct();

		Clock const now = _device.now();

		rel_us = max(rel_us, 250u);
		_alarm.construct(_alarms, *this, Clock { now.us + rel_us });

		if (!_device.update_deadline(next_deadline(_alarms)))
			_device.notify();
	}

	void trigger_periodic(uint64_t period_us) override
	{
		_period.destruct();
		_alarm.destruct();

		if (period_us) {
			period_us = max(period_us, 1000u);
			_period.construct(period_us);
			handle_wakeup();
		}

		if (!_device.update_deadline(next_deadline(_alarms)))
			_device.notify();
	}

	void sigh(Signal_context_capability sigh) override { _sigh = sigh; }

	uint64_t elapsed_ms() const override { return _local_now_us()/1000; }
	uint64_t elapsed_us() const override { return _local_now_us(); }

	void msleep(uint64_t) override { }
	void usleep(uint64_t) override { }
};


struct Timer::Root : public Root_component<Session_component>
{
	private:

		Env    &_env;
		Alarms &_alarms;
		Device &_device;

	protected:

		Create_result _create_session(const char *args) override
		{
			return _alloc_obj(_env, session_resources_from_args(args),
			                  session_label_from_args(args), _alarms, _device);
		}

		void _upgrade_session(Session_component &s, const char *args) override
		{
			s.upgrade(ram_quota_from_args(args));
			s.upgrade(cap_quota_from_args(args));
		}

		void _destroy_session(Session_component &session) override
		{
			Genode::destroy(md_alloc(), &session);
		}

	public:

		Root(Env &env, Allocator &md_alloc, Alarms &alarms, Device &device)
		:
			Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _alarms(alarms), _device(device)
		{ }
};


void Timer::Alarm::print(Output &out) const { Genode::print(out, session.label()); }


struct Timer::Main : Device::Wakeup_dispatcher
{
	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	Device _device { _env, *this, _config.xml() };

	Alarms _alarms { };

	Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	Root _root { _env, _sliced_heap, _alarms, _device };

	/**
	 * Device::Wakeup_dispatcher
	 */
	void dispatch_device_wakeup() override
	{
		do {
			Clock const now = _device.now();

			/* handle and remove pending alarms */
			while (_alarms.with_any_in_range({ 0 }, now, [&] (Alarm &alarm) {
				alarm.session.handle_wakeup(); }));

			/* schedule next wakeup */
		} while (!_device.update_deadline(next_deadline(_alarms)));
	}

	Main(Genode::Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env) { static Timer::Main inst(env); }
