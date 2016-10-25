/*
 * \brief  Client-side Audio_in-session
 * \author Josef Soentgen
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__AUDIO_IN_SESSION__CLIENT_H_
#define _INCLUDE__AUDIO_IN_SESSION__CLIENT_H_

/* Genode includes */
#include <base/env.h>
#include <base/rpc_client.h>
#include <audio_in_session/audio_in_session.h>


namespace Audio_in {
	struct Signal;
	struct Session_client;
}


struct Audio_in::Signal
{
	Genode::Signal_receiver           recv;
	Genode::Signal_context            context;
	Genode::Signal_context_capability cap;

	Signal() : cap(recv.manage(&context)) { }
	~Signal() { recv.dissolve(&context); }

	void wait() { recv.wait_for_signal(); }
};


class Audio_in::Session_client : public Genode::Rpc_client<Session>
{
	private:

		Signal _progress;

		Genode::Signal_transmitter _data_avail;

	public:

		/**
		 * Constructor
		 *
		 * \param session          session capability
		 * \param progress_signal  true, install 'progress_signal' receiver
		 */
		Session_client(Genode::Capability<Session> session,
		               bool progress_signal)
		:
		  Genode::Rpc_client<Session>(session),
		  _data_avail(call<Rpc_data_avail_sigh>())
		{
			/* ask server for stream data space and attach it */
			_stream = static_cast<Stream *>(Genode::env()->rm_session()->attach(call<Rpc_dataspace>()));

			if (progress_signal)
				progress_sigh(_progress.cap);
		}


		/*************
		 ** Signals **
		 *************/

		void progress_sigh(Genode::Signal_context_capability sigh) {
			call<Rpc_progress_sigh>(sigh); }

		void overrun_sigh(Genode::Signal_context_capability sigh) {
			call<Rpc_overrun_sigh>(sigh); }

		Genode::Signal_context_capability data_avail_sigh() {
			return Genode::Signal_context_capability(); }


		/***********************
		 ** Session interface **
		 ***********************/

		void start()
		{
			call<Rpc_start>();

			/* reset tail pointer */
			stream()->reset();
		}

		void stop() { call<Rpc_stop>();  }


		/**********************************
		 ** Session interface extensions **
		 **********************************/

		/**
		 * Wait for progress signal
		 */
		void wait_for_progress()
		{
			if (!_progress.cap.valid()) {
				Genode::warning("Progress signal is not installed, will not block "
				                "(enable in 'Audio_in::Connection')");
				return;
			}

			_progress.wait();
		}
};

#endif /* _INCLUDE__AUDIO_IN_SESSION__CLIENT_H_ */
