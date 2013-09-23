/*
 * \brief  Test for the NIC loop-back service
 * \author Norman Feske
 * \date   2009-11-14
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/allocator_avl.h>
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <timer_session/connection.h>

using namespace Genode;


static bool single_packet_roundtrip(Nic::Session  *nic,
                                    unsigned char  content_pattern,
                                    size_t         packet_size)
{
	Packet_descriptor tx_packet;

	printf("single_packet_roundtrip(content='%c', packet_size=%zd)\n",
	       content_pattern, packet_size);

	/* allocate transmit packet */
	try {
		tx_packet = nic->tx()->alloc_packet(packet_size);
	} catch (Nic::Session::Tx::Source::Packet_alloc_failed) {
		PERR("tx packet alloc failed");
		return false;
	}

	printf("allocated tx packet (offset=%ld, size=%zd)\n",
	       tx_packet.offset(), tx_packet.size());

	/* fill packet with pattern */
	char *tx_content = nic->tx()->packet_content(tx_packet);
	for (unsigned i = 0; i < packet_size; i++)
		tx_content[i] = content_pattern;

	nic->tx()->submit_packet(tx_packet);

	/* wait for acknowledgement */
	Packet_descriptor ack_tx_packet = nic->tx()->get_acked_packet();

	if (ack_tx_packet.size()   != tx_packet.size()
	 || ack_tx_packet.offset() != tx_packet.offset()) {
		PERR("unexpected acked packet");
		return false;
	}

	/*
	 * Because we sent the packet to a loop-back device, we expect
	 * the same packet to be available at the rx channel of the NIC
	 * session.
	 */
	Packet_descriptor rx_packet = nic->rx()->get_packet();
	printf("received rx packet (offset=%ld, size=%zd)\n",
	       tx_packet.offset(), tx_packet.size());

	if (rx_packet.size() != tx_packet.size()) {
		PERR("sent and echoed packets differ in size");
		return false;
	}

	/* compare original and echoed packets */
	char *rx_content = nic->rx()->packet_content(rx_packet);
	for (unsigned i = 0; i < packet_size; i++)
		if (rx_content[i] != tx_content[i]) {
			PERR("sent and echoed packets have differnt content");
			return false;
		}

	/* acknowledge received packet */
	nic->rx()->acknowledge_packet(rx_packet);

	/* release sent packet to free the space in the tx communication buffer */
	nic->tx()->release_packet(tx_packet);

	return true;
}


static bool batch_packets(Nic::Session *nic, unsigned num_packets)
{
	unsigned tx_cnt = 0, acked_cnt = 0, rx_cnt = 0, batch_cnt = 0;

	Genode::Signal_context tx_ready_to_submit, tx_ack_avail,
	                       rx_ready_to_ack, rx_packet_avail;
	Genode::Signal_receiver signal_receiver;

	nic->tx_channel()->sigh_ready_to_submit (signal_receiver.manage(&tx_ready_to_submit));
	nic->tx_channel()->sigh_ack_avail       (signal_receiver.manage(&tx_ack_avail));
	nic->rx_channel()->sigh_ready_to_ack    (signal_receiver.manage(&rx_ready_to_ack));
	nic->rx_channel()->sigh_packet_avail    (signal_receiver.manage(&rx_packet_avail));

	enum { PACKET_SIZE = 100 };

	while (acked_cnt != num_packets
	    || tx_cnt    != num_packets
	    || rx_cnt    != num_packets) {

		if (tx_cnt > rx_cnt || tx_cnt > acked_cnt)
			signal_receiver.wait_for_signal();

		/* produce as many packets as possible as one batch */
		unsigned max_outstanding_requests = Nic::Session::QUEUE_SIZE - 1;
		while (nic->tx()->ready_to_submit()
		    && tx_cnt < num_packets
		    && tx_cnt - rx_cnt < max_outstanding_requests) {

			try {
				Packet_descriptor tx_packet = nic->tx()->alloc_packet(PACKET_SIZE);
				nic->tx()->submit_packet(tx_packet);
				tx_cnt++;
			} catch (Nic::Session::Tx::Source::Packet_alloc_failed) {
				break;
			}
		}

		unsigned batch_rx_cnt = 0, batch_acked_cnt = 0;

		/* check for acknowledgements */
		while (nic->tx()->ack_avail()) {
			Packet_descriptor acked_packet = nic->tx()->get_acked_packet();
			nic->tx()->release_packet(acked_packet);
			acked_cnt++;
			batch_acked_cnt++;
		}

		/* check for available rx packets */
		while (nic->rx()->packet_avail() && nic->rx()->ready_to_ack()) {
			Packet_descriptor rx_packet = nic->rx()->get_packet();

			if (!nic->rx()->ready_to_ack())
				PWRN("not ready for ack, going to blocK");

			nic->rx()->acknowledge_packet(rx_packet);
			rx_cnt++;
			batch_rx_cnt++;
		}

		printf("acked %u packets, received %u packets "
		       "-> tx: %u, acked: %u, rx: %u\n",
		       batch_acked_cnt, batch_rx_cnt, tx_cnt, acked_cnt, rx_cnt);

		batch_cnt++;
	}

	printf("test used %u batches\n", batch_cnt);
	return true;
}


int main(int, char **)
{
	printf("--- NIC loop-back test ---\n");

	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	bool config_test_roundtrip = true;
	bool config_test_batch     = true;

	if (config_test_roundtrip) {
		printf("-- test roundtrip two times (packet offsets should be the same) --\n");
		Allocator_avl tx_block_alloc(env()->heap());
		Nic::Connection nic(&tx_block_alloc, BUF_SIZE, BUF_SIZE);
		single_packet_roundtrip(&nic, 'a', 100);
		single_packet_roundtrip(&nic, 'b', 100);
	}

	if (config_test_batch) {
		printf("-- test submitting and receiving batches of packets --\n");
		Allocator_avl tx_block_alloc(env()->heap());
		Nic::Connection nic(&tx_block_alloc, BUF_SIZE, BUF_SIZE);
		enum { NUM_PACKETS = 1000 };
		batch_packets(&nic, NUM_PACKETS);
	}

	printf("--- finished NIC loop-back test ---\n");
	return 0;
}
