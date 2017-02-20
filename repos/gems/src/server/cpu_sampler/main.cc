/*
 * \brief  CPU sampler
 * \author Christian Prochaska
 * \date   2016-01-15
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <cpu_session/cpu_session.h>
#include <base/attached_dataspace.h>
#include <os/server.h>
#include <os/session_policy.h>
#include <os/static_root.h>
#include <timer_session/connection.h>
#include <util/list.h>

/* local includes */
#include "cpu_root.h"
#include "cpu_session_component.h"
#include "cpu_thread_component.h"
#include "thread_list_change_handler.h"

namespace Cpu_sampler { struct Main; }

static constexpr bool verbose = false;
static constexpr bool verbose_missed_timeouts = false;
static constexpr bool verbose_sample_duration = true;


/******************
 ** Main program **
 ******************/

struct Cpu_sampler::Main : Thread_list_change_handler
{
	Genode::Env            &env;
	Genode::Heap            alloc;
	Cpu_root                cpu_root;
	Attached_rom_dataspace  config;
	Timer::Connection       timer;
	Thread_list             thread_list;
	Thread_list             selected_thread_list;

	unsigned int            sample_index;
	unsigned int            max_sample_index;
	unsigned int            timeout_us;


	void handle_timeout(unsigned int num)
	{
		if (verbose_missed_timeouts && (num > 1))
			Genode::log("missed ", num - 1, " timeouts");

		auto lambda = [&] (Thread_element *cpu_thread_element) {

			Cpu_thread_component *cpu_thread = cpu_thread_element->object();

			cpu_thread->take_sample();

			if (sample_index == max_sample_index)
				cpu_thread->flush();
		};

		for_each_thread(selected_thread_list, lambda);

		if (verbose_sample_duration && (sample_index == max_sample_index))
			Genode::log("sample period finished");

		sample_index++;

		if (sample_index == max_sample_index)
			timer.trigger_once(timeout_us);
	}


	Signal_rpc_member<Main> timeout_dispatcher =
		{ env.ep(), *this, &Main::handle_timeout };


	void handle_config_update(unsigned)
	{
		config.update();

		sample_index = 0;

		unsigned int sample_interval_ms =
			config.xml().attribute_value<unsigned int>("sample_interval_ms", 1000);

		unsigned int sample_duration_s =
			config.xml().attribute_value<unsigned int>("sample_duration_s", 10);

		max_sample_index = ((sample_duration_s * 1000) / sample_interval_ms) - 1;

		timeout_us = sample_interval_ms * 1000;

		thread_list_changed();

		if (verbose_sample_duration)
			Genode::log("starting a new sample period");

		timer.trigger_periodic(timeout_us);
	}


	Signal_rpc_member<Main> config_update_dispatcher =
		{ env.ep(), *this, &Main::handle_config_update};


	void thread_list_changed() override
	{
		/* clear selected_thread_list */

		auto remove_lambda = [&] (Thread_element *cpu_thread_element) {

			if (verbose)
				Genode::log("removing thread ",
				            cpu_thread_element->object()->label().string(),
				            " from selection");

			selected_thread_list.remove(cpu_thread_element);
			destroy(&alloc, cpu_thread_element);
		};

		for_each_thread(selected_thread_list, remove_lambda);


		/* generate new selected_thread_list */

		auto insert_lambda = [&] (Thread_element *cpu_thread_element) {

			Cpu_thread_component *cpu_thread = cpu_thread_element->object();

			if (verbose)
				Genode::log("evaluating thread ", cpu_thread->label().string());

			try {

				Session_policy policy(cpu_thread->label(), config.xml());
				cpu_thread->reset();
				selected_thread_list.insert(new (&alloc)
				                            Thread_element(cpu_thread));

				if (verbose)
					Genode::log("added thread ",
					            cpu_thread->label().string(),
					            " to selection");

			} catch (Session_policy::No_policy_defined) {

				if (verbose)
					Genode::log("no session policy defined for thread ",
					            cpu_thread->label().string());
			}
		};

		for_each_thread(thread_list, insert_lambda);
	}


	/**
	 * Constructor
	 */
	Main(Genode::Env &env)
	: env(env),
	  alloc(env.ram(), env.rm()),
	  cpu_root(env.ep().rpc_ep(), env.ep().rpc_ep(), env, alloc, thread_list, *this),
	  config(env, "config")
	{
		/*
		 * Register signal handlers
		 */
		config.sigh(config_update_dispatcher);
		timer.sigh(timeout_dispatcher);

		/*
		 * Apply initial configuration
		 */
		handle_config_update(0);

		/*
		 * Announce service
		 */
		env.parent().announce(env.ep().manage(cpu_root));
	}

};


/***************
 ** Component **
 ***************/

void Component::construct(Genode::Env &env) { static Cpu_sampler::Main inst(env); }
