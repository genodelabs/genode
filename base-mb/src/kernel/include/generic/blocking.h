/*
 * \brief  Generic userland execution blockings
 * \author Martin Stein
 * \date   2010-10-27
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__INCLUDE__GENERIC__BLOCKING_H_
#define _KERNEL__INCLUDE__GENERIC__BLOCKING_H_

#include <kernel/types.h>
#include <util/queue.h>
#include <generic/verbose.h>
#include <generic/tlb.h>
#include <generic/event.h>
#include <generic/syscall_events.h>
#include <generic/ipc.h>

namespace Kernel {

	enum { DATA_TLB_MISS__VERBOSE        = 0,
	       INSTRUCTION_TLB_MISS__VERBOSE = 0 };


	struct Blocking
	{
		virtual bool unblock() = 0;
		virtual ~Blocking() { }
	};


	class Kernel_exit;


	/**
	 * This event is triggered everytime kernels main
	 * routine is done and returns
	 */
	Kernel_exit* kernel_exit_event();


	/**
	 * Event occuring when kernel exits execution
	 */
	struct Kernel_exit : public Event
	{
		class Listener : public Event::Listener
		{
			Kernel_exit* _event;

			protected:

				Listener() : _event(kernel_exit_event()) { _event->add(this); }

				virtual ~Listener(){}

				virtual void _on_kernel_exit() = 0;
				void         _on_event() { _on_kernel_exit(); }
		};

		void add(Listener *l) { Event::_add(l); }

		On_occurence__result on_occurence()
		{
			_populate();
			return EVENT_PROCESSED;
		}
	};


	class Kernel_entry;

	/**
	 * This Event triggers everytime kernels main
	 * routine starts execution
	 */
	Kernel_entry* kernel_entry_event();

	/**
	 * Event occuring when kernel starts to be executed
	 */
	struct Kernel_entry : public Event
	{
		class Listener : public Event::Listener
		{
			Kernel_entry* _event;

			protected:

				Listener() : _event(kernel_entry_event()) { _event->add(this); }

				virtual ~Listener(){}

				virtual void _on_kernel_entry() = 0;
				void         _on_event() { _on_kernel_entry(); }
		};

		void add(Listener *l) { Event::_add(l); }

		On_occurence__result on_occurence()
		{
			_populate();
			return EVENT_PROCESSED;
		}
	};


	class Instruction_tlb_miss : public Event
	{
		friend class Exception;
		friend class Listener;

		void _add(Listener *l) { Event::_add(l); }

		public:

			typedef Tlb::Virtual_page  Virtual_page;
			typedef Tlb::Physical_page Physical_page;
			typedef Tlb::Resolution    Resolution;

			class Listener : public Event::Listener
			{
				typedef Instruction_tlb_miss::Resolution Resolution;

				Resolution* _resolution;

				protected:

					typedef Instruction_tlb_miss::Virtual_page  Virtual_page;
					typedef Instruction_tlb_miss::Physical_page Physical_page;

					virtual ~Listener(){}

					void _resolve_identically(Physical_page::size_t s,
					                          Physical_page::Permissions p);

					virtual void _on_instruction_tlb_miss(Virtual_page* accessed_page) = 0;

					void _on_event();

					void _event(Instruction_tlb_miss* itm)
					{
						itm->_add(this);
						_resolution = itm->missing_resolution();
					}

					/**
					 * Write read access for listeners
					 */
					Physical_page* physical_page()
					{
						return &_resolution->physical_page;
					}
			};

			Resolution *missing_resolution() { return &_missing_resolution; }

		protected:

			Resolution _missing_resolution;

			void _on_occurence__error__virtual_page_invalid()
			{
				printf("Error in Kernel::Instruction_tlb_miss::on_occurence, "
				       "virtual page invalid, halt\n");
				halt();
			}

			void _on_occerence__verbose__waiting_for_resolution()
			{
				if (!INSTRUCTION_TLB_MISS__VERBOSE) return;

				printf("Kernel::Instruction_tlb_miss::on_occurence, "
				       "leaving unresoluted virtual page, address=0x%p, pid=%i\n",
				       (void*)_missing_resolution.virtual_page.address(),
				       (int)_missing_resolution.virtual_page.protection_id() );
}

		public:

			On_occurence__result on_occurence();
	};


	class Data_tlb_miss : public Event
	{
		friend class Exception;
		friend class Listener;

		void _add(Listener* l) {Event::_add(l); }

		public:

			typedef Tlb::Virtual_page  Virtual_page;
			typedef Tlb::Physical_page Physical_page;
			typedef Tlb::Resolution    Resolution;

			class Listener : public Event::Listener
			{
				typedef Data_tlb_miss::Resolution Resolution;

				Resolution* _resolution;

				protected:

					typedef Data_tlb_miss::Virtual_page  Virtual_page;
					typedef Data_tlb_miss::Physical_page Physical_page;

					virtual ~Listener(){}

					void _resolve_identically(Physical_page::size_t s,
					                          Physical_page::Permissions p);

					virtual void _on_data_tlb_miss(Virtual_page* accessed_page,
					                               bool write_access) = 0;

					void _on_event();

					void _event(Data_tlb_miss* dtm)
					{
						dtm->_add(this);
						_resolution = dtm->missing_resolution();
					}

					/**
					 * Write read access for listeners
					 */
					Physical_page* physical_page()
					{
						return &_resolution->physical_page;
					}
			};

			Resolution* missing_resolution() { return &_missing_resolution; }

		protected:

			Resolution _missing_resolution;

			enum{ ON_OCCURENCE__ERROR = 1 };

			void _on_occurence__error__virtual_page_invalid()
			{
				printf("Error in Kernel::Data_tlb_miss::on_occurence, "
				       "virtual page invalid, halt\n");
				halt();
			}

			void _on_occurence__verbose__waiting_for_resolution()
			{
				if (!DATA_TLB_MISS__VERBOSE) return;

				printf("Kernel::Data_tlb_miss::on_occurence, "
				       "leaving unresoluted virtual page, address=0x%p, pid=%i)\n",
				       (void*)_missing_resolution.virtual_page.address(),
				       (int)_missing_resolution.virtual_page.protection_id() );
}

		public:

			On_occurence__result on_occurence();
	};


	class Exception : public Instruction_tlb_miss,
	                  public Data_tlb_miss,
	                  public Blocking
	{
		typedef Kernel::Tlb::Virtual_page Virtual_page;

		protected:

			Exception_id _id;

			virtual Protection_id protection_id()=0;
			virtual addr_t        address()=0;
			virtual bool          attempted_write_access()=0;

		public:

			enum { UNBLOCK__WARNING = 1 };

			enum { UNBLOCK__RETURN__SUCCESS = 0,
				UNBLOCK__RETURN__FAILED= - 1};

			bool unblock();

			Instruction_tlb_miss *instruction_tlb_miss() { return this; }

			Data_tlb_miss *data_tlb_miss() { return this; }
	};


	class Irq : public Blocking
	{
		public:

			enum{ UNBLOCK__WARNING=1 };

			enum { UNBLOCK__RETURN__SUCCESS =  0,
				UNBLOCK__RETURN__FAILED  = -1};

			bool unblock();

		protected:

			Irq_id _id;

			void _unblock__error__release_irq_failed()
			{
				printf("Error in Kernel::Irq::unblock, failed to release IRQ, halt\n");
				halt();
			}

			void _unblock__warning__unknown_id()
			{
				if (!UNBLOCK__WARNING) return;

				printf("Warning in Kernel::Irq::unblock, unexpected _id=%i\n", _id);
			}
	};


	class Syscall : public Blocking
	{
		public:

			class Source;

		private:

			word_t *_argument_0,
			     *_argument_1,
			     *_argument_2,
			     *_argument_3,
			     *_argument_4,
			     *_argument_5,
			     *_argument_6,
			     *_result_0;

			Source* _source;

		protected:

			Syscall_id _id;

			enum{ UNBLOCK__WARNING = 1 };

			void _unblock__warning__unknown_id()
			{
				if (!UNBLOCK__WARNING) return;

				printf("Warning in Kernel::Syscall::unblock, unexpected _id=%i\n", _id);
			}

		public:

			class  Source : public Print_char,
			                public Thread_create,
			                public Thread_sleep,
			                public Thread_kill,
			                public Thread_wake,
			                public Thread_pager,
			                public Ipc::Participates_dialog,
			                public Tlb_load,
			                public Tlb_flush,
			                public Thread_yield
//			                public Print_info
			{
				protected:

					Source(Utcb *utcb, Thread_id i) : 
						Ipc::Participates_dialog(utcb),
						tid(i)
					{ }

				public:

					Thread_id tid;

					virtual bool irq_allocate(Irq_id i, int * const result)=0;
					virtual bool irq_free(Irq_id i, int * const result)=0;
					virtual bool irq_wait()=0;
			};

			bool unblock();

			Syscall(word_t* argument_0,
			        word_t* argument_1,
			        word_t* argument_2,
			        word_t* argument_3,
			        word_t* argument_4,
			        word_t* argument_5,
			        word_t* argument_6,
			        word_t* result_0,
			        Source* s)
			:
				_argument_0(argument_0),
				_argument_1(argument_1),
				_argument_2(argument_2),
				_argument_3(argument_3),
				_argument_4(argument_4),
				_argument_5(argument_5),
				_argument_6(argument_6),
				_result_0(result_0),
				_source(s)
			{ }
	};
}

#endif /* _KERNEL__INCLUDE__GENERIC__BLOCKING_H_ */
