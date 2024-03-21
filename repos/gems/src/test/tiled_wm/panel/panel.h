/*
 * \brief  Tiled-WM test: panel widget
 * \author Christian Helmuth
 * \date   2018-09-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TEST__TILED_WM__PANEL__PANEL_H_
#define _TEST__TILED_WM__PANEL__PANEL_H_

/* Qt includes */
#include <QHBoxLayout>
#include <QPushButton>
#include <QWidget>
#include <QButtonGroup>
#include <QMenu>

/* Qoost includes */
#include <qoost/compound_widget.h>
#include <qoost/qmember.h>

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

/* local includes */
#include <util.h>
#include <icon.h> /* build-dir copy of qoost/icon.h works around missing "vtable for Icon" */


class Panel_button : public Compound_widget<QPushButton, QHBoxLayout>
{
	Q_OBJECT

	private:

		QMember<Icon> _icon;
		QMember<QMenu> _menu;

	private Q_SLOTS:

		void _clicked();
		void _toggled(bool);

	public:

		Panel_button(QString label = QString());
		~Panel_button();

	Q_SIGNALS:

		void clicked(QString label);
		void toggled(bool checked, QString label);
};


class App_bar : public Compound_widget<QWidget, QHBoxLayout>
{
	Q_OBJECT

	private:

		Genode::Attached_rom_dataspace _apps;
		Genode::Reporter               _content_request;

		QMember<QButtonGroup>        _button_group;
		QMember<Genode_signal_proxy> _apps_proxy;

	private Q_SLOTS:

		void _handle_apps();
		void _app_button(QAbstractButton *, bool);

	public:

		App_bar(Genode::Env &, Genode::Entrypoint &);
		~App_bar();
};



class Panel : public Compound_widget<QWidget, QHBoxLayout>
{
	Q_OBJECT

	private:

		Genode::Attached_rom_dataspace _overlay;
		Genode::Reporter               _overlay_request;

		QMember<Panel_button> _panel_button;
		QMember<App_bar>      _app_bar;
		QMember<Panel_button> _wifi_button;

	private Q_SLOTS:

		void _wifi_toggled(bool);

	public:

		Panel(Genode::Env &, Genode::Entrypoint &);
		~Panel();
};

#endif /* _TEST__TILED_WM__PANEL__PANEL_H_ */
