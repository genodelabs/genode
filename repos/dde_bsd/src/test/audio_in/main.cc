/*
 * \brief  Audio-in test implementation
 * \author Josef Soentgen
 * \date   2015-05-11
 *
 * The test program records on channel.
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <audio_out_session/connection.h>
#include <audio_in_session/connection.h>
#include <base/log.h>
#include <base/sleep.h>
#include <dataspace/client.h>
#include <os/config.h>
#include <rom_session/connection.h>


enum {
	LEFT        = 0,
	RIGHT       = 1,
	CHANNELS    = 2,
	FRAME_SIZE  = sizeof(float),
	PERIOD_SIZE = FRAME_SIZE * Audio_in::PERIOD, /* size of period frame */
};


class Recording
{
	private:

		Genode::Signal_receiver              &_sig_rec;
		Genode::Signal_dispatcher<Recording>  _record_progress;
		Genode::Signal_dispatcher<Recording>  _record_overrun;

		Audio_out::Connection *_audio_out[CHANNELS];
		Audio_in::Connection   _audio_in { "left" };
		Audio_in::Stream      &_stream;

		void _play(Audio_in::Packet *ip)
		{
			float *in = ip->content();

			Audio_out::Packet *op[CHANNELS];

			/* get left channel packet pos and sync right channel */
			try {
				op[LEFT]     = _audio_out[LEFT]->stream()->alloc();
				unsigned pos = _audio_out[LEFT]->stream()->packet_position(op[LEFT]);
				op[RIGHT]    = _audio_out[RIGHT]->stream()->get(pos);
			} catch (...) { return; }

			for (int c = 0; c < CHANNELS; c++) {
				float *out = op[c]->content();
				for (int i = 0; i < Audio_in::PERIOD; i++) {
					out[i] = in[i];
				}
			}

			for (int c = 0; c < CHANNELS; c++)
				_audio_out[c]->submit(op[c]);
		}

		void _handle_record_progress(unsigned)
		{
			Audio_in::Packet *p      = _stream.get(_stream.pos());

			if (!p->valid())
				return;

			_play(p);

			p->invalidate();
			p->mark_as_recorded();

			_stream.increment_position();
		}

		void _handle_record_overrun(unsigned)
		{
			unsigned pos  = _stream.pos();
			unsigned tail = _stream.tail();

			Genode::warning("record overrun, pos: ", pos, " tail: ", tail, " ",
			                "overriden: ", tail - pos);

			/*
			 * Normally you would handle this case properly by saving all
			 * packet that have not been already overriden. For simplicity
			 * we just discard all packets by setting pos to current tail.
			 */
			_stream.pos(tail);
		}

	public:

		Recording(Genode::Allocator &md_alloc, Genode::Signal_receiver &sig_rec)
		:
			_sig_rec(sig_rec),
			_record_progress(_sig_rec, *this, &Recording::_handle_record_progress),
			_record_overrun(_sig_rec, *this, &Recording::_handle_record_overrun),
			_stream(*_audio_in.stream())
		{
			_audio_in.progress_sigh(_record_progress);
			_audio_in.overrun_sigh(_record_overrun);

			_audio_out[0] = new (&md_alloc) Audio_out::Connection("front left", true);
			_audio_out[1] = new (&md_alloc) Audio_out::Connection("front right", true);

			_audio_out[0]->start();
			_audio_out[1]->start();

			_audio_in.start();
		}
};

int main(int argc, char *argv[])
{
	Genode::log("--- Audio_in test ---");

	using namespace Genode;

	Signal_receiver sig_rec;

	Recording record(*env()->heap(), sig_rec);

	for (;;) {
		Signal sig = sig_rec.wait_for_signal();
		Signal_dispatcher_base *dispatcher =
			dynamic_cast<Signal_dispatcher_base *>(sig.context());

		if (dispatcher)
			dispatcher->dispatch(sig.num());
	}

	return 0;
}
