/*
 * \brief  Timer driver for the PIT
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2024-05-13
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/session_object.h>
#include <irq_session/connection.h>
#include <io_port_session/connection.h>
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

	static constexpr uint64_t MASK = uint64_t(-1);

	uint64_t value() const { return us; }

	void print(Output &out) const { Genode::print(out, us/1000); }
};


class Timer::Device : Noncopyable
{
	private:

		enum {
			PIT_TICKS_PER_SECOND = 1193182,
			PIT_MAX_COUNT        =   65535,
			PIT_MAX_USEC         = (1000ull * 1000 * PIT_MAX_COUNT) /
			                       (PIT_TICKS_PER_SECOND),

			PIT_DATA_PORT_0      =    0x40,  /* data port for PIT channel 0,
			                                    connected to the PIC */
			PIT_CMD_PORT         =    0x43,  /* PIT command port */
			IRQ_PIT              =       0,  /* timer interrupt at the PIC */

			/*
			 * Bit definitions for accessing the PIT command port
			 */
			PIT_CMD_SELECT_CHANNEL_0 = 0 << 6,
			PIT_CMD_ACCESS_LO        = 1 << 4,
			PIT_CMD_ACCESS_LO_HI     = 3 << 4,
			PIT_CMD_MODE_IRQ         = 0 << 1,
			PIT_CMD_MODE_RATE        = 2 << 1,

			PIT_CMD_READ_BACK        = 3 << 6,
			PIT_CMD_RB_COUNT         = 0 << 5,
			PIT_CMD_RB_STATUS        = 0 << 4,
			PIT_CMD_RB_CHANNEL_0     = 1 << 1,

			/*
			 * Bit definitions of the PIT status byte
			 */
			PIT_STAT_INT_LINE = 1 << 7,
		};

		/* PIT counter */
		struct Counter { uint16_t value; };

	public:

		struct Wakeup_dispatcher : Interface
		{
			virtual void dispatch_device_wakeup() = 0;
		};

		struct Deadline : Clock { };

		static constexpr Deadline infinite_deadline { uint64_t(-1) };

	private:

		Env &_env;

		Io_port_connection _io_port { _env, PIT_DATA_PORT_0,
		                              PIT_CMD_PORT - PIT_DATA_PORT_0 + 1 };

		Irq_connection _timer_irq { _env, unsigned(IRQ_PIT) };

		uint64_t _max_timeout_us { PIT_MAX_USEC };

		Wakeup_dispatcher &_dispatcher;

		Signal_handler<Device> _handler { _env.ep(), *this, &Device::_handle_timeout };

		uint64_t _curr_time_us      { };
		Counter  _last_read         { };
		bool     _wrap_handled      { };

		uint64_t _convert_counter_to_us(uint64_t counter)
		{
			/* round up to 1us in case of rest */
			auto const mod = (counter * 1000 * 1000) % PIT_TICKS_PER_SECOND;
			return (counter * 1000 * 1000 / PIT_TICKS_PER_SECOND)
			       + (mod ? 1 : 0);
		}

		Counter _convert_relative_us_to_counter(uint64_t rel_us)
		{
			return { .value = uint16_t(min(rel_us * PIT_TICKS_PER_SECOND / 1000 / 1000,
			                               uint64_t(PIT_MAX_COUNT))) };
		}

		void _handle_timeout()
		{
			_dispatcher.dispatch_device_wakeup();
			_timer_irq.ack_irq();
		}

		void _set_counter(Counter const &cnt)
		{
			/* wrap status gets reset by re-programming counter */
			_wrap_handled = false;

			_io_port.outb(PIT_DATA_PORT_0, uint8_t( cnt.value       & 0xff));
			_io_port.outb(PIT_DATA_PORT_0, uint8_t((cnt.value >> 8) & 0xff));
		}

		void _with_counter(auto const &fn)
		{
			/* read-back count of counter 0 */
			_io_port.outb(PIT_CMD_PORT, PIT_CMD_READ_BACK |
			                            PIT_CMD_RB_COUNT  |
			                            PIT_CMD_RB_STATUS |
			                            PIT_CMD_RB_CHANNEL_0);

			/* read status byte from latch register */
			uint8_t status = _io_port.inb(PIT_DATA_PORT_0);

			/* read low and high bytes from latch register */
			uint16_t lo = _io_port.inb(PIT_DATA_PORT_0);
			uint16_t hi = _io_port.inb(PIT_DATA_PORT_0);

			bool const wrapped = !!(status & PIT_STAT_INT_LINE);

			fn(Counter(uint16_t((hi << 8) | lo)), wrapped && !_wrap_handled);

			/* only handle wrap one time until next _set_counter */
			if (wrapped)
				_wrap_handled = true;
		}

		void _advance_current_time()
		{
			_with_counter([&](Counter const &pit, bool wrapped) {

				auto diff = (!wrapped && (_last_read.value >= pit.value))
				          ? _last_read.value - pit.value
				          : PIT_MAX_COUNT - pit.value + _last_read.value;

				_curr_time_us  += _convert_counter_to_us(diff);

				_last_read = pit;
			});
		}

	public:

		Device(Env &env, Wakeup_dispatcher &dispatcher)
		: _env(env), _dispatcher(dispatcher)
		{
			/* operate PIT in one-shot mode */
			_io_port.outb(PIT_CMD_PORT, PIT_CMD_SELECT_CHANNEL_0 |
			              PIT_CMD_ACCESS_LO_HI | PIT_CMD_MODE_IRQ);

			_timer_irq.sigh(_handler);

			_handle_timeout();
		}

		Clock now()
		{
			_advance_current_time();

			return Clock { .us = _curr_time_us };
		}

		void update_deadline(Deadline const deadline)
		{
			uint64_t const now_us = now().us;
			uint64_t const rel_us = (deadline.us > now_us)
			                      ? min(_max_timeout_us, deadline.us - now_us)
			                      : 1;

			auto const pit_cnt = _convert_relative_us_to_counter(rel_us);

			_last_read = pit_cnt;

			_set_counter(pit_cnt);
		}
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
		[&] (Alarms::None) { return Device::infinite_deadline; });
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
	                  Diag      const &diag,
	                  Alarms          &alarms,
	                  Device          &device)
	:
		Session_object(env.ep(), resources, label, diag),
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

		_device.update_deadline(next_deadline(_alarms));
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

		_device.update_deadline(next_deadline(_alarms));
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

		Session_component *_create_session(const char *args) override
		{
			return new (md_alloc())
				Session_component(_env,
				                  session_resources_from_args(args),
				                  session_label_from_args(args),
				                  session_diag_from_args(args),
				                  _alarms, _device);
		}

		void _upgrade_session(Session_component *s, const char *args) override
		{
			s->upgrade(ram_quota_from_args(args));
			s->upgrade(cap_quota_from_args(args));
		}

		void _destroy_session(Session_component *session) override
		{
			Genode::destroy(md_alloc(), session);
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

	Device _device { _env, *this };

	Alarms _alarms { };

	Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	Root _root { _env, _sliced_heap, _alarms, _device };

	/**
	 * Device::Wakeup_dispatcher
	 */
	void dispatch_device_wakeup() override
	{
		Clock const now = _device.now();

		/* handle and remove pending alarms */
		while (_alarms.with_any_in_range({ 0 }, now, [&] (Alarm &alarm) {
			alarm.session.handle_wakeup(); }));

		/* schedule next wakeup */
		_device.update_deadline(next_deadline(_alarms));
	}

	Main(Genode::Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env) { static Timer::Main inst(env); }
