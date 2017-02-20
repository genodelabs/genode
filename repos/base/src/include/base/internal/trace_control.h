/*
 * \brief  Event tracing control interface between CPU client and CPU server
 * \author Norman Feske
 * \date   2013-08-09
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__TRACE_CONTROL_H_
#define _INCLUDE__BASE__INTERNAL__TRACE_CONTROL_H_

namespace Genode { namespace Trace { class Control; } }


/**
 * Structure shared between core and CPU session client
 *
 * The 'Trace_control' structure allows core to propagate control
 * information to the threads of a process in an asynchronous way,
 * for example to enable/disable tracing or setting the tracing
 * policy.
 */
class Genode::Trace::Control
{
	private:

		enum State { FREE, DISABLED, ENABLED, ERROR };

		unsigned volatile _policy_version;

		State volatile _designated_state;
		State volatile _acknowledged_state;

		bool volatile _inhibit;

	public:

		/*************************************************
		 ** Interface used by by the CPU session client **
		 *************************************************/

		/**
		 * Facility to temporarily inhibit tracing
		 *
		 * This utility is used by a thread during the initialization of its
		 * 'Trace::Logger' to prevent recursion. During initialiation, the
		 * logger performs a few RPCs (e.g., to obtain the trace-control
		 * index). Because an RPC is a trace point, this would result in the
		 * re-entering the logger initialization.
		 */
		struct Inhibit_guard
		{
			Control &_control;
			Inhibit_guard(Control &control) : _control(control)
			{
				_control._inhibit = true;
			}

			~Inhibit_guard() { _control._inhibit = false; }
		};

		/**
		 * Request current policy version
		 *
		 * To be compared to the version currently installed at the
		 * client.
		 */
		unsigned policy_version() const { return _policy_version; }

		/**
		 * Called after having updated the policy
		 */
		void acknowledge_policy_version(unsigned version) { _policy_version = version; }

		/**
		 * Detect state change
		 */
		bool state_changed() const { return _designated_state != _acknowledged_state; }

		/**
		 * Return true if CPU client thread should stop tracing
		 */
		bool to_be_disabled() const
		{
			return state_changed() && (_designated_state == DISABLED);
		}

		/**
		 * Return true if CPU client thread should start tracing
		 */
		bool to_be_enabled() const
		{
			return state_changed() && (_designated_state == ENABLED);
		}

		/**
		 * Confirm that the CPU client has enabled the tracing
		 */
		void acknowledge_enabled() { _acknowledged_state = ENABLED; };

		/**
		 * Confirm that the CPU client has disabled the tracing
		 *
		 * After acknowledging that we disabled the policy, core is
		 * safe to free the policy dataspace.
		 */
		void acknowledge_disabled() { _acknowledged_state = DISABLED; };

		/**
		 * State set when trace buffer or policy could not be successfully
		 * obtained.
		 */
		void error() { _acknowledged_state = ERROR; }

		/**
		 * Return true if the corresponding thread should suppress trace events
		 */
		bool tracing_inhibited() const { return _inhibit; }


		/*****************************************
		 ** Accessors called by the CPU service **
		 *****************************************/

		bool is_free() const { return _designated_state == FREE; }

		void alloc()
		{
			_policy_version     = 0;
			_designated_state   = DISABLED;
			_acknowledged_state = DISABLED;
		}

		void reset()
		{
			_policy_version     = 0;
			_designated_state   = FREE;
			_acknowledged_state = FREE;
		}

		void trace()
		{
			_policy_version++;
			enable();
		}

		void enable()
		{
			_designated_state = ENABLED;
		}

		void disable()
		{
			_designated_state = DISABLED;
		}

		bool has_error() const { return _acknowledged_state == ERROR; }

		/**
		 * Return true if tracing is enabled
		 */
		bool enabled() const { return _acknowledged_state == ENABLED; }

		/**
		 * Return true if tracing is enabled
		 *
		 * \noapi
		 * \deprecated  use 'enabled' instead
		 */
		bool is_enabled() const { return enabled(); }
};

#endif /* _INCLUDE__BASE__INTERNAL__TRACE_CONTROL_H_ */
