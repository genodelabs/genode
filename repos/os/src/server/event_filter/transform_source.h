/*
 * \brief  Input-event source that transforms motion events
 * \author Christian Helmuth
 * \date   2024-01-28
 *
 * This filter configurably transforms touch and absolute-motion event
 * coordinates by a sequence of translation, scaling, rotation, and flipping
 * primitives.
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__TRANSFORM_SOURCE_H_
#define _EVENT_FILTER__TRANSFORM_SOURCE_H_

/* local includes */
#include <source.h>
#include <affine_transform.h>

namespace Event_filter { class Transform_source; }


class Event_filter::Transform_source : public Source, Source::Filter
{
	private:

		Owner _owner;

		Source &_source;

		Transform::Matrix _transform = Transform::Matrix::identity();

		void _apply_config(Xml_node const config)
		{
			config.for_each_sub_node([&] (Xml_node node) {
				if (node.has_type("translate")) {
					_transform = _transform.translate(
						float(node.attribute_value("x", 0.0)),
						float(node.attribute_value("y", 0.0)));
					return;
				}
				if (node.has_type("scale")) {
					_transform = _transform.scale(
						float(node.attribute_value("x", 1.0)),
						float(node.attribute_value("y", 1.0)));
					return;
				}
				if (node.has_type("rotate")) {
					unsigned degrees = node.attribute_value("angle", 0);
					Transform::Angle angle =
						Transform::angle_from_degrees(degrees);

					if (angle == Transform::ANGLE_0)
						warning("invalid transform rotate(", degrees, ")");
					else
						_transform = _transform.rotate(angle);
					return;
				}
				if (node.has_type("hflip")) {
					float width = float(node.attribute_value("width", -1.0));

					if (width <= 0)
						warning("invalid transform hflip");
					else
						_transform = _transform.hflip(width);
					return;
				}
				if (node.has_type("vflip")) {
					float height = float(node.attribute_value("height", -1.0));

					if (height <= 0)
						warning("invalid transform vflip");
					else
						_transform = _transform.vflip(height);
					return;
				}
				if (node.has_type("reorient")) {
					float width      = float(node.attribute_value("width",  -1.0));
					float height     = float(node.attribute_value("height", -1.0));
					unsigned degrees = node.attribute_value("angle", 0);

					Transform::Angle angle =
						Transform::angle_from_degrees(degrees);

					if (width <= 0 || height <= 0 || angle == Transform::ANGLE_0)
						warning("invalid transform reorient");
					else
						_transform = _transform.reorient(angle, width, height);
					return;
				}
			});
		}

		/**
		 * Filter interface
		 */
		void filter_event(Sink &destination, Input::Event const &event) override
		{
			if (!event.absolute_motion() && !event.touch()) {
				destination.submit(event);
				return;
			}

			event.handle_touch([&] (Input::Touch_id id, float x, float y) {
				Transform::Point p { x, y };
				p = p.transform(_transform);
				destination.submit(Input::Touch { id, p.x, p.y });
			});

			event.handle_absolute_motion([&] (int x, int y) {
				Transform::Point p { float(x), float(y) };
				p = p.transform(_transform);
				destination.submit(Input::Absolute_motion { p.int_x(), p.int_y() });
			});
		}

	public:

		static char const *name() { return "transform"; }

		Transform_source(Owner &owner, Xml_node config, Source::Factory &factory)
		:
			Source(owner),
			_owner(factory),
			_source(factory.create_source(_owner, input_sub_node(config)))
		{
			_apply_config(config);
		}

		void generate(Sink &destination) override
		{
			Source::Filter::apply(destination, *this, _source);
		}
};

#endif /* _EVENT_FILTER__TRANSFORM_SOURCE_H_ */
