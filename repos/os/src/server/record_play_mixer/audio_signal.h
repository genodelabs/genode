/*
 * \brief  Audio-signal types
 * \author Norman Feske
 * \date   2023-12-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _AUDIO_SIGNAL_H_
#define _AUDIO_SIGNAL_H_

/* Genode includes */
#include <util/list_model.h>

/* local includes */
#include <types.h>

namespace Mixer { class Audio_signal; }


class Mixer::Audio_signal : List_model<Audio_signal>::Element, public Sample_producer
{
	public:

		using Name = String<32>;

		static constexpr auto mix_type_name = "mix";

	private:

		friend class List_model<Audio_signal>;
		friend class List<Audio_signal>;

	public:

		Name const name;

		Audio_signal(Name const name) : name(name) { };

		virtual void update(Xml_node const &) { }

		virtual void bind_inputs(List_model<Audio_signal> &, Play_sessions &) = 0;

		/**
		 * List_model::Element
		 */
		bool matches(Xml_node const &node) const
		{
			return node.attribute_value("name", Name()) == name;
		}

		/**
		 * List_model::Element
		 */
		static bool type_matches(Xml_node const &node)
		{
			return node.has_type(mix_type_name);
		}
};

#endif /* _AUDIO_SIGNAL_H_ */
