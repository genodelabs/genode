/*
 * \brief  Mix signal
 * \author Norman Feske
 * \date   2023-18-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MIX_SIGNAL_H_
#define _MIX_SIGNAL_H_

/* local includes */
#include <audio_signal.h>

namespace Mixer { class Mix_signal; }


struct Mixer::Mix_signal : Audio_signal
{
	Allocator &_alloc;

	Volume _volume { };

	struct Input : Interface
	{
		Volume volume;

		Input(Xml_node const &node) : volume(Volume::from_xml(node)) { }
	};

	struct Named_signal_input : Input
	{
		using Name = Audio_signal::Name;

		Name const name;

		struct { Sample_producer *_sample_producer_ptr = nullptr; };

		Named_signal_input(Xml_node const &node)
		:
			Input(node), name(node.attribute_value("name", Name()))
		{ }

		void with_sample_producer(auto const &fn)
		{
			if (_sample_producer_ptr)
				fn(*_sample_producer_ptr, volume);
		}
	};

	struct Play_session_input : Input
	{
		using Label  = String<64>;
		using Suffix = String<64>;

		Allocator &_alloc;

		Label    const label;
		Suffix   const suffix;
		unsigned const jitter_us;

		struct Sample_producer_ptr { Sample_producer *ptr = nullptr; };

		using Registered_sample_producer_ptr = Registered_no_delete<Sample_producer_ptr>;

		Registry<Registered_sample_producer_ptr> _sample_producer_ptrs { };

		Play_session_input(Xml_node const &node, Allocator &alloc)
		:
			Input(node), _alloc(alloc),
			label (node.attribute_value("label", Label())),
			suffix(node.attribute_value("label_suffix", Suffix())),
			jitter_us(us_from_ms_attr(node, "jitter_ms", 0.0))
		{ }

		~Play_session_input() { detach_all_producers(); }

		void try_attach(Play_session &session)
		{
			auto matches = [&] (Session_label const &session_label) -> bool
			{
				if (label.valid() && label == session_label)
					return true;

				if (!suffix.valid())
					return false;

				if (session_label.length() < suffix.length())
					return false;

				size_t const len = suffix.length() - 1;
				char const * const label_suffix_cstring =
					session_label.string() + session_label.length() - 1 - len;

				return (strcmp(label_suffix_cstring, suffix.string(), len) == 0);
			};

			if (!matches(session.label()))
				return;

			session.expect_jitter_us(jitter_us);

			new (_alloc) Registered_sample_producer_ptr(_sample_producer_ptrs,
			                                            Sample_producer_ptr { &session });
		}

		void detach_all_producers()
		{
			_sample_producer_ptrs.for_each([&] (Registered_sample_producer_ptr &ptr) {
				destroy(_alloc, &ptr); });
		}

		void for_each_sample_producer(auto const &fn)
		{
			_sample_producer_ptrs.for_each([&] (Sample_producer_ptr &ptr) {
				if (ptr.ptr)
					fn(*ptr.ptr, volume); });
		}
	};

	Registry<Registered<Named_signal_input>> _named_signal_inputs { };
	Registry<Registered<Play_session_input>> _play_session_inputs { };

	Sample_buffer<512> _input_buffer { };

	bool _input_buffer_used = false;
	bool _warned_once = false;

	/*
	 * Helper to protect against nested call of 'produce_sample_data'
	 */
	struct Used_guard
	{
		bool &_used;
		Used_guard(bool &used) : _used(used) { _used = true; }
		~Used_guard() { _used = false; }
	};

	Mix_signal(Xml_node const &node, Allocator &alloc)
	:
		Audio_signal(node.attribute_value("name", Name())),
		_alloc(alloc)
	{ }

	/**
	 * Audio_signal interface
	 */
	void update(Xml_node const &node) override
	{
		_volume = Volume::from_xml(node);

		_named_signal_inputs.for_each([&] (Registered<Named_signal_input> &input) {
			destroy(_alloc, &input); });

		_play_session_inputs.for_each([&] (Registered<Play_session_input> &input) {
			destroy(_alloc, &input); });

		node.for_each_sub_node([&] (Xml_node const &input_node) {

			if (input_node.has_type("signal"))
				new (_alloc) Registered<Named_signal_input>(_named_signal_inputs, input_node);

			if (input_node.has_type("play"))
				new (_alloc) Registered<Play_session_input>(_play_session_inputs, input_node, _alloc);
		});
	}

	/**
	 * Audio_signal interface
	 */
	void bind_inputs(List_model<Audio_signal> &named_signals,
	                 Play_sessions            &play_sessions) override
	{
		_named_signal_inputs.for_each([&] (Named_signal_input &input) {
			named_signals.for_each([&] (Audio_signal &named_signal) {
				if (named_signal.name == input.name) {
					input._sample_producer_ptr = &named_signal; } }); });

		_play_session_inputs.for_each([&] (Play_session_input &input) {
			input.detach_all_producers();
			play_sessions.for_each([&] (Play_session &play_session) {
				input.try_attach(play_session); }); });
	}

	/**
	 * Sample_producer interface
	 */
	bool produce_sample_data(Time_window const tw, Float_range_ptr &samples) override
	{
		if (_input_buffer_used) {
			if (!_warned_once)
				error("attempt to feed <mix> output (", name, ") as input to the same node");
			_warned_once = true;
			return false;
		}

		samples.clear();

		Used_guard guard(_input_buffer_used);

		auto for_each_sample_producer = [&] (auto const &fn)
		{
			_named_signal_inputs.for_each([&] (Named_signal_input &input) {
				input.with_sample_producer([&] (Sample_producer &p, Volume v) {
					fn(p, v); }); });

			_play_session_inputs.for_each([&] (Play_session_input &input) {
				input.for_each_sample_producer([&] (Sample_producer &p, Volume v) {
					fn(p, v); }); });
		};

		bool result = false;

		for_each_sub_window<_input_buffer.CAPACITY>(tw, samples,
			[&] (Time_window sub_tw, Float_range_ptr &dst) {
				for_each_sample_producer([&] (Sample_producer &producer, Volume volume) {

					/* render input into '_input_buffer", mix result into 'dst' */
					Float_range_ptr input_dst(_input_buffer.values, dst.num_floats);
					input_dst.clear();
					result |= producer.produce_sample_data(sub_tw, input_dst);
					input_dst.scale(volume.value);
					dst.add(input_dst);
				});
			});

		samples.scale(_volume.value);
		return result;
	}
};

#endif /* _MIX_SIGNAL_H_ */
