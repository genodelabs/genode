/*
 * \brief  Packet statistics
 * \author Johannes Schlatow
 * \date   2022-06-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PACKET_STATS_H_
#define _PACKET_STATS_H_

/* Genode includes */
#include <base/session_label.h>

namespace Nic_perf {
	using namespace Genode;

	class Packet_stats;
}

class Nic_perf::Packet_stats
{
	private:

		Session_label            const &_label;

		size_t   _sent_cnt    { 0 };
		size_t   _recv_cnt    { 0 };
		size_t   _sent_bytes  { 0 };
		size_t   _recv_bytes  { 0 };
		unsigned _period_ms   { 0 };
		float    _rx_mbit_sec { 0.0 };
		float    _tx_mbit_sec { 0.0 };

	public:

		Packet_stats(Session_label const & label)
		: _label(label)
		{ }

		void reset()
		{
			_sent_cnt = 0;
			_recv_cnt = 0;
			_sent_bytes = 0;
			_recv_bytes = 0;
			_rx_mbit_sec = 0;
			_tx_mbit_sec = 0;
		}

		void rx_packet(size_t bytes)
		{
			_recv_cnt++;
			_recv_bytes += bytes;
		}

		void tx_packet(size_t bytes)
		{
			_sent_cnt++;
			_sent_bytes += bytes;
		}

		void calculate_throughput(unsigned period_ms)
		{
			_period_ms   = period_ms;

			if (_period_ms == 0) return;

			_rx_mbit_sec = (float)(_recv_bytes * 8ULL) / (float)(period_ms*1000ULL);
			_tx_mbit_sec = (float)(_sent_bytes * 8ULL) / (float)(period_ms*1000ULL);
		}

		void print(Output &out) const
		{
			Genode::print(out, "# Stats for session ", _label, "\n");
			Genode::print(out, "  Received ", _recv_cnt, " packets in ",
			              _period_ms, "ms at ", _rx_mbit_sec, "Mbit/s\n");
			Genode::print(out, "  Sent     ", _sent_cnt, " packets in ",
			              _period_ms, "ms at ", _tx_mbit_sec, "Mbit/s\n");
		}

};


#endif /* _PACKET_STATS_H_ */
