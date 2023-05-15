/*
 * \brief  Audio_out session interface
 * \author Sebastian Sumpf
 * \date   2012-12-20
 *
 * An audio session corresponds to one output channel, which can be used to
 * send audio frames. Each session consists of an 'Audio_out::Stream' object
 * that resides in shared memory between the client and the server. The
 * 'Audio_out::Stream' in turn consists of 'Audio_out::Packet's that contain
 * the actual frames. Each packet within a stream is freely accessible or may
 * be allocated successively. Also there is a current position pointer for each
 * stream that is updated by the server. This way, it is possible to send
 * sporadic events that need immediate processing as well as streams that rely
 * on buffering.
 *
 * Audio_out channel identifiers (loosely related to WAV channels) are:
 *
 * * front left (or left), front right (or right), front center
 * * lfe (low frequency effects, subwoofer)
 * * rear left, rear right, rear center
 *
 * For example, consumer-oriented 6-channel (5.1) audio uses front
 * left/right/center, rear left/right and lfe.
 *
 * Note: That most components right now only support: "(front) left" and
 * "(front) right".
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO_OUT_SESSION__AUDIO_OUT_SESSION_H_
#define _INCLUDE__AUDIO_OUT_SESSION__AUDIO_OUT_SESSION_H_

#include <base/allocator.h>
#include <base/rpc.h>
#include <base/signal.h>
#include <dataspace/capability.h>
#include <session/session.h>


namespace Audio_out {
	class Packet;
	class Stream;
	class Session;

	enum {
		QUEUE_SIZE  = 256,           /* buffer queue size */
		SAMPLE_RATE = 44100,
		SAMPLE_SIZE = sizeof(float),
	};

	/**
	 * Samples per perios (~11.6ms)
	 */
	static constexpr Genode::size_t PERIOD = 512;
}


/**
 * Audio_out packet containing frames
 */
class Audio_out::Packet
{
	private:

		friend class Session_client;
		friend class Stream;

		bool  _valid;
		bool  _wait_for_play;
		float _data[PERIOD];

		void _submit() { _valid = true; _wait_for_play = true; }
		void _alloc() { _wait_for_play = false; _valid = false; }

	public:

		Packet() : _valid(false), _wait_for_play(false) { }

		/**
		 * Copy data into packet, if there are less frames given than 'PERIOD',
		 * the remainder is filled with zeros
		 *
		 * \param  data  frames to copy in
		 * \param  size  number of frames to copy
		 */
		void content(float const *data, Genode::size_t samples)
		{
			Genode::memcpy(_data, data, (samples > PERIOD ? PERIOD : samples) * SAMPLE_SIZE);

			if (samples < PERIOD)
				Genode::memset(_data + samples, 0, (PERIOD - samples) * SAMPLE_SIZE);
		}

		/**
		 * Get content
		 *
		 * \return  pointer to frame data
		 */
		float *content() { return _data; }

		/**
		 * Play state
		 *
		 * \return  true if the packet has been played back; false otherwise
		 */
		bool played() const { return !_wait_for_play; }

		/**
		 * Valid state
		 *
		 * The valid state of a packet describes that the packet has been
		 * processed by the server even though it may not have been played back
		 * if the packet is invalid. For example, if a server is a filter, the
		 * audio may not have been processed by the output driver.
		 *
		 * \return  true if packet has *not* been processed yet; false otherwise
		 */
		bool valid() const { return _valid; }

		Genode::size_t size() const { return sizeof(_data); }


		/**********************************************
		 ** Intended to be called by the server side **
		 **********************************************/

		/**
		 * Invalidate packet, thus marking it as processed
		 */
		void invalidate() { _valid = false; }

		/**
		 * Mark a packet as played
		 */
		void mark_as_played() { _wait_for_play = false; }
};


/**
 * The audio-stream object containing packets
 *
 * The stream object is created upon session creation. The server will allocate
 * a dataspace on the client's account. The client session will then request
 * this dataspace and both client and server will attach it in their respective
 * protection domain. After that, the stream pointer within a session will be
 * pointed to the attached dataspace on both sides. Because the 'Stream' object
 * is backed by shared memory, its constructor is never supposed to be called.
 */
class Audio_out::Stream
{
	private:

		unsigned  _pos { 0 };       /* current playback position */
		unsigned  _tail { 0 };      /* tail pointer used for allocations */
		Packet    _buf[QUEUE_SIZE]; /* packet queue */

	public:

		/**
		 * Exceptions
		 */
		class Alloc_failed { };

		/**
		 * Current audio playback position
		 *
		 * \return  position
		 */
		unsigned pos() const { return _pos; }

		/**
		 * Current audio allocation position
		 *
		 * \return position
		 */
		unsigned tail() const { return _tail; }

		/**
		 * Number of packets between playback and allocation position
		 *
		 * \return number
		 */
		unsigned queued() const
		{
			if (_tail > _pos)
				return _tail - _pos;
			else if (_pos > _tail)
				return QUEUE_SIZE - (_pos - _tail);
			else
				return 0;
		}

		/**
		 * Retrieve next packet for given packet
		 *
		 * \param packet  preceding packet
		 *
		 * \return  Successor of packet or successor of current position if
		 *          'packet' is zero
		 */
		Packet *next(Packet *packet = 0)
		{
			return packet ? get(packet_position(packet) + 1) : get(pos() + 1);
		}

		/**
		 * Retrieves the position of a given packet in the stream queue
		 *
		 * \param packet  a packet
		 *
		 * \return  position in stream queue
		 */
		unsigned packet_position(Packet *packet) { return (unsigned)(packet - &_buf[0]); }

		/**
		 * Check if stream queue is full/empty
		 */
		bool empty() const
		{
			bool valid = false;
			for (int i = 0; i < QUEUE_SIZE; i++)
				valid |= _buf[i].valid();

			return !valid;
		}

		bool full() const { return (_tail + 1) % QUEUE_SIZE == _pos; }

		/**
		 * Retrieve an audio at given position
		 *
		 * \param  pos  position in stream
		 *
		 * \return  Audio_out packet
		 */
		Packet *get(unsigned pos) { return &_buf[pos % QUEUE_SIZE]; }

		
		/**
		 * Allocate a packet in stream
		 *
		 * \return  Packet
		 * \throw   Alloc_failed  when stream queue is full
		 */
		Packet *alloc()
		{
			if (full())
				throw Alloc_failed();
			unsigned pos = _tail;

			_tail = (_tail + 1) % QUEUE_SIZE;

			Packet *p = get(pos);
			p->_alloc();

			return p;
		}

		/**
		 * Reset stream queue
		 *
		 * This means that allocation will start at current queue position.
		 */
		void reset() { _tail = _pos; }


		/**
		 * Invalidate all packets in stream queue
		 */
		void invalidate_all()
		{
			for (int i = 0; i < QUEUE_SIZE; i++)
				_buf[i]._valid = false;
		}


		/**********************************************
		 ** Intended to be called by the server side **
		 ***********************************************/

		/**
		 * Set current stream position
		 *
		 * \param pos  current position
		 */
		void pos(unsigned p) { _pos = p; }

		/**
		 * Increment current stream position by one
		 */
		void increment_position() { _pos = (_pos + 1) % QUEUE_SIZE; }
};


/**
 * Audio_out session base
 */
class Audio_out::Session : public Genode::Session
{
	protected:

		Stream *_stream = nullptr;

	public:

		/**
		 * \noapi
		 */
		static const char *service_name() { return "Audio_out"; }

		static constexpr unsigned CAP_QUOTA = 4;

		/**
		 * Return stream of this session, see 'Stream' above
		 */
		Stream *stream() const { return _stream; }

		/**
		 * Start playback (alloc and submit packets after calling 'start')
		 */
		virtual void start() = 0;

		/**
		 * Stop playback
		 */
		virtual void stop() = 0;


		/*************
		 ** Signals **
		 *************/

		/**
		 * The 'progress' signal is sent from the server to the client if a
		 * packet has been played.
		 *
		 * See: client.h, connection.h
		 */
		virtual void progress_sigh(Genode::Signal_context_capability sigh) = 0;

		/**
		 * The 'alloc' signal is sent from the server to the client when the
		 * stream queue leaves the 'full' state.
		 *
		 * See: client.h, connection.h
		 */
		virtual void alloc_sigh(Genode::Signal_context_capability sigh)    = 0;
		
		/**
		 * The 'data_avail' signal is sent from the client to the server if the
		 * stream queue leaves the 'empty' state.
		 */
		virtual Genode::Signal_context_capability data_avail_sigh()        = 0;

		GENODE_RPC(Rpc_start, void, start);
		GENODE_RPC(Rpc_stop, void, stop);
		GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, dataspace);
		GENODE_RPC(Rpc_progress_sigh, void, progress_sigh, Genode::Signal_context_capability);
		GENODE_RPC(Rpc_alloc_sigh, void, alloc_sigh, Genode::Signal_context_capability);
		GENODE_RPC(Rpc_data_avail_sigh, Genode::Signal_context_capability, data_avail_sigh);

		GENODE_RPC_INTERFACE(Rpc_start, Rpc_stop, Rpc_dataspace, Rpc_progress_sigh,
		                     Rpc_alloc_sigh, Rpc_data_avail_sigh);
};

#endif /* _INCLUDE__AUDIO_OUT_SESSION__AUDIO_OUT_SESSION_H_ */
