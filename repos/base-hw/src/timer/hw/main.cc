/*
 * \brief  Timer driver for the base-hw kernel
 * \author Norman Feske
 * \date   2024-03-11
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
#include <root/component.h>
#include <timer_session/timer_session.h>
#include <trace/timestamp.h>

/* base-internal includes */
#include <base/internal/alarm_registry.h>

/* base-hw includes */
#include <kernel/interface.h>

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
	public:

		struct Wakeup_dispatcher : Interface
		{
			virtual void dispatch_device_wakeup() = 0;
		};

		struct Deadline : Clock { };

		static constexpr Deadline infinite_deadline { uint64_t(-1) };

	private:

		Env &_env;

		Kernel::time_t const _max_timeout_us = Kernel::timeout_max_us();

		Wakeup_dispatcher &_dispatcher;

		Signal_handler<Device> _handler { _env.ep(), *this, &Device::_handle_timeout };

		Signal_context_capability const _handler_cap = _handler;

		Kernel::capid_t const _sel = Kernel::capid_t(addr_t(_handler_cap.data()) & 0xffffu);

		void _handle_timeout() { _dispatcher.dispatch_device_wakeup(); }

	public:

		Device(Env &env, Wakeup_dispatcher &dispatcher)
		: _env(env), _dispatcher(dispatcher) { }

		Clock now() const
		{
			return Clock { .us = Kernel::time() };
		}

		void update_deadline(Deadline const deadline)
		{
			uint64_t const now_us = now().us;
			uint64_t const rel_us = (deadline.us > now_us)
			                      ? min(_max_timeout_us, deadline.us - now_us)
			                      : 0;

			Kernel::timeout(Kernel::timeout_t(rel_us), _sel);
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
