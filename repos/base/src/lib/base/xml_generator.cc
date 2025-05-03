/*
 * \brief  XML generator
 * \author Norman Feske
 * \date   2021-03-11
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/xml_generator.h>

using namespace Genode;


void Xml_generator::Node::_on_exception(Xml_generator &xml)
{
	/* reset and drop changes by not committing it */
	xml._curr_node = _parent_node;
	xml._curr_indent--;
	if (_parent_node)
		_parent_node->_undo_content_buffer(true, _parent_was_indented, _parent_had_content);
}


Xml_generator::Node::Node(Xml_generator &xml, char const *name, bool,
                          Callable<void>::Ft const &fn)
:
	_indent_level(xml._curr_indent),
	_parent_node(xml._curr_node),
	_parent_was_indented(_parent_node ? _parent_node->is_indented() : false),
	_parent_had_content (_parent_node ? _parent_node->has_content() : false),
	_out_buffer(_parent_node ? _parent_node->_content_buffer(true)
	                         : xml._out_buffer)
{
	_exceeded |= _out_buffer.append('\t', _indent_level).exceeded
	          || _out_buffer.append("<").exceeded
	          || _out_buffer.append(name).exceeded;

	if (_exceeded)
		return;

	_attr_offset = _out_buffer.used();

	xml._curr_node = this;
	xml._curr_indent++;

	/*
	 * Handle exception thrown by fn()
	 */
	struct Guard
	{
		Xml_generator &xml;
		Node          &_this;
		bool ok = false;
		~Guard() { if (!ok) _this._on_exception(xml); }
	} guard { xml, *this };

	/*
	 * Process attributes and sub nodes
	 */
	fn();

	guard.ok = true;

	xml._curr_node = _parent_node;
	xml._curr_indent--;

	if (_is_indented)
		_exceeded |= _out_buffer.append("\n").exceeded
		          || _out_buffer.append('\t', _indent_level).exceeded;

	if (_has_content)
		_exceeded |= _out_buffer.append("</").exceeded
		          || _out_buffer.append(name).exceeded
		          || _out_buffer.append(">") .exceeded;
	else
		_exceeded |= _out_buffer.append("/>").exceeded;

	if (_parent_node)
		_exceeded |= _parent_node->_commit_content(_out_buffer).exceeded;
	else
		xml._out_buffer = _out_buffer;

	_exceeded |= _out_buffer.append('\0').exceeded;
}
