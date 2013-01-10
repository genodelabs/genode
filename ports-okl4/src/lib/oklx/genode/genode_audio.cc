/*
 * \brief  Genode C API audio functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-11-12
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/printf.h>
#include <audio_out_session/connection.h>

extern "C" {
#include <genode/config.h>
}

static unsigned long stream_data                = 0;
static void (*interrupt_handler)(unsigned long) = 0;

class Audio_cache
{
	public:

		typedef Audio_out::Session::Channel::Source Stream;

		enum Channels { LEFT, RIGHT, CHANNEL };

		enum {
			FRAME_SIZE_OUT   = Audio_out::FRAME_SIZE,
			FRAME_SIZE_IN    = sizeof(Genode::int16_t),
			PACKET_SIZE_OUT  = FRAME_SIZE_OUT * Audio_out::PERIOD,
			PACKET_SIZE_IN   = FRAME_SIZE_IN  * Audio_out::PERIOD * CHANNEL,
			PACKET_CNT_MAX   = Audio_out::QUEUE_SIZE - 1,
			BUF_SIZE         = PACKET_CNT_MAX * PACKET_SIZE_OUT + 0x400
		};

	private:

		class Entry
		{
			private:

				Stream           *_stream;
				Packet_descriptor _descriptor; /* packet stream descriptor */
				bool              _ready;      /* ready to submit */
				bool              _submitted;  /* submitted to hardware */

			public:

				Entry(Stream *stream)
				: _stream(stream), _descriptor(_stream->alloc_packet(PACKET_SIZE_OUT)),
				  _ready(false), _submitted(false) {}

				Entry() : _ready(false), _submitted(false) { }

				~Entry() {}

				Packet_descriptor descriptor() { return _descriptor; }

				float* content() {
					return _stream->packet_content(_descriptor); }

				void submit(bool delay=false)
				{
					_ready = true;
					if(interrupt_handler && !_submitted && !delay) {
						_submitted = true;
						_stream->submit_packet(_descriptor);
					}
				}

				bool submitted() { return _submitted; }

				bool ready() { return _ready; }

				void acknowledge()
				{
					_submitted = false;
					_ready     = false;
				}
		};


		Genode::Allocator_avl _alloc_left;
		Genode::Allocator_avl _alloc_right;
		Audio_out::Connection _left;
		Audio_out::Connection _right;
		Entry                 _entries[CHANNEL][PACKET_CNT_MAX];
		unsigned              _idx[CHANNEL];
		Genode::size_t        _offset;
		unsigned              _hw_pointer[CHANNEL];

		Stream *_stream(unsigned channel) {
			return channel == LEFT ? _left.stream() : _right.stream(); }

	public:

		Audio_cache() : _alloc_left(Genode::env()->heap()),
		                _alloc_right(Genode::env()->heap()),
		                _left("front left",   &_alloc_left, BUF_SIZE),
		                _right("front right", &_alloc_right, BUF_SIZE)
		{
			for (unsigned ch=LEFT; ch < CHANNEL; ch++) {
				for (unsigned i = 0; i < PACKET_CNT_MAX; i++)
					_entries[ch][i] = Entry(_stream(ch));
				_idx[ch]        = 0;
				_hw_pointer[ch] = 0;
			}
			_offset = 0;
			_right.sync_session(_left.session_capability());
		}

		~Audio_cache()
		{
			for (unsigned ch=LEFT; ch < CHANNEL; ch++)
				for (unsigned i = 0; i < PACKET_CNT_MAX; i++)
					_stream(ch)->release_packet(_entries[ch][i].descriptor());
		}

		void write (void *src, Genode::size_t size)
		{
			using namespace Genode;

			/* The actual packet should not be in use */
			for (unsigned ch=0; ch < CHANNEL; ch++)
				if (_entries[ch][_idx[ch]].submitted()) {
					PERR("Error: (un-)acknowledged packet chan=%d idx=%d",
					     ch, _idx[ch]);
				}

			short *src_ptr = (short*)src;
			while (size) {
				/* Use left space in packet or content-size */
				size_t packet_space =
					min<size_t>(size / CHANNEL / FRAME_SIZE_IN * FRAME_SIZE_OUT,
					            PACKET_SIZE_OUT - _offset);

				for (unsigned chan=0; chan < CHANNEL; chan++) {
					float *dest = _entries[chan][_idx[chan]].content()
						          + (_offset / FRAME_SIZE_OUT);
					for (unsigned frame = 0; frame < packet_space / FRAME_SIZE_OUT;
					     frame++)
						dest[frame] = src_ptr[(frame * CHANNEL) + chan] / 32767.0;
				}

				/* If packet is full, submit it and switch to next one */
				if ((_offset + packet_space) == PACKET_SIZE_OUT) {
					for (unsigned chan=0; chan < CHANNEL; chan++) {
						_entries[chan][_idx[chan]].submit(true);
						_idx[chan] = (_idx[chan] + 1) % PACKET_CNT_MAX;
					}
					_offset  = 0;
				} else
					_offset += packet_space;
				src_ptr += (packet_space / FRAME_SIZE_OUT) * CHANNEL;
				size    -= packet_space / FRAME_SIZE_OUT * FRAME_SIZE_IN * CHANNEL;
			}
		}

		void submit_all()
		{
			for (unsigned i = 0; i < PACKET_CNT_MAX; i++) {
				for (unsigned ch = 0; ch < CHANNEL; ch++) {
					if (_entries[ch][i].ready() && !_entries[ch][i].submitted())
						_entries[ch][i].submit();
				}
			}
		}

		void flush() {
			_left.flush();
			_right.flush();

			for (unsigned i = 0; i < PACKET_CNT_MAX; i++) {
				for (unsigned ch = 0; ch < CHANNEL; ch++) {
					if (_entries[ch][i].submitted())
						_stream(ch)->get_acked_packet();
					_entries[ch][i].acknowledge();
				}
			}

			for (unsigned ch = 0; ch < CHANNEL; ch++) {
				_idx[ch]        = 0;
				_hw_pointer[ch] = 0;
			}
			_offset = 0;
		}

		void acknowledge()
		{
			bool acked = false;
			for (unsigned ch = 0; ch < CHANNEL; ch++) {
				if (_stream(ch)->ack_avail()) {
					_stream(ch)->get_acked_packet();

					for (unsigned i = _idx[ch]; ;
						 i = (i + 1) % PACKET_CNT_MAX) {
						if (_entries[ch][i].submitted()) {
							_entries[ch][i].acknowledge();
							break;
						}
					}

					_hw_pointer[ch]++;
					acked = true;
				}
			}
			if (acked)
				interrupt_handler(stream_data);
		}

		Genode::size_t pointer()
		{
			using namespace Genode;
			Genode::size_t size = min<size_t>(_hw_pointer[LEFT], _hw_pointer[RIGHT]);
			return size * Audio_cache::PACKET_SIZE_OUT * CHANNEL
				/ FRAME_SIZE_OUT * FRAME_SIZE_IN ;
		}
};


static Audio_cache *audio_cache(void)
{
	try {
		static Audio_cache _cache;
		return &_cache;
	} catch(...) {}
	return 0;
}

static Genode::uint8_t zeroes[Audio_cache::PACKET_SIZE_IN];


extern "C" {
#include <genode/audio.h>

	int genode_audio_ready()
	{
		return genode_config_audio() && audio_cache();
	}


	void genode_audio_collect_acks()
	{
		if (!interrupt_handler)
			return;
		audio_cache()->submit_all();
		audio_cache()->acknowledge();
	}


	void genode_audio_prepare()
	{
		audio_cache()->flush();
	}


	void genode_audio_trigger_start(void (*func)(unsigned long), unsigned long data)
	{
		interrupt_handler = func;
		stream_data       = data;
	}


	void genode_audio_trigger_stop()
	{
		interrupt_handler = 0;
		stream_data       = 0;
		audio_cache()->flush();
	}


	unsigned long genode_audio_position()
	{
		unsigned long ret = audio_cache()->pointer();
		return ret;
	}


	void genode_audio_write(void* src, unsigned long sz)
	{
		audio_cache()->write(src, sz);
	}


	void genode_audio_fill_silence(unsigned long sz)
	{
		audio_cache()->write(&zeroes, sz);
	}


	unsigned int genode_audio_packet_size() { return Audio_cache::PACKET_SIZE_IN; }


	unsigned int genode_audio_packet_count() { return Audio_cache::PACKET_CNT_MAX; }

} // extern "C"
