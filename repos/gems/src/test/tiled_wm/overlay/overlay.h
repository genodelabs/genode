/*
 * \brief  Tiled-WM test: overlay widget
 * \author Christian Helmuth
 * \date   2018-09-28
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TEST__TILED_WM__OVERLAY__OVERLAY_H_
#define _TEST__TILED_WM__OVERLAY__OVERLAY_H_

/* Qt includes */
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>
#include <QLineEdit>

/* Qoost includes */
#include <qoost/compound_widget.h>
#include <qoost/qmember.h>

/* local includes */
#include <util.h>


class Overlay : public Compound_widget<QWidget, QHBoxLayout>
{
	Q_OBJECT

	private:

		QMember<QLabel>    _label;
		QMember<QLineEdit> _entry;

	public:

		Overlay();
		~Overlay();
};

#endif /* _TEST__TILED_WM__OVERLAY__OVERLAY_H_ */
