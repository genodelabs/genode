/*
 * \brief  Mechanism for accessing the virtual memory of inferiors
 * \author Norman Feske
 * \date   2023-05-24
 *
 * The 'Memory_accessor' uses a dedicated "probe" thread of accessing the
 * inferior's memory. It keeps a window of the address space locally attached
 * and guards the access of this window by the probe thread by watching out for
 * page faults. The sacrifice of the probe thread shields the monitor from the
 * effects of accessing empty regions of the inferior's address space.
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MEMORY_ACCESSOR_H_
#define _MEMORY_ACCESSOR_H_

/* Genode includes */
#include <rm_session/connection.h>
#include <timer_session/connection.h>

/* local includes */
#include <inferior_pd.h>
#include <types.h>

namespace Monitor { struct Memory_accessor; }


class Monitor::Memory_accessor : Noncopyable
{
	public:

		struct Virt_addr { addr_t value; };

	private:

		Env &_env;

		static constexpr size_t WINDOW_SIZE_LOG2 = 24, /* 16 MiB */
		                        WINDOW_SIZE      = 1 << WINDOW_SIZE_LOG2;

		struct Curr_view
		{
			Region_map  &_local_rm;
			Inferior_pd &_pd;
			addr_t const _offset;

			struct { uint8_t const *_local_ptr; };

			Curr_view(Region_map &local_rm, Inferior_pd &pd, addr_t offset)
			:
				_local_rm(local_rm), _pd(pd), _offset(offset),
				_local_ptr(_local_rm.attach(pd._address_space.dataspace(),
				                            WINDOW_SIZE, offset))
			{ }

			~Curr_view() { _local_rm.detach(_local_ptr); }

			bool _in_curr_range(Virt_addr at) const
			{
				return (at.value >= _offset) && (at.value < _offset + WINDOW_SIZE);
			}

			bool contains(Inferior_pd &pd, Virt_addr at) const
			{
				return (pd.id() == _pd.id()) && _in_curr_range(at);
			}
		};

		Constructible<Curr_view> _curr_view { };

		using Probe_response_handler = Io_signal_handler<Memory_accessor>;

		Io_signal_handler<Memory_accessor> _probe_response_handler {
			_env.ep(), *this, &Memory_accessor::_handle_probe_response };

		void _handle_probe_response() { }

		struct Probe : Thread
		{
			Probe_response_handler &_probe_response_handler;

			Blockade _blockade { };

			/* exists only temporary during a 'Memory_accessor::read' call */
			struct Read_job
			{
				Curr_view            &curr_view;
				Virt_addr             at;
				Byte_range_ptr const &dst;

				size_t pos  = 0; /* number of completed bytes */
				bool   done = 0; /* true if 'execute_may_fault' survived */

				Read_job(Curr_view &curr_view, Virt_addr at, Byte_range_ptr const &dst)
				:
					curr_view(curr_view), at(at), dst(dst)
				{ }

				/* called only once */
				void execute_may_fault()
				{
					/* offset from the start of the window */
					addr_t const window_pos = at.value - curr_view._offset;
					addr_t const num_bytes_at_window_pos = WINDOW_SIZE - window_pos;
					size_t const len = min(dst.num_bytes, num_bytes_at_window_pos);

					uint8_t const * const src_ptr = curr_view._local_ptr;

					for (pos = 0; pos < len; pos++)
						dst.start[pos] = src_ptr[window_pos + pos];

					done = true;
				}
			};

			Constructible<Read_job> _read_job { };

			/**
			 * Thread interface
			 */
			void entry() override
			{
				for (;;) {
					_blockade.block();

					if (_read_job.constructed()) {
						_read_job->execute_may_fault();

						/* wakeup 'Memory_accessor::read' via I/O signal */
						_probe_response_handler.local_submit();
					}
				}
			}

			Probe(Env &env, Probe_response_handler &probe_response_handler)
			:
				Thread(env, "probe", 16*1024),
				_probe_response_handler(probe_response_handler)
			{
				start();
			}

			size_t read(Curr_view &curr_view,
			            Virt_addr at, Byte_range_ptr const &dst, auto const &block_fn)
			{
				_read_job.construct(curr_view, at, dst);
				_blockade.wakeup();

				/* block until read is done or a page fault occurred */
				while (!_read_job->done && block_fn());

				size_t const result = _read_job->pos;

				_read_job.destruct();

				return result;
			}
		};

		Constructible<Probe> _probe { };

		Io_signal_handler<Memory_accessor> _timeout_handler {
			_env.ep(), *this, &Memory_accessor::_handle_timeout };

		Timer::Connection _watchdog_timer { _env };

		unsigned _timeout_count = 0;

		void _handle_timeout() { _timeout_count++; }

	public:

		Memory_accessor(Env &env) : _env(env)
		{
			_watchdog_timer.sigh(_timeout_handler);
		}

		void flush() { _curr_view.destruct(); }

		/**
		 * Read memory from inferior 'pd' at address 'at' into buffer 'dst'
		 *
		 * The 'dst.num_bytes' value denotes the number of bytes to read.
		 *
		 * \return  number of successfully read bytes, which can be smaller
		 *          than 'dst.num_bytes' when encountering the bounds of
		 *          mapped memory in the inferior's address space.
		 */
		size_t read(Inferior_pd &pd, Virt_addr at, Byte_range_ptr const &dst)
		{
			if (_curr_view.constructed() && !_curr_view->contains(pd, at))
				_curr_view.destruct();

			if (!_curr_view.constructed()) {
				addr_t const offset = at.value & ~(WINDOW_SIZE - 1);
				try { _curr_view.construct(_env.rm(), pd, offset); }
				catch (Region_map::Region_conflict) {
					warning("attempt to read outside the virtual address space: ",
					        Hex(at.value));
					return 0;
				}
			}

			/* give up after 100 milliseconds */
			_watchdog_timer.trigger_once(1000*100);

			/* drain pending signals to avoid spurious watchdog timeouts */
			while (_env.ep().dispatch_pending_io_signal());

			unsigned const orig_page_fault_count = pd.page_fault_count();
			unsigned const orig_timeout_count    = _timeout_count;

			if (!_probe.constructed())
				_probe.construct(_env, _probe_response_handler);

			auto fault_or_timeout_occurred = [&] {
				return (orig_page_fault_count != pd.page_fault_count())
				    || (orig_timeout_count != _timeout_count); };

			auto block_fn = [&]
			{
				if (fault_or_timeout_occurred())
					return false; /* cancel read */

				_env.ep().wait_and_dispatch_one_io_signal();
				return true; /* keep trying */
			};

			size_t const read_num_bytes =
				_probe->read(*_curr_view, at, dst, block_fn);

			/* wind down the faulted probe thread, spawn a fresh one on next call */
			if (fault_or_timeout_occurred())
				_probe.destruct();

			return read_num_bytes;
		}
};

#endif /* _MEMORY_ACCESSOR_H_ */
