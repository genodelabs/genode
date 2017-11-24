/*
 * \brief  QNitpickerCursor
 * \author Christian Prochaska
 * \date   2017-11-13
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <base/log.h>

/* Qt includes */
#include <QtGui/QBitmap>
#include "qnitpickercursor.h"

QT_BEGIN_NAMESPACE

QNitpickerCursor::QNitpickerCursor(Genode::Env &env)
{
	try {
		_shape_report_connection.construct(env, "shape", sizeof(Pointer::Shape_report));
		_shape_report_ds.construct(env.rm(), _shape_report_connection->dataspace());
		_shape_report = _shape_report_ds->local_addr<Pointer::Shape_report>();
	} catch (Genode::Service_denied) { }
}

void QNitpickerCursor::changeCursor(QCursor *widgetCursor, QWindow *window)
{
	Q_UNUSED(window);

#ifndef QT_NO_CURSOR

	if (!_shape_report)
		return;

	const Qt::CursorShape shape = widgetCursor ?
			                      widgetCursor->shape() :
			                      Qt::ArrowCursor;

	_shape_report->visible = (shape != Qt::BlankCursor);

	QImage cursor;

	if (shape == Qt::BitmapCursor) {
		// application supplied cursor
		cursor = widgetCursor->pixmap().toImage();
		_shape_report->x_hot = widgetCursor->hotSpot().x();
		_shape_report->y_hot = widgetCursor->hotSpot().y();
	} else {
		// system cursor
		QPlatformCursorImage platformImage(0, 0, 0, 0, 0, 0);
		platformImage.set(shape);
		cursor = *platformImage.image();
		_shape_report->x_hot = platformImage.hotspot().x();
		_shape_report->y_hot = platformImage.hotspot().y();
	}

	cursor = cursor.convertToFormat(QImage::Format_RGBA8888);

	_shape_report->width = cursor.width();
	_shape_report->height = cursor.height();

	memcpy(_shape_report->shape, cursor.constBits(),
		   cursor.width() * cursor.height() * 4);

	_shape_report_connection->submit(sizeof(Pointer::Shape_report));

#else // !QT_NO_CURSOR
	Q_UNUSED(widgetCursor);
#endif
}

QT_END_NAMESPACE
