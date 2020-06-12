/*
 * \brief  QGenodeClipboard
 * \author Christian Prochaska
 * \date   2015-09-18
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "qgenodeclipboard.h"

#ifndef QT_NO_CLIPBOARD

/* Genode includes */
#include <util/xml_node.h>

/* Qt includes */
#include <QMimeData>

QT_BEGIN_NAMESPACE


static constexpr bool verbose = false;


QGenodeClipboard::QGenodeClipboard(Genode::Env &env)
: _clipboard_signal_handler(env.ep(), *this, &QGenodeClipboard::_clipboard_changed)
{
	try {

		Genode::Attached_rom_dataspace config(env, "config");

		if (config.xml().attribute_value("clipboard", false)) {

			try {

				_clipboard_ds = new Genode::Attached_rom_dataspace(env, "clipboard");

				_clipboard_ds->sigh(_clipboard_signal_handler);
				_clipboard_ds->update();

			} catch (...) { }

			try {
				_clipboard_reporter = new Genode::Reporter(env, "clipboard");
				_clipboard_reporter->enabled(true);
			} catch (...) {	}

		}
	} catch (...) { }

	connect(this, SIGNAL(_clipboard_changed()),
	        this, SLOT(_handle_clipboard()),
	        Qt::QueuedConnection);
}


QGenodeClipboard::~QGenodeClipboard()
{
	free(_decoded_clipboard_content);
	delete _clipboard_ds;
	delete _clipboard_reporter;
}


void QGenodeClipboard::_handle_clipboard()
{
	emitChanged(QClipboard::Clipboard);
}


QMimeData *QGenodeClipboard::mimeData(QClipboard::Mode mode)
{
	if (!_clipboard_ds)
		return 0;

	_clipboard_ds->update();

	if (!_clipboard_ds->valid()) {
		if (verbose)
			Genode::error("invalid clipboard dataspace");
		return 0;
	}

	char *xml_data = _clipboard_ds->local_addr<char>();

	try {
		Genode::Xml_node node(xml_data);

		if (!node.has_type("clipboard")) {
			Genode::error("invalid clipboard xml syntax");
			return 0;
		}

		free(_decoded_clipboard_content);

		_decoded_clipboard_content = (char*)malloc(node.content_size());

		if (!_decoded_clipboard_content) {
			Genode::error("could not allocate buffer for decoded clipboard content");
			return 0;
		}

		_mimedata->setText(QString::fromUtf8(_decoded_clipboard_content,
		                                     node.decoded_content(_decoded_clipboard_content,
		                                                          node.content_size())));

	} catch (Genode::Xml_node::Invalid_syntax) {
		Genode::error("invalid clipboard xml syntax");
		return 0;
	}

	return _mimedata;
}


void QGenodeClipboard::setMimeData(QMimeData *data, QClipboard::Mode mode)
{
	if (!data || !data->hasText() || !supportsMode(mode))
		return;

	QString text = data->text();
	QByteArray utf8text = text.toUtf8();

	if (!_clipboard_reporter)
		return;

	try {
		Genode::Reporter::Xml_generator xml(*_clipboard_reporter, [&] () {
			xml.append_sanitized(utf8text.constData(), utf8text.size()); });
	} catch (...) {
		Genode::error("could not write clipboard data");
	}
}

QT_END_NAMESPACE

#endif /* QT_NO_CLIPBOARD */
