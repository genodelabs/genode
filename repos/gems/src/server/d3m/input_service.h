/*
 * \brief  D3m input service
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2012-01-25
 *
 * D3m supports merging the input events of multiple devices into one
 * stream of events. Each driver corresponds to an event 'Source'. When the
 * driver announces the "Input" session interface, the corresponding
 * 'Source' is added to the 'Source_registry'. The d3m input serves queries
 * all sources registered at the source registry for input and merges the
 * streams of events.
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INPUT_SERVICE_H_
#define _INPUT_SERVICE_H_

/* Genode includes */
#include <os/attached_ram_dataspace.h>
#include <root/component.h>
#include <util/list.h>
#include <input_session/client.h>
#include <input/event.h>

namespace Input {

	class Source : public Genode::List<Source>::Element
	{
		public:

			class Source_unavailable { };

		private:

			Genode::Root_capability   _root;
			Input::Session_capability _session;
			Input::Session_client     _client;
			Event                    *_ev_buf;
			Genode::size_t            _ev_buf_max; /* in events */

			/**
			 * Open input session via the specified root capability
			 */
			static Session_capability
			_request_session(Genode::Root_capability root)
			{
				const char *args = "ram_quota=8K";

				try {
					using namespace Genode;
					return static_cap_cast<Session>
					       (Root_client(root).session(args, Genode::Affinity()));
				} catch (...) {
					throw Source_unavailable();
				}
			}

		public:

			/**
			 * Constructor
			 *
			 * At construction time, the '_client' gets initialized using the
			 * default-initialized (invalid) '_session' capability. The
			 * 'Source::session' function is not usaable before
			 * re-initializing the object via the 'connect' function.
			 */
			Source()
			: _client(_session), _ev_buf(0), _ev_buf_max(0) { }

			/**
			 * Called when the driver announces the "Input" service
			 */
			void connect(Genode::Root_capability root)
			{
				_root    = root;
				_session = _request_session(_root);
				_client  = Session_client(_session);

				/* locally map input-event buffer */
				Genode::Dataspace_capability ds_cap = _client.dataspace();
				_ev_buf = Genode::env()->rm_session()->attach(ds_cap);
				_ev_buf_max = Genode::Dataspace_client(ds_cap).size()
				            / sizeof(Event);
			}

			/**
			 * Return true is 'session()' is ready to use
			 */
			bool connected() const { return _session.valid(); }

			/**
			 * Return true if input is pending
			 */
			bool input_pending() const { return connected() && _client.is_pending(); }

			/**
			 * Return event buffer
			 */
			Event const *ev_buf() const { return _ev_buf; }

			/**
			 * Flush input events
			 */
			Genode::size_t flush() { return _client.flush(); }

			/**
			 * Register signal handler for input notifications
			 */
			void sigh(Genode::Signal_context_capability sigh)
			{
				_client.sigh(sigh);
			}
	};


	struct Source_registry
	{
		private:

			Genode::Lock                        _lock;
			Genode::List<Source> _sources;

		public:

			/**
			 * Register a new source of input events
			 *
			 * This function is called once for each driver, when the driver
			 * announced its "Input" service. By adding the new source, the
			 * driver's input events become visible to the d3m input session.
			 */
			void add_source(Source *entry)
			{
				Genode::Lock::Guard guard(_lock);
				_sources.insert(entry);
			}

			bool any_source_has_pending_input()
			{
				Genode::Lock::Guard guard(_lock);

				for (Source *e = _sources.first(); e; e = e->next())
					if (e->connected() && e->input_pending())
						return true;

				return false;
			}

			/**
			 * Flush all input events from all available sources
			 *
			 * This function merges the input-event streams of all sources into
			 * one.
			 *
			 * \param dst      destination event buffer
			 * \param dst_max  capacility of event buffer, in number of events
			 *
			 * \return  total number of available input events
			 */
			Genode::size_t flush_sources(Event *dst, Genode::size_t dst_max)
			{
				Genode::size_t dst_count = 0;

				for (Source *e = _sources.first(); e; e = e->next()) {

					if (!e->connected() || !e->input_pending())
						continue;

					/*
					 * Copy input events from driver to client buffer
					 */
					Event const *src = e->ev_buf();
					Genode::size_t const src_max = e->flush();
					for (Genode::size_t i = 0; i < src_max; i++, dst_count++) {

						if (dst_count == dst_max) {
							PERR("client input-buffer overflow");
							return dst_count;
						}

						dst[dst_count] = src[i];
					}
				}
				return dst_count;
			}

			void sigh(Genode::Signal_context_capability sigh)
			{
				for (Source *e = _sources.first(); e; e = e->next())
					e->sigh(sigh);
			}
	};


	/*****************************
	 ** Input service front end **
	 *****************************/

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			Source_registry &_source_registry;

			/*
			 * Input event buffer shared with the client
			 */
			enum { MAX_EVENTS = 1000 };
			Genode::Attached_ram_dataspace _ev_ds;

		public:

			Session_component(Source_registry &source_registry)
			:
				_source_registry(source_registry),
				_ev_ds(Genode::env()->ram_session(), MAX_EVENTS*sizeof(Event))
			{ }

			/*****************************
			 ** Input-session interface **
			 *****************************/

			Genode::Dataspace_capability dataspace() override { return _ev_ds.cap(); }

			bool is_pending() const override
			{
				return _source_registry.any_source_has_pending_input();
			}

			int flush() override
			{
				return _source_registry.flush_sources(_ev_ds.local_addr<Event>(),
				                                       MAX_EVENTS);
			}

			void sigh(Genode::Signal_context_capability sigh) override
			{
				_source_registry.sigh(sigh);
			}
	};

	/**
	 * Shortcut for single-client root component
	 */
	typedef Genode::Root_component<Session_component, Genode::Single_client> Root_component;

	class Root : public Root_component
	{
		private:

			Source_registry &_source_registry;

		protected:

			Session_component *_create_session(const char *args)
			{
				try {
					return new (md_alloc()) Session_component(_source_registry);
				} catch (Genode::Allocator::Out_of_memory()) {
					PERR("Out of memory");
					throw Genode::Root::Quota_exceeded();
				} catch (...) {
					PERR("Exception in construction of NIC slave");
					throw;
				}
			}

		public:

			Root(Genode::Rpc_entrypoint *session_ep,
			     Genode::Allocator      *md_alloc,
			     Source_registry        &source_registry)
			:
				Root_component(session_ep, md_alloc),
				_source_registry(source_registry)
			{ }
	};
}

#endif /* _INPUT_SERVICE_H_ */
