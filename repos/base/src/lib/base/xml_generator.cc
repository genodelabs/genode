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


Xml_generator::Node::Node(Xml_generator &xml, char const *name, _Fn const &func)
:
	_indent_level(xml._curr_indent),
	_parent_node(xml._curr_node),
	_parent_was_indented(_parent_node ? _parent_node->is_indented() : false),
	_parent_had_content (_parent_node ? _parent_node->has_content() : false),
	_out_buffer(_parent_node ? _parent_node->_content_buffer(true)
	                         : xml._out_buffer)
{
	_out_buffer.append('\t', _indent_level);
	_out_buffer.append("<");
	_out_buffer.append(name);
	_attr_offset = _out_buffer.used();

	xml._curr_node = this;
	xml._curr_indent++;

	try {
		/*
		 * Process attributes and sub nodes
		 */
		func.call();
	} catch (...) {
		/* reset and drop changes by not committing it */
		xml._curr_node = _parent_node;
		xml._curr_indent--;
		if (_parent_node) {
			_parent_node->_undo_content_buffer(true, _parent_was_indented, _parent_had_content); }
		throw;
	}

	xml._curr_node = _parent_node;
	xml._curr_indent--;

	if (_is_indented) {
		_out_buffer.append("\n");
		_out_buffer.append('\t', _indent_level);
	}

	if (_has_content) {
		_out_buffer.append("</");
		_out_buffer.append(name);
		_out_buffer.append(">");
	} else
		_out_buffer.append("/>");

	if (_parent_node)
		_parent_node->_commit_content(_out_buffer);
	else
		xml._out_buffer = _out_buffer;

	_out_buffer.append('\0');
}
