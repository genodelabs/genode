/*
 * \brief  Audio_in session interface
 * \author Josef Soentgen
 * \date   2015-05-08
 *
 * An Audio_in session corresponds to one input channel, which can be used to
 * receive audio frames. Each session consists of an 'Audio_in::Stream' object
 * that resides in shared memory between the client and the server. The
 * 'Audio_in::Stream' in turn consists of 'Audio_in::Packet's that contain
 * the actual frames. Each packet within a stream is freely accessible. When
 * recording the source will allocate a new packet and override already
 * recorded ones if the queue is already full. In contrast to the
 * 'Audio_out::Stream' the current position pointer is updated by the client.
 */

/*
 * Copyright (C) 2015-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO_IN_SESSION__AUDIO_IN_SESSION_H_
#define _INCLUDE__AUDIO_IN_SESSION__AUDIO_IN_SESSION_H_

#include <base/allocator.h>
#include <dataspace/capability.h>
#include <base/rpc.h>
#include <session/session.h>


namespace Audio_in {
	class Packet;
	class Stream;
	class Session;

	enum {
		QUEUE_SIZE  = 431,            /* buffer queue size (~5s) */
		SAMPLE_RATE = 44100,
		SAMPLE_SIZE = sizeof(float),
	};

	/**
	 * Samples per period (~11.6ms)
	 */
	static constexpr Genode::size_t PERIOD = 512;
}


/**
 * Audio_in packet containing frames
 */
class Audio_in::Packet
{
	private:

		friend class Session_client;
		friend class Stream;

		bool  _valid;
		bool  _wait_for_record;
		float _data[PERIOD];

		void _submit() { _valid = true; _wait_for_record = true; }
		void _alloc() { _wait_for_record = false; _valid = false; }

	public:

		Packet() : _valid(false), _wait_for_record(false) { }

		/**
		 * Copy data into packet, if there are less frames given than 'PERIOD',
		 * the remainder is filled with zeros
		 *
		 * \param  data  frames to copy in
		 * \param  size  number of frames to copy
		 */
		void content(float *data, Genode::size_t samples)
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
		 * Record state
		 *
		 * \return  true if the packet has been recorded; false otherwise
		 */
		bool recorded() const { return !_wait_for_record; }

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
		 * Mark a packet as recorded
		 */
		void mark_as_recorded() { _wait_for_record = false; }
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
class Audio_in::Stream
{
	private:

		unsigned  _pos { 0 };       /* current record position */
		unsigned  _tail { 0 };      /* tail pointer used for allocations */
		Packet    _buf[QUEUE_SIZE]; /* packet queue */

	public:

		/**
		 * Current audio record position
		 *
		 * \return  position
		 */
		unsigned pos() const { return _pos; }

		/**
		 * Current tail position
		 *
		 * \return  tail position
		 */
		unsigned tail() const { return _tail; }

		/**
		 * Number of packets between record and allocation position
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
		 * Check if stream queue is empty
		 */
		bool empty() const
		{
			bool valid = false;
			for (int i = 0; i < QUEUE_SIZE; i++)
				valid |= _buf[i].valid();

			return !valid;
		}

		/**
		 * Retrieve an packet at given position
		 *
		 * \param  pos  position in stream
		 *
		 * \return  Audio_in packet
		 */
		Packet *get(unsigned pos) { return &_buf[pos % QUEUE_SIZE]; }

		/**
		 * Allocate a packet in stream
		 *
		 * \return  Packet
		 */
		Packet *alloc()
		{
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


		/**********************************************
		 ** Intended to be called by the server side **
		 **********************************************/

		/**
		 * Submit a packet to the packet queue
		 */
		void submit(Packet *p) { p->_submit(); }

		/**
		 * Check if stream queue has overrun
		 */
		bool overrun() const { return (_tail + 1) % QUEUE_SIZE == _pos; }


		/**********************************************
		 ** Intended to be called by the client side **
		 **********************************************/

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
 * Audio_in session base
 */
class Audio_in::Session : public Genode::Session
{
	protected:

		Stream *_stream { nullptr };

	public:

		/**
		 * \noapi
		 */
		static const char *service_name() { return "Audio_in"; }

		static constexpr unsigned CAP_QUOTA = 4;

		/**
		 * Return stream of this session, see 'Stream' above
		 */
		Stream *stream() const { return _stream; }

		/**
		 * Start recording (alloc and submit packets after calling 'start')
		 */
		virtual void start() = 0;

		/**
		 * Stop recording
		 */
		virtual void stop() = 0;


		/*************
		 ** Signals **
		 *************/

		/**
		 * The 'progress' signal is sent from the server to the client if a
		 * packet has been recorded.
		 *
		 * See: client.h, connection.h
		 */
		virtual void progress_sigh(Genode::Signal_context_capability sigh) = 0;

		/**
		 * The 'overrun' signal is sent from the server to the client if an
		 * overrun has occured.
		 *
		 * See: client.h, connection.h
		 */
		virtual void overrun_sigh(Genode::Signal_context_capability sigh) = 0;

		/**
		 * The 'data_avail' signal is sent from the server to the client if the
		 * stream queue leaves the 'empty' state.
		 */
		virtual Genode::Signal_context_capability data_avail_sigh()        = 0;

		GENODE_RPC(Rpc_start, void, start);
		GENODE_RPC(Rpc_stop, void, stop);
		GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, dataspace);
		GENODE_RPC(Rpc_progress_sigh, void, progress_sigh, Genode::Signal_context_capability);
		GENODE_RPC(Rpc_overrun_sigh, void, overrun_sigh, Genode::Signal_context_capability);
		GENODE_RPC(Rpc_data_avail_sigh, Genode::Signal_context_capability, data_avail_sigh);

		GENODE_RPC_INTERFACE(Rpc_start, Rpc_stop, Rpc_dataspace,
		                     Rpc_progress_sigh, Rpc_overrun_sigh,
		                     Rpc_data_avail_sigh);
};

#endif /* _INCLUDE__AUDIO_IN_SESSION__AUDIO_IN_SESSION_H_ */
