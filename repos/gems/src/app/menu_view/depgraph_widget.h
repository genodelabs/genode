/*
 * \brief  Widget that organizes child widgets as a directed graph
 * \author Norman Feske
 * \date   2017-08-09
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DEPGRAPH_WIDGET_H_
#define _DEPGRAPH_WIDGET_H_

/* Genode includes */
#include <base/registry.h>

/* gems includes */
#include <polygon_gfx/line_painter.h>

/* local includes */
#include "widget.h"

namespace Menu_view { struct Depgraph_widget; }


struct Menu_view::Depgraph_widget : Widget
{
	struct Depth_direction
	{
		enum Value { EAST, WEST, NORTH, SOUTH };
		Value value;
		bool horizontal() const { return value == EAST || value == WEST; }

	} _depth_direction { Depth_direction::EAST };

	struct Node
	{
		Allocator &_alloc;
		Widget    &_widget;
		Animator  &_animator;

		struct Anchor
		{
			Node &_remote;

			/**
			 * Dependency type
			 *
			 * Primary dependency nodes define the topology of the tree
			 * whereas secondary dependencies are weaker links that are
			 * displayed but ignored for the topology.
			 */
			enum Type { PRIMARY, SECONDARY } const _type;

			Anchor(Node &remote, Type type) : _remote(remote), _type(type) { }

			virtual ~Anchor() { }

			bool primary() const { return _type == PRIMARY; }

			/**
			 * Return breadth position of the anchored component
			 *
			 * The returned value is used to compute the order of anchors
			 * along the node edges.
			 */
			int remote_centered_breadth_pos(Depth_direction dir) const
			{
				return _remote.centered_breadth_pos(dir);
			}
		};

		Registry<Registered<Anchor> > _server_anchors { };
		Registry<Registered<Anchor> > _client_anchors { };

		Rect _widget_geometry { Point(0, 0), Area(0, 0) };

		/**
		 * Set cached widget geometry, calculated during 'update'
		 */
		void widget_geometry(Rect geometry) { _widget_geometry = geometry; }

		/**
		 * Propagate cached geometry to widget, called during '_layout'
		 *
		 * Calling this method may trigger the widget's geometry animation.
		 */
		void apply_layout_to_widget()
		{
			_widget.position(_widget_geometry.p1());
			_widget.size(_widget_geometry.area());
		}

		struct Dependency : Animator::Item
		{
			Anchor::Type const _type;

			enum Visible { VISIBLE, HIDDEN } _visible;

			/*
			 * Dependencies are marked as out of date at the beginning of the
			 * update procedure. Each dependency visited during the update is
			 * marked as up-to-date. The remaining out-of-date dependencies
			 * are stale and must be destroyed.
			 */
			bool up_to_date = true;

			int _dst_alpha() const { return _visible == VISIBLE ? 255 << 8 : 0; }

			/*
			 * Alpha value used for drawing, animated when 'visible' is
			 * toggled.
			 */
			Lazy_value<int> _alpha { 0 };

			Node &_server;

			/*
			 * Connection points at both ends of the dependency
			 */
			Registered<Anchor> _anchor_at_server;
			Registered<Anchor> _anchor_at_client;

			Dependency(Node &client, Node &server, Anchor::Type type,
			           Visible visible, Animator &animator)
			:
				Animator::Item(animator),
				_type(type), _visible(visible), _server(server),
				_anchor_at_server(server._server_anchors, client, type),
				_anchor_at_client(client._client_anchors, server, type)
			{
				/* trigger fade-in if initially visible */
				if (_dst_alpha()) {
					_alpha.dst(_dst_alpha(), Widget::motion_steps().value);
					animate();
				}
			}

			virtual ~Dependency() { }

			bool depends_on(Node const &n) const { return &_server == &n; }

			unsigned server_depth_pos(Depth_direction dir) const
			{
				return _server.depth_pos(dir) + _server.depth_size(dir);
			}

			unsigned server_breadth_pos(Depth_direction dir) const
			{
				return _server.breadth_pos(dir);
			}

			unsigned server_breadth_alignment(Depth_direction dir) const
			{
				unsigned const children_size = _server.layout_breadth_child_offset;
				unsigned const total_size    = _server.breadth_size(dir);

				return children_size < total_size ? (total_size - children_size)/2 : 0;
			}

			bool primary() const { return _type == Anchor::PRIMARY; }

			template <typename FN>
			void apply_to_server(FN const &fn) { fn(_server); }

			template <typename FN>
			void apply_to_server(FN const &fn) const { fn(_server); }

			void visible(Visible const visible)
			{
				if (visible == _visible)
					return;

				_visible = visible;

				_alpha.dst(_dst_alpha(), Widget::motion_steps().value);
				animate();
			}

			int alpha() const { return _alpha >> 8; }


			/******************************
			 ** Animator::Item interface **
			 ******************************/

			void animate() override
			{
				_alpha.animate();

				animated(_alpha != _alpha.dst());
			}
		};

		Registry<Registered<Dependency> > _deps { };

		void cut_dependencies()
		{
			_deps.for_each([&] (Dependency &dep) {
				destroy(_alloc, &dep); });
		}

		/*
		 * Helper variable for calculating the layout of dependent (child) nodes
		 *
		 * This variable tracks the offset of the last visited child node.
		 * During the layouting procedure, it gets successively increased.
		 */
		unsigned layout_breadth_child_offset = 0;

		/*
		 * Breadth position relative to the node's primary dependency node.
		 */
		unsigned layout_breadth_offset = 0;

		Node(Allocator &alloc, Widget &widget, Animator &animator)
		: _alloc(alloc), _widget(widget), _animator(animator) { }

		template <typename FN>
		void for_each_dependent_node(FN const &fn)
		{
			_server_anchors.for_each([&] (Anchor &anchor) { fn(anchor._remote); });
		}

		virtual ~Node() { cut_dependencies(); }

		bool has_deps() const
		{
			bool result = false;
			_deps.for_each([&] (Dependency const &) { result = true; });
			return result;
		}

		bool belongs_to(Widget const &w) { return &_widget == &w; }

		bool has_name(Name const &name) const { return _widget.has_name(name); }

		unsigned depth_size(Depth_direction dir) const
		{
			return dir.horizontal() ? _widget.min_size().w() : _widget.min_size().h();
		}

		/**
		 * Accumulate breadth of all clients of the node
		 */
		unsigned _breadth_clients_size(Depth_direction dir) const
		{
			unsigned sum_clients_size = 0;

			_server_anchors.for_each([&] (Anchor const &anchor) {
				if (anchor.primary())
					sum_clients_size += anchor._remote.breadth_size(dir); });

			return sum_clients_size;
		}

		/**
		 * Return breadth of the node, including the widget and all children
		 */
		unsigned breadth_size(Depth_direction dir) const
		{
			unsigned const widget_size =
				dir.horizontal() ? _widget.min_size().h() : _widget.min_size().w();

			unsigned const breadth_padding = 10;

			return max(widget_size + breadth_padding, _breadth_clients_size(dir));
		}

		/**
		 * Return 'depth' position of node within the dependency tree
		 */
		unsigned depth_pos(Depth_direction dir) const
		{
			/* maximum depth position of all nodes we depend on */
			unsigned max_deps_depth = 0;
			_deps.for_each([&] (Dependency const &dep) {
				max_deps_depth = max(max_deps_depth, dep.server_depth_pos(dir)); });

			unsigned depth_padding = 10;
			return max_deps_depth + depth_padding;
		}

		/**
		 * Return breadth position of our primary dependency (parent) node
		 * within the dependency tree
		 */
		unsigned _primary_dep_breadth_pos(Depth_direction dir) const
		{
			unsigned result = 0;
			_deps.for_each([&] (Registered<Dependency> const &dep) {
				if (dep.primary()) {
					result = dep.server_breadth_pos(dir)
					       + dep.server_breadth_alignment(dir); } });
			return result;
		}

		/**
		 * Return absolute 'breadth' position of node within the dependency tree
		 *
		 * This method relies on the prior computed 'layout_breadth_offset'
		 * values (of this and its chain of primary dependency nodes).
		 */
		unsigned breadth_pos(Depth_direction dir) const
		{
			return _primary_dep_breadth_pos(dir) + layout_breadth_offset;
		}

		void mark_deps_as_out_of_date()
		{
			_deps.for_each([&] (Dependency &dep) { dep.up_to_date = false; });
		}

		void depends_on(Node &node, Anchor::Type type, Dependency::Visible visible)
		{
			bool dependency_exists = false;
			_deps.for_each([&] (Dependency &dep) {
				if (dep.depends_on(node)) {
					dep.visible(visible);
					dep.up_to_date = true; /* skip in 'destroy_stale_deps' */
					dependency_exists = true;
				}
			});

			if (!dependency_exists)
				new (_alloc)
					Registered<Dependency>(_deps, *this, node, type, visible,
					                       _animator);
		}

		void destroy_stale_deps()
		{
			_deps.for_each([&] (Registered<Dependency> &dep) {
				if (!dep.up_to_date)
					destroy(_alloc, &dep); });
		}

		template <typename FN>
		bool apply_to_primary_dependency(FN &fn)
		{
			bool result = false;
			_deps.for_each([&] (Registered<Dependency> &dep) {
				if (dep.primary()) {
					dep.apply_to_server(fn);
					result = true;
				}
			});
			return result;
		}

		int centered_breadth_pos(Depth_direction dir) const
		{
			return dir.horizontal() ? (_widget.geometry().y1() + _widget.geometry().y2()) / 2
			                        : (_widget.geometry().x1() + _widget.geometry().x2()) / 2;
		}

		unsigned _edge_size(Depth_direction dir) const
		{
			if (dir.horizontal())
				return max(0, (int)_widget.geometry().h() - (int)_widget.margin.top
				                                          - (int)_widget.margin.bottom);
			else
				return max(0, (int)_widget.geometry().w() - (int)_widget.margin.left
				                                          - (int)_widget.margin.right);
		}

		/**
		 * Return position of connection point at node edge
		 */
		unsigned _edge_pos(Registry<Registered<Anchor> > const &anchors,
		                   Node const &client, Depth_direction dir) const
		{
			int const client_pos = client.centered_breadth_pos(dir);

			/*
			 * Count number of anchors lower than the client-node position and
			 * the total number of clients. The anchor points are positioned
			 * along the widget edge in the order of the client positions to
			 * avoid intersecting dependency lines.
			 */
			int lower_cnt = 0, total_cnt = 0;
			anchors.for_each([&] (Anchor const &anchor) {
				total_cnt++;
				if (anchor.remote_centered_breadth_pos(dir) < client_pos)
					lower_cnt++;
			});

			return ((lower_cnt + 1)*_edge_size(dir)) / (total_cnt + 1);
		}

		Point server_anchor_point(Node const &client, Depth_direction dir) const
		{
			int  const pos   = _edge_pos(_server_anchors, client, dir);
			Rect const edges = _widget.edges();

			switch (dir.value) {
			case Depth_direction::EAST:  return Point(edges.x2(), edges.y1() + pos);
			case Depth_direction::WEST:  return Point(edges.x1(), edges.y1() + pos);
			case Depth_direction::NORTH: return Point(edges.x1() + pos, edges.y1());
			case Depth_direction::SOUTH: return Point(edges.x1() + pos, edges.y2());
			}
			return Point(0, 0);
		}

		Point client_anchor_point(Node const &client, Depth_direction dir) const
		{
			int  const pos   = _edge_pos(_client_anchors, client, dir);
			Rect const edges = _widget.edges();

			switch (dir.value) {
			case Depth_direction::EAST:  return Point(edges.x1(), edges.y1() + pos);
			case Depth_direction::WEST:  return Point(edges.x2(), edges.y1() + pos);
			case Depth_direction::NORTH: return Point(edges.x1() + pos, edges.y2());
			case Depth_direction::SOUTH: return Point(edges.x1() + pos, edges.y1());
			}
			return Point(0, 0);
		}
	};

	typedef Registered<Node>          Registered_node;
	typedef Registry<Registered_node> Node_registry;

	Node_registry _nodes { };

	Registered_node _root_node { _nodes, _factory.alloc, *this, _factory.animator };

	/*
	 * Defined by 'update', used by '_layout'
	 */
	Rect _bounding_box { Point(0, 0), Area(0, 0) };

	template <typename FN>
	void apply_to_primary_dependency(Node &node, FN const &fn)
	{
		if (node.apply_to_primary_dependency(fn))
			return;

		/* node has no primary dependency defined, use root node */
		fn(_root_node);
	}

	Depgraph_widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
	:
		Widget(factory, node, unique_id)
	{ }

	~Depgraph_widget() { _update_children(Xml_node("<empty/>")); }

	void update(Xml_node node) override
	{
		/* update depth direction */
		{
			typedef String<10> Dir_name;
			Dir_name dir_name = node.attribute_value("direction", Dir_name());
			_depth_direction = { Depth_direction::EAST };
			if (dir_name == "north") _depth_direction = { Depth_direction::NORTH };
			if (dir_name == "south") _depth_direction = { Depth_direction::SOUTH };
			if (dir_name == "east")  _depth_direction = { Depth_direction::EAST  };
			if (dir_name == "west")  _depth_direction = { Depth_direction::WEST  };
		}

		_update_children(node);
	}

	void _update_children(Xml_node const &node)
	{
		Allocator &alloc = _factory.alloc;

		_children.update_from_xml(node,

			/* create */
			[&] (Xml_node const &node) -> Widget &
			{
				Widget &w = _factory.create(node);
				new (alloc) Registered_node(_nodes, alloc, w, _factory.animator);
				return w;
			},

			/* destroy */
			[&] (Widget &w)
			{
				auto destroy_node = [&] (Registered_node &node)
				{
					/*
					 * If a server node vanishes, disconnect all client nodes. The
					 * nodes will be reconnected - if possible - after the model
					 * update.
					 */
					node.for_each_dependent_node([&] (Node &dependent) {
						dependent.cut_dependencies(); });

					Widget &w = node._widget;
					destroy(alloc, &node);
					_factory.destroy(&w);
				};

				_nodes.for_each([&] (Registered_node &node) {
					if (node.belongs_to(w))
						destroy_node(node); });
			},

			/* update */
			[&] (Widget &w, Xml_node const &node) { w.update(node); }
		);

		/*
		 * Import dependencies
		 */
		_nodes.for_each([&] (Node &node) {
			node.mark_deps_as_out_of_date(); });

		node.for_each_sub_node([&] (Xml_node node) {

			bool const primary = !node.has_type("dep");

			typedef String<64> Node_name;
			Node_name client_name, server_name;
			bool dep_visible = true;
			if (primary) {
				client_name = node.attribute_value("name", Node_name());
				server_name = node.attribute_value("dep",  Node_name());
				dep_visible = node.attribute_value("dep_visible", true);
			} else {
				client_name = node.attribute_value("node", Node_name());
				server_name = node.attribute_value("on",   Node_name());
				dep_visible = node.attribute_value("visible", true);
			}

			if (!server_name.valid())
				return;

			Node *client = nullptr, *server = nullptr;
			_nodes.for_each([&] (Node &node) {
				if (node.has_name(client_name)) client = &node;
				if (node.has_name(server_name)) server = &node;
			});

			if (client && server && client != server)
				client->depends_on(*server,
				                   primary ? Node::Anchor::PRIMARY
				                           : Node::Anchor::SECONDARY,
				                   dep_visible ? Node::Dependency::VISIBLE
				                               : Node::Dependency::HIDDEN);
			if (client && !server) {
				warning("node '", client_name, "' depends on "
				        "non-existing node '", server_name, "'");
				client->_widget.position(Point(0, 0));
				client->_widget.size(Area(0, 0));
			}
		});

		_nodes.for_each([&] (Node &node) {
			node.destroy_stale_deps(); });

		_nodes.for_each([&] (Node &node) {
			node.layout_breadth_child_offset = 0; });

		/*
		 * Compute 'layout_breadth_offset' values of all nodes
		 *
		 * The computation depends on the order of '_children'.
		 */
		_children.for_each([&] (Widget &w) {
			_nodes.for_each([&] (Registered_node &node) {
				if (!node.belongs_to(w))
					return;

				apply_to_primary_dependency(node, [&] (Node &parent) {

					node.layout_breadth_offset = parent.layout_breadth_child_offset;

					/* advance breadth offset at parent by size of current node */
					parent.layout_breadth_child_offset +=
						node.breadth_size(_depth_direction);
				});
			});
		});

		/*
		 * Calculate the bounding box and the designated geometries of all
		 * widgets.
		 *
		 * The bounding box dictates the 'min_size' of the depgraph widget.
		 *
		 * The computed widget geometries are stored in their corresponding
		 * 'Node' objects but are not immediately propagated to the widgets.
		 * The computed geometries are applied to the widgets in '_layout'
		 * phase.
		 */
		_bounding_box = Rect(Point(0, 0), Area(0, 0));
		_children.for_each([&] (Widget &w) {
			_nodes.for_each([&] (Registered_node &node) {
				if (!node.belongs_to(w))
					return;

				int const depth_pos    = node.depth_pos(_depth_direction),
				          breadth_pos  = node.breadth_pos(_depth_direction),
				          depth_size   = node.depth_size(_depth_direction),
				          breadth_size = node.breadth_size(_depth_direction);

				Rect const node_rect = _depth_direction.horizontal()
				                     ? Rect(Point(depth_pos, breadth_pos),
				                            Area(depth_size, breadth_size))
				                     : Rect(Point(breadth_pos, depth_pos),
				                            Area(breadth_size, depth_size));

				Rect geometry(node_rect.center(w.min_size()), w.min_size());

				node.widget_geometry(geometry);

				_bounding_box = Rect::compound(_bounding_box, geometry);
			});
		});
	}

	Area min_size() const override { return _bounding_box.area(); }

	void _draw_connect(Surface<Pixel_rgb888> &pixel_surface,
	                   Surface<Pixel_alpha8> &alpha_surface,
	                   Point p1, Point p2, Color color, bool horizontal) const
	{
		Line_painter line_painter;

		auto draw_segment = [&] (long x1, long y1, long x2, long y2)
		{
			auto const fx1 = Line_painter::Fixpoint::from_raw(x1),
			           fy1 = Line_painter::Fixpoint::from_raw(y1),
			           fx2 = Line_painter::Fixpoint::from_raw(x2),
			           fy2 = Line_painter::Fixpoint::from_raw(y2);

			line_painter.paint(pixel_surface, fx1, fy1, fx2, fy2, color);
			line_painter.paint(alpha_surface, fx1, fy1, fx2, fy2, color);
		};

		long const mid_x = (p1.x() + p2.x()) / 2,
		           mid_y = (p1.y() + p2.y()) / 2;

		long const x1 = p1.x(),
		           y1 = p1.y(),
		           x2 = horizontal ? mid_x  : p1.x(),
		           y2 = horizontal ? p1.y() : mid_y,
		           x3 = horizontal ? mid_x  : p2.x(),
		           y3 = horizontal ? p2.y() : mid_y,
		           x4 = p2.x(),
		           y4 = p2.y();

		auto abs = [] (auto v) { return v >= 0 ? v : -v; };

		/* subdivide the curve depending on the size of its bounding box */
		unsigned const levels = (unsigned)max(log2(max(abs(x4 - x1), abs(y4 - y1)) >> 2), 3);

		bezier(x1 << 8, y1 << 8, x2 << 8, y2 << 8,
		       x3 << 8, y3 << 8, x4 << 8, y4 << 8, draw_segment, levels);
	}

	void _draw_connections(Surface<Pixel_rgb888> &pixel_surface,
	                       Surface<Pixel_alpha8> &alpha_surface,
	                       Point at, bool shadow) const
	{
		_nodes.for_each([&] (Node const &client) {

			client._deps.for_each([&] (Node::Dependency const &dep) {

				int const alpha = dep.alpha();
				if (!alpha)
					return;

				Color color;

				if (shadow) {
					color = dep.primary() ? Color(0, 0, 0, (150*alpha)>>8)
					                      : Color(0, 0, 0,  (50*alpha)>>8);
				} else {
					color = dep.primary() ? Color(255, 255, 255, (190*alpha)>>8)
					                      : Color(255, 255, 255, (120*alpha)>>8);
				}

				dep.apply_to_server([&] (Node const &server) {

					Point from = server.server_anchor_point(client, _depth_direction);
					Point to   = client.client_anchor_point(server, _depth_direction);

					_draw_connect(pixel_surface, alpha_surface,
					              at + from, at + to, color, _depth_direction.horizontal());
				});
			});
		});
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const override
	{
		/* draw connections twice, for the shadow and actual color */
		_draw_connections(pixel_surface, alpha_surface, at + Point(0, 1), true);
		_draw_connections(pixel_surface, alpha_surface, at, false);

		_draw_children(pixel_surface, alpha_surface, at);
	}

	void _layout() override
	{
		/*
		 * Apply layout to the children
		 */
		_nodes.for_each([&] (Registered_node &node) {
			if (&node != &_root_node)
				node.apply_layout_to_widget(); });

		/*
		 * Mirror coordinates if graph grows towards north or west
		 */
		if (_depth_direction.value == Depth_direction::NORTH
		 || _depth_direction.value == Depth_direction::WEST) {

			_children.for_each([&] (Widget &w) {

				int x = w.geometry().x1(), y = w.geometry().y1();

				if (_depth_direction.value == Depth_direction::NORTH)
					y = (int)_bounding_box.h() - y - w.geometry().h();

				if (_depth_direction.value == Depth_direction::WEST)
					x = (int)_bounding_box.w() - x - w.geometry().w();

				w.position(Point(x, y));
			});
		}

		/*
		 * Prompt each child to update its layout
		 */
		_children.for_each([&] (Widget &w) {
			w.size(w.geometry().area()); });
	}
};

#endif /* _DEPGRAPH_WIDGET_H_ */
