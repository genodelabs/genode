/*
 * \brief  Representation of connectors reported by the framebuffer driver
 * \author Norman Feske
 * \date   2024-10-23
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__FB_CONNECTORS_H_
#define _MODEL__FB_CONNECTORS_H_

#include <types.h>

namespace Sculpt { struct Fb_connectors; };


struct Sculpt::Fb_connectors : private Noncopyable
{
	using Area = Gui::Area;
	using Name = String<16>;

	struct Connector;
	using  Connectors = List_model<Connector>;

	struct Brightness
	{
		bool     defined;
		unsigned percent;

		bool operator != (Brightness const &other) const
		{
			return defined != other.defined || percent != other.percent;
		}

		static Brightness from_node(Node const &node)
		{
			return { .defined = node.has_attribute("brightness"),
			         .percent = node.attribute_value("brightness", 0u) };
		}
	};

	struct Orientation
	{
		enum class Rotate { UNSUPPORTED, R0, R90, R180, R270 } rotate;
		enum class Flip   { UNSUPPORTED, NO, YES }             flip;

		Flip toggled_flip() const
		{
			return flip == Flip::NO  ? Flip::YES
			     : flip == Flip::YES ? Flip::NO
			     :                     Flip::UNSUPPORTED;
		}

		bool operator != (Orientation const &other) const
		{
			return rotate != other.rotate || flip != other.flip;
		}

		static Orientation from_node(Node const &node)
		{
			unsigned const rotate = node.attribute_value("rotate", ~0u);
			bool     const flip   = node.attribute_value("flip",   false);
			bool     const flip_supported = node.has_attribute("flip");

			return {
				.rotate = (rotate ==   0) ? Rotate::R0
				        : (rotate ==  90) ? Rotate::R90
				        : (rotate == 180) ? Rotate::R180
				        : (rotate == 270) ? Rotate::R270
				        :                   Rotate::UNSUPPORTED,

				.flip   = (flip_supported &&  flip) ? Flip::YES
				        : (flip_supported && !flip) ? Flip::NO
				        :                             Flip::UNSUPPORTED
			};
		}

		unsigned angle() const
		{
			switch (rotate) {
			case Rotate::UNSUPPORTED:
			case Rotate::R0:   break;
			case Rotate::R90:  return 90;
			case Rotate::R180: return 180;
			case Rotate::R270: return 270;
			}
			return 0;
		}

		void gen_attr(Generator &g) const
		{
			if (rotate != Rotate::UNSUPPORTED)
				g.attribute("rotate", angle());

			if (flip != Flip::UNSUPPORTED)
				g.attribute("flip", flip == Flip::YES ? "yes" : "no");
		}

		bool rotate_supported() const { return rotate != Rotate::UNSUPPORTED; }
		bool flip_supported()   const { return flip   != Flip::UNSUPPORTED;   }

		bool supported() const { return rotate_supported() || flip_supported(); }
	};

	struct Connector : Connectors::Element
	{
		Name  const name;
		Area        mm { };
		Brightness  brightness  { };
		Orientation orientation { };

		struct Mode;
		using  Modes = List_model<Mode>;

		Modes _modes { };

		struct Mode : Modes::Element
		{
			using Id = String<16>;

			Id const id;

			struct Attr
			{
				Name     name;
				Area     px;
				Area     mm;
				bool     used;
				unsigned hz;

				bool operator != (Attr const &other) const
				{
					return name      != other.name
					    || px        != other.px
					    || mm        != other.mm
					    || used      != other.used
					    || hz        != other.hz;
				}

				static Attr from_node(Node const &node)
				{
					return {
						.name      = node.attribute_value("name",      Name()),
						.px = { .w = node.attribute_value("width",     0u),
						        .h = node.attribute_value("height",    0u) },
						.mm = { .w = node.attribute_value("width_mm",  0u),
						        .h = node.attribute_value("height_mm", 0u) },
						.used      = node.attribute_value("used",      false),
						.hz        = node.attribute_value("hz",        0u)
					};
				}
			};

			Attr attr { };

			Mode(Id id) : id(id) { };

			static Id id_from_node(Node const &node)
			{
				return node.attribute_value("id", Mode::Id());
			}

			bool matches(Node const &node) const
			{
				return id_from_node(node) == id;
			}

			static bool type_matches(Node const &node)
			{
				return node.has_type("mode");
			}

			bool has_resolution(Area     px) const { return attr.px == px; }
			bool has_hz        (unsigned hz) const { return attr.hz == hz; }
		};

		Connector(Name const &name) : name(name) { }

		void _with_mode(auto const &match_fn, auto const &fn) const
		{
			bool found = false;
			_modes.for_each([&] (Mode const &mode) {
				if (!found && match_fn(mode.attr)) {
					fn(mode);
					found = true; } });
		}

		void with_used_mode(auto const &fn) const
		{
			_with_mode([&] (Mode::Attr const &attr) { return attr.used; }, fn);
		}

		void with_matching_mode(Mode::Id const &preferred_id,
		                        Mode::Attr const &attr, auto const &fn) const
		{
			auto matches_resolution_and_id = [&] (Mode const &mode)
			{
				return mode.has_resolution(attr.px) && (mode.id == preferred_id);
			};

			auto matches_resolution_and_hz = [&] (Mode const &mode)
			{
				return mode.has_resolution(attr.px) && mode.has_hz(attr.hz);
			};

			auto matches_resolution = [&] (Mode const &mode)
			{
				return mode.has_resolution(attr.px);
			};

			bool matched = false;
			auto with_match_once = [&] (auto const &matches_fn, auto const &fn)
			{
				if (!matched)
					_modes.for_each([&] (Mode const &mode) {
						if (matches_fn(mode)) {
							fn(mode);
							matched = true; } });
			};

			with_match_once(matches_resolution_and_id, fn);
			with_match_once(matches_resolution_and_hz, fn);
			with_match_once(matches_resolution,        fn);

			if (!matched)
				with_used_mode([&] (Mode const &used) {
					fn(used);
					matched = true; });
		}

		bool update(Allocator &alloc, Node const &node)
		{
			Area        const orig_mm = mm;
			Brightness  const orig_brightness  = brightness;
			Orientation const orig_orientation = orientation;

			mm.w        = node.attribute_value("width_mm",  0u);
			mm.h        = node.attribute_value("height_mm", 0u);
			brightness  = Brightness::from_node(node);
			orientation = Orientation::from_node(node);

			bool progress = orig_mm          != mm
			             || orig_brightness  != brightness
			             || orig_orientation != orientation;

			_modes.update_from_node(node,
				[&] (Node const &node) -> Mode & {
					progress = true;
					return *new (alloc) Mode(Mode::id_from_node(node));
				},
				[&] (Mode &mode) {
					progress = true;
					destroy(alloc, &mode);
				},
				[&] (Mode &mode, Node const &node) {
					Mode::Attr const orig_attr = mode.attr;
					mode.attr = Mode::Attr::from_node(node);
					progress |= (orig_attr != mode.attr);
				}
			);

			return progress;
		}

		bool matches(Node const &node) const
		{
			return node.attribute_value("name", Name()) == name;
		}

		static bool type_matches(Node const &node)
		{
			return node.has_type("connector")
			    && node.attribute_value("connected", false)
			    && node.has_sub_node("mode");
		}
	};

	Connectors _merged   { };
	Connectors _discrete { };

	[[nodiscard]] Progress update(Allocator &alloc, Node const &connectors)
	{
		bool progress = false;

		auto update = [&] (Connectors &model, Node const &node)
		{
			model.update_from_node(node,
				[&] (Node const &node) -> Connector & {
					progress = true;
					return *new (alloc) Connector(node.attribute_value("name", Name()));
				},
				[&] (Connector &conn) {
					progress = true;
					conn.update(alloc, Node());
					destroy(alloc, &conn);
				},
				[&] (Connector &conn, Node const &node) {
					progress |= conn.update(alloc, node);
				});
		};

		update(_discrete, connectors);

		connectors.with_sub_node("merge",
			[&] (Node const &merge) { update(_merged, merge); },
			[&]                     { update(_merged, Node()); });

		return { progress };
	}

	static unsigned _count(Connectors const &connectors)
	{
		unsigned count = 0;
		connectors.for_each([&] (Connector const &) { count++; });
		return count;
	}

	unsigned num_merged() const { return _count(_merged);   }

	void for_each(auto const &fn) const
	{
		_merged  .for_each(fn);
		_discrete.for_each(fn);
	}

	void with_connector(Name const &conn_name, auto const &fn) const
	{
		for_each([&] (Connector const &connector) {
			if (connector.name == conn_name)
				fn(connector); });
	}

	void with_mode_attr(Name const &conn_name, Connector::Mode::Id const &id, auto const &fn) const
	{
		with_connector(conn_name, [&] (Connector const &connector) {
			connector._modes.for_each([&] (Connector::Mode const &mode) {
				if (mode.id == id)
					fn(mode.attr); }); });
	}
};

#endif /* _MODEL__FB_CONNECTORS_H_ */
