/*
 * \brief  Server-side Audio_in session interface
 * \author Josef Soentgen
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__AUDIO_IN_SESSION__RPC_OBJECT_H_
#define _INCLUDE__AUDIO_IN_SESSION__RPC_OBJECT_H_

/* Genode includes */
#include <base/env.h>
#include <base/rpc_server.h>
#include <audio_in_session/audio_in_session.h>


namespace Audio_in { class Session_rpc_object; }


class Audio_in::Session_rpc_object : public Genode::Rpc_object<Audio_in::Session,
                                                               Session_rpc_object>
{
	protected:

		bool                              _stopped; /* state */
		Genode::Ram_dataspace_capability  _ds;      /* contains Audio_in stream */

		Genode::Signal_context_capability _data_cap;
		Genode::Signal_context_capability _progress_cap;
		Genode::Signal_context_capability _overrun_cap;

	public:

		Session_rpc_object(Genode::Signal_context_capability data_cap)
		:
			_stopped(true), _data_cap(data_cap)
		{
			using namespace Genode;

			_ds = env()->ram_session()->alloc(sizeof(Stream));
			_stream = static_cast<Stream *>(env()->rm_session()->attach(_ds));
		}

		virtual ~Session_rpc_object()
		{
			if (_ds.valid()) {
				Genode::env()->rm_session()->detach(_stream);
				Genode::env()->ram_session()->free(_ds);
			}
		}


		/**************
		 ** Signals  **
		 **************/

		void progress_sigh(Genode::Signal_context_capability sigh) {
			_progress_cap = sigh; }

		void overrun_sigh(Genode::Signal_context_capability sigh) {
			_overrun_cap = sigh; }

		Genode::Signal_context_capability data_avail_sigh() {
			return _data_cap; }


		/***********************
		 ** Session interface **
		 ***********************/

		void start() { _stopped = false; }
		void stop()  { _stopped = true; }

		Genode::Dataspace_capability dataspace() { return _ds; }


		/**********************************
		 ** Session interface extensions **
		 **********************************/

		/**
		 * Send 'progress' signal
		 */
		void progress_submit()
		{
			if (_progress_cap.valid())
				Genode::Signal_transmitter(_progress_cap).submit();
		}

		/**
		 * Send 'overrun' signal
		 */
		void overrun_submit()
		{
			if (_overrun_cap.valid())
				Genode::Signal_transmitter(_overrun_cap).submit();
		}

		/**
		 * Return true if client state is stopped
		 */
		bool stopped() const { return _stopped; }

		/**
		 * Return true if client session is active
		 */
		bool active()  const { return !_stopped; }
};

#endif /* _INCLUDE__AUDIO_IN_SESSION__RPC_OBJECT_H_ */
