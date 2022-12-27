/*
 * \brief  Utilities and interfaces for implementing pin drivers
 * \author Norman Feske
 * \date   2021-04-20
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__PIN_DRIVER_H_
#define _INCLUDE__OS__PIN_DRIVER_H_

#include <util/reconstructible.h>
#include <base/session_label.h>
#include <base/registry.h>
#include <root/component.h>
#include <irq_session/irq_session.h>

namespace Pin {

	using namespace Genode;

	enum class Direction { IN, OUT };

	template <typename> struct Driver;
	template <typename> struct Assignment;

	template <typename, Direction> class Root;

	template <typename> class  Irq_session_component;
	template <typename> class  Irq_root;

	template <typename ID>
	struct Irq_root : Pin::Root<Irq_session_component<ID>, Direction::IN>
	{
		using Pin::Root<Irq_session_component<ID>, Direction::IN>::Root;
	};

	enum class Level { LOW, HIGH, HIGH_IMPEDANCE };
}


template <typename ID>
struct Pin::Driver : Interface
{
	/**
	 * Request pin state
	 */
	virtual bool pin_state(ID) const = 0;

	/**
	 * Set pin state
	 */
	virtual void pin_state(ID, Level) = 0;

	/**
	 * Return pin ID assigned to the specified session label
	 *
	 * \throw Service_denied
	 */
	virtual ID assigned_pin(Session_label, Direction) const = 0;

	/**
	 * Inform driver that the specified pin is in use
	 *
	 * The driver may use this information to maintain a reference counter
	 * per pin. The specified direction allows the driver to select one of
	 * multiple pin declarations used for time-multiplexing a pin between input
	 * and output.
	 */
	virtual void acquire_pin(ID, Direction) { };

	/**
	 * Inform driver that the specified pin is not longer in use
	 *
	 * This is the counter part of 'acquire_pin'.
	 *
	 * When releasing an output pin, the driver may respond by resetting the
	 * pin to its default state.
	 */
	virtual void release_pin(ID, Direction) = 0;

	/**
	 * Enable/disable interrupt for specified pin
	 */
	virtual void irq_enabled(ID, bool) = 0;

	/**
	 * Return true if IRQ is pending for specified pin
	 */
	virtual bool irq_pending(ID) const = 0;

	/**
	 * Acknowledge interrupt of specified pin
	 */
	virtual void ack_irq(ID) = 0;

	struct Irq_subscriber : Interface
	{
		ID id;
		Signal_context_capability sigh;
		bool outstanding_ack = false;

		void submit_irq()
		{
			Signal_transmitter(sigh).submit();
			outstanding_ack = true;
		}

		Irq_subscriber(ID id, Signal_context_capability sigh)
		: id(id), sigh(sigh) { }
	};

	Registry<Registered<Irq_subscriber>> irq_subscribers { };

	void deliver_pin_irqs()
	{
		irq_subscribers.for_each([&] (Irq_subscriber &subscriber) {

			if (subscriber.outstanding_ack || !irq_pending(subscriber.id))
				return;

			/* mask IRQ until acked */
			irq_enabled(subscriber.id, false);

			subscriber.submit_irq();
		});
	}
};


template <typename ID>
struct Pin::Assignment : Noncopyable
{
	Driver<ID> &driver;

	struct Target
	{
		ID id;

		Pin::Direction direction;

		Target(ID id, Pin::Direction dir) : id(id), direction(dir) { }

		bool operator != (Target const &other) const
		{
			return (id != other.id) || (direction != other.direction);
		}
	};

	Constructible<Target> target { };

	void _release()
	{
		if (target.constructed()) {
			driver.release_pin(target->id, target->direction);
			target.destruct();
		}
	}

	Assignment(Driver<ID> &driver) : driver(driver) { }

	~Assignment() { _release(); }

	enum class Update { UNCHANGED, CHANGED };

	/**
	 * Trigger an update of the pin assignment
	 *
	 * This method called in response to dynamic configuration changes.
	 *
	 * \return CHANGED if assignment to physical pin was modified
	 */
	Update update(Session_label const &label, Pin::Direction direction)
	{
		bool changed = false;
		try {
			/*
			 * \throw Service_denied
			 */
			Target const new_target { driver.assigned_pin(label, direction), direction };

			/* assignment changed from one pin to another */
			if (target.constructed() && (*target != new_target)) {
				_release();
				changed = true;
			}

			if (!target.constructed()) {
				driver.acquire_pin(new_target.id, new_target.direction);
				target.construct(new_target.id, new_target.direction);
				changed = true;
			}
		}
		catch (Service_denied) { _release(); }

		return changed ? Update::CHANGED : Update::UNCHANGED;
	}
};


/**
 * Common root component for 'Pin_state' and 'Pin_control' services
 *
 * \param SC  session-component type
 */
template <typename SC, Pin::Direction DIRECTION>
class Pin::Root : Genode::Root_component<SC>
{
	private:

		using Pin_id  = typename SC::Pin_id;
		using Driver  = Pin::Driver<Pin_id>;
		using Session = Registered<SC>;

		Entrypoint &_ep;

		Driver &_driver;

		Registry<Session> _sessions { };

		SC *_create_session(char const *args) override
		{
			return new (Root_component<SC>::md_alloc())
				Session(_sessions, _ep,
				        session_resources_from_args(args),
				        session_label_from_args(args),
				        session_diag_from_args(args),
				        _driver);
		}

	public:

		Root(Env &env, Allocator &alloc, Driver &driver)
		:
			Genode::Root_component<SC>(env.ep(), alloc),
			_ep(env.ep()), _driver(driver)
		{
			env.parent().announce(_ep.manage(*this));
		}

		~Root() { _ep.dissolve(*this); }

		void update_assignments()
		{
			_sessions.for_each([&] (Session &session) {
				session.update_assignment(); });
		}
};


template <typename ID>
class Pin::Irq_session_component : public Session_object<Irq_session>
{
	public:

		using Pin_id = ID;  /* expected by 'Pin::Root' */

	private:

		using Irq_subscriber = typename Driver<ID>::Irq_subscriber;
		using Assignment     = Pin::Assignment<Pin_id>;

		Assignment _assignment;

		Signal_context_capability _sigh { };

		Constructible<Registered<Irq_subscriber>> _subscriber { };

		void _ack_dangling_irq()
		{
			if (_subscriber.constructed() && _subscriber->outstanding_ack)
				ack_irq();
		}

	public:

		Irq_session_component(Entrypoint &ep, Resources const &resources,
		                      Label const &label, Diag &diag, Driver<ID> &driver)
		:
			Session_object<Irq_session>(ep, resources, label, diag),
			_assignment(driver)
		{
			update_assignment();
		}

		~Irq_session_component() { _ack_dangling_irq(); }

		void update_assignment()
		{
			if (_assignment.target.constructed())
				_assignment.driver.irq_enabled(_assignment.target->id, false);

			typename Assignment::Update const update_result =
				_assignment.update(label(), Pin::Direction::IN);

			if (update_result == Assignment::Update::CHANGED) {

				_ack_dangling_irq();

				if (_assignment.target.constructed())
					_subscriber.construct(_assignment.driver.irq_subscribers,
					                      _assignment.target->id, _sigh);
				else
					_subscriber.destruct();
			}

			bool const charged = _assignment.target.constructed()
			                  && _subscriber.constructed()
			                  && !_subscriber->outstanding_ack;
			if (charged)
				_assignment.driver.irq_enabled(_assignment.target->id, true);
		}

		/**
		 * Irq_session interface
		 */
		void ack_irq() override
		{
			if (_assignment.target.constructed()) {
				_assignment.driver.ack_irq(_assignment.target->id);
				_assignment.driver.irq_enabled(_assignment.target->id, true);
			}

			if (_subscriber.constructed())
				_subscriber->outstanding_ack = false;
		}

		/**
		 * Irq_session interface
		 */
		void sigh(Signal_context_capability sigh) override
		{
			_sigh = sigh;

			if (_subscriber.constructed())
				_subscriber->sigh = sigh;

			update_assignment();

			bool initial_irq = _subscriber.constructed()
			                && _assignment.target.constructed()
			                && _assignment.driver.irq_pending(_assignment.target->id);

			if (initial_irq)
				_subscriber->submit_irq();
		}

		/**
		 * Irq_session interface
		 */
		Info info() override { return Info { }; }
};

#endif /* _INCLUDE__OS__PIN_DRIVER_H_ */
