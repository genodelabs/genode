/*
 * \brief  Test for changing configuration at runtime
 * \author Norman Feske
 * \date   2012-04-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/config.h>


void parse_config()
{
	try {
		long counter = 1;
		Genode::config()->xml_node().sub_node("counter").value(&counter);
		Genode::printf("obtained counter value %ld from config\n", counter);

	} catch (...) {
		PERR("Error while parsing the configuration");
	}
}


int main(int, char **)
{
	parse_config();

	struct Signal_dispatcher : public Genode::Signal_context
	{
		void dispatch()
		{
			try {
				Genode::config()->reload();
				parse_config();
			} catch (Genode::Config::Invalid) {
				PERR("Error: reloading config failed");
			}
		}
	} signal_dispatcher;

	static Genode::Signal_receiver sig_rec;

	/* register signal handler for config changes */
	Genode::config()->sigh(sig_rec.manage(&signal_dispatcher));

	for (;;) {

		/* wait for config change */
		Genode::Signal signal = sig_rec.wait_for_signal();

		for (int i = 0; i < signal.num(); i++)
			static_cast<Signal_dispatcher *>(signal.context())->dispatch();
	}
	return 0;
}
