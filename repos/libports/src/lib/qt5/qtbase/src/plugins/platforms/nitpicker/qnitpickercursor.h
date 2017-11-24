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


#ifndef _QNITPICKERCURSOR_H_
#define _QNITPICKERCURSOR_H_

/* Genode includes */
#include <base/attached_dataspace.h>
#include <report_session/connection.h>
#include <pointer/shape_report.h>

/* Qt includes */
#include <qpa/qplatformcursor.h>

QT_BEGIN_NAMESPACE

class QNitpickerCursor : public QPlatformCursor
{
	private:

		Genode::Constructible<Report::Connection>          _shape_report_connection;
		Genode::Constructible<Genode::Attached_dataspace>  _shape_report_ds;
		Pointer::Shape_report                             *_shape_report { nullptr };

	public:

		QNitpickerCursor(Genode::Env &env);

		virtual void changeCursor(QCursor *widgetCursor, QWindow *window) override;
};

QT_END_NAMESPACE

#endif /* _QNITPICKERCURSOR_H_ */
