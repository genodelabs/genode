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

#ifndef _QGENODECLIPBOARD_H_
#define _QGENODECLIPBOARD_H_

#include <QtCore/qglobal.h>

#ifndef QT_NO_CLIPBOARD

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <qoost/qmember.h>

/* Qt includes */
#include <qpa/qplatformclipboard.h>

QT_BEGIN_NAMESPACE

class QGenodeClipboard : public QPlatformClipboard
{
	private:

		Genode::Attached_rom_dataspace              *_clipboard_ds = nullptr;
		Genode::Signal_dispatcher<QGenodeClipboard>  _clipboard_signal_dispatcher;

		Genode::Reporter *_clipboard_reporter = nullptr;

		char *_decoded_clipboard_content = nullptr;

		QMember<QMimeData> _mimedata;

		void _handle_clipboard(unsigned int);

	public:

		QGenodeClipboard(Genode::Env &env, Genode::Signal_receiver &sig_rcv);
		~QGenodeClipboard();
		QMimeData *mimeData(QClipboard::Mode mode = QClipboard::Clipboard);
		void setMimeData(QMimeData *data, QClipboard::Mode mode = QClipboard::Clipboard);
};

QT_END_NAMESPACE

#endif /* QT_NO_CLIPBOARD */
#endif /* _QGENODECLIPBOARD_H_ */
