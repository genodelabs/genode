/*
 * \brief  Timer driver for Fiasco.OC
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2024-06-16
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
#include <base/attached_rom_dataspace.h>
#include <root/component.h>
#include <timer_session/timer_session.h>

/* base-internal includes */
#include <base/internal/alarm_registry.h>

/* Fiasco.OC includes */
#include <foc/native_thread.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"

namespace Foc {
	#include <l4/sys/ipc.h>
	#include <l4/sys/kip.h>
}

#pragma GCC diagnostic pop


using namespace Foc;


namespace Timer {

	using namespace Genode;

	struct Tsc { uint64_t tsc; };
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

	void print(Output &out) const { Genode::print(out, us); }
};


class Timer::Device
{
	private:

		Attached_rom_dataspace _kip_ds;

	public:

		struct Wakeup_dispatcher : Interface
		{
			virtual void dispatch_device_wakeup() = 0;
		};

		struct Deadline : Clock { };

		static constexpr Deadline infinite_deadline { uint64_t(-1) };

	private:

		struct Waiter : Thread
		{
			l4_timeout_s mus_to_timeout(uint64_t const mus) const
			{
				if (mus == 0)
					return L4_IPC_TIMEOUT_0;
				else if (mus == ~0ULL)
					return L4_IPC_TIMEOUT_NEVER;

				long e = Genode::log2((unsigned long)mus) - 7;

				if (e < 0) e = 0;

				uint64_t m = mus / (1UL << e);

				enum { M_MASK = 0x3ff };

				/* check corner case */
				if ((e > 31 ) || (m > M_MASK)) {
					Genode::warning("invalid timeout ", mus, ", using max. values");
					e = 0;
					m = M_MASK;
				}

				return l4_timeout_rel(m & M_MASK, (unsigned)e);
			}

			Wakeup_dispatcher &_dispatcher;

			Mutex    _mutex    { }; /* protect '_deadline' */
			Deadline _deadline { ~0ULL };

			l4_cap_idx_t _myself { };

			Device  &_device;

			Waiter(Env &env, Wakeup_dispatcher &dispatcher, Device &device)
			:
				Thread(env, "waiter", 8*1024*sizeof(addr_t)),
				_dispatcher(dispatcher),
				_device(device)
			{
				start();
			}

			void entry() override
			{
				_myself = native_thread().kcap;

				for (;;) {

					auto deadline_atomic = [&]
					{
						Mutex::Guard guard(_mutex);
						return _deadline;
					};

					{
						auto const deadline = deadline_atomic();
						auto const now      = _device.now();

						if (now.us < deadline.us) {
							/* due to ex_regs not working, use 1ms granularity */
							auto usecs = min(deadline.us - now.us, 1000ull);

							auto const timeout = l4_timeout(L4_IPC_TIMEOUT_NEVER,
							                                mus_to_timeout(usecs));
							l4_ipc_sleep(timeout);
						}
					}

					if (_device.now().us >= deadline_atomic().us)
						_dispatcher.dispatch_device_wakeup();
				}
			}

			void update_deadline(Deadline const deadline)
			{
				Mutex::Guard guard(_mutex);

				bool const sooner_than_scheduled = (deadline.us < _deadline.us);

				_deadline = deadline;

				if (sooner_than_scheduled) {
					/* cancel old timeout by waking sleeping waiter */

					/* XXX not working, syscall does not return
					Foc::l4_thread_ex_regs(_myself, ~0UL, ~0UL,
					                       L4_THREAD_EX_REGS_CANCEL);
					 */
				}
			}
		} _waiter;

	public:

		Device(Env &env, Wakeup_dispatcher &dispatcher)
		: _kip_ds(env, "l4v2_kip"), _waiter(env, dispatcher, *this) { }

		Clock now()
		{
			auto kip = _kip_ds.local_addr<Foc::l4_kernel_info_t>();
			return { .us = Foc::l4_kip_clock(kip) };
		}

		void update_deadline(Deadline deadline) {
			_waiter.update_deadline(deadline); }
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
	Mutex  &_alarms_mutex;
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
	                  Mutex           &alarms_mutex,
	                  Device          &device)
	:
		Session_object(env.ep(), resources, label, diag),
		_alarms(alarms), _alarms_mutex(alarms_mutex), _device(device)
	{ }

	~Session_component()
	{
		Mutex::Guard guard(_alarms_mutex);

		_alarm.destruct();
	}

	/**
	 * Called by Device::Wakeup_dispatcher with '_alarms_mutex' taken
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
		Mutex::Guard guard(_alarms_mutex);

		_period.destruct();
		_alarm.destruct();

		Clock const now = _device.now();

		rel_us = max(rel_us, 250u);
		_alarm.construct(_alarms, *this, Clock { now.us + rel_us });

		_device.update_deadline(next_deadline(_alarms));
	}

	void trigger_periodic(uint64_t period_us) override
	{
		Mutex::Guard guard(_alarms_mutex);

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
		Mutex  &_alarms_mutex;
		Device &_device;

	protected:

		Session_component *_create_session(const char *args) override
		{
			return new (md_alloc())
				Session_component(_env,
				                  session_resources_from_args(args),
				                  session_label_from_args(args),
				                  session_diag_from_args(args),
				                  _alarms, _alarms_mutex, _device);
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

		Root(Env &env, Allocator &md_alloc,
		     Alarms &alarms, Mutex &alarms_mutex, Device &device)
		:
			Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _alarms(alarms), _alarms_mutex(alarms_mutex), _device(device)
		{ }
};


void Timer::Alarm::print(Output &out) const { Genode::print(out, session.label()); }


struct Timer::Main : Device::Wakeup_dispatcher
{
	Env &_env;

	Device _device { _env, *this };

	Mutex  _alarms_mutex { };
	Alarms _alarms { };

	Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	Root _root { _env, _sliced_heap, _alarms, _alarms_mutex, _device };

	/**
	 * Device::Wakeup_dispatcher
	 */
	void dispatch_device_wakeup() override
	{
		Mutex::Guard guard(_alarms_mutex);

		/* handle and remove pending alarms */
		while (_alarms.with_any_in_range({ 0 }, _device.now(), [&] (Alarm &alarm) {
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
