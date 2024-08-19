/*
 * \brief   Main window of the mixer Qt frontend
 * \author  Josef Soentgen
 * \date    2015-10-15
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MAIN_WINDOW_H_
#define _MAIN_WINDOW_H_

/* Genode includes */
#include <base/env.h>
#include <util/xml_node.h>

/* Qt includes */
#include <QEvent>
#include <QHBoxLayout>
#include <QWidget>

/* Qoost includes */
#include <qoost/compound_widget.h>
#include <qoost/qmember.h>

/* application includes */


/**
 * This class proxies Genode signals to Qt signals
 */
struct Report_proxy : QObject
{
	Q_OBJECT

	Q_SIGNALS:

		void report_changed(void *, void const *);
};


class Main_window : public Compound_widget<QWidget, QHBoxLayout>
{
	Q_OBJECT

	private:

		int  _default_out_volume;
		int  _default_volume;
		bool _default_muted;

		bool _verbose;

		void _update_clients(Genode::Xml_node &);

	private Q_SLOTS:

		void _update_config();

	public Q_SLOTS:

		void report_changed(void *, void const *);

	public:

		Main_window(Genode::Env &);

		~Main_window();
};

#endif /* _MAIN_WINDOW_H_ */
