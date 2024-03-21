/*
 * \brief  Tiled-WM test: panel widget
 * \author Christian Helmuth
 * \date   2018-09-28
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Qt includes */
#include <QApplication>
#include <QDesktopWidget>

/* local includes */
#include "panel.h"


void Panel_button::_clicked() { Q_EMIT clicked(text()); if (text() == "Panel") showMenu(); }
void Panel_button::_toggled(bool checked) { Q_EMIT toggled(checked, text()); }


Panel_button::Panel_button(QString label)
{
	if (!label.isNull()) {
		setText(label);
	}

	setCheckable(true);
	setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

	_layout->addWidget(_icon, 0, Qt::AlignCenter);

	if (label == "Panel") {
		_menu->addAction("Action");
		setMenu(_menu);
	}

	connect(this, SIGNAL(clicked()), SLOT(_clicked()));
	connect(this, SIGNAL(toggled(bool)), SLOT(_toggled(bool)));
}


Panel_button::~Panel_button() { }


void App_bar::_app_button(QAbstractButton *b, bool checked)
{
	if (!checked) return;

	if (Panel_button *button = qobject_cast<Panel_button *>(b)) {
		Name name { button->text().toUtf8().constData() };

		Genode::Reporter::Xml_generator xml(_content_request, [&] () {
			xml.attribute("name", name);
		});
	}
}


void App_bar::_handle_apps()
{
	/* empty bar before adding current apps */
	while (QLayoutItem *item = _layout->takeAt(0)) {
		if (Panel_button *button = qobject_cast<Panel_button *>(item->widget())) {
			_button_group->removeButton(button);
			button->deleteLater();
		}
		delete item;
	}

	_apps.update();

	Panel_button *visible_app_button = nullptr;

	_apps.xml().for_each_sub_node("app", [&] (Genode::Xml_node node) {
		QString const name    { node.attribute_value("name", Name("no name")).string() };
		bool    const visible { node.attribute_value("visible", false) };
		Panel_button *button  { new Panel_button(name) };

		if (visible) visible_app_button = button;

		_button_group->addButton(button);
		_layout->addWidget(button);
	});

	if (visible_app_button) visible_app_button->setChecked(true);
}


App_bar::App_bar(Genode::Env &env, Genode::Entrypoint &sig_ep)
:
	_apps(env, "apps"), _content_request(env, "content_request"),
	_apps_proxy(sig_ep)
{
	_content_request.enabled(true);

	_button_group->setExclusive(true);

	_handle_apps();

	_apps.sigh(*_apps_proxy);

	connect(_apps_proxy, SIGNAL(signal()), SLOT(_handle_apps()));
	connect(_button_group, SIGNAL(buttonToggled(QAbstractButton *, bool)),
	                       SLOT(_app_button(QAbstractButton *, bool)));
}


App_bar::~App_bar() { }


void Panel::_wifi_toggled(bool checked)
{
	Genode::Reporter::Xml_generator xml(_overlay_request, [&] () {
		xml.attribute("visible", checked);
	});
}


Panel::Panel(Genode::Env &env, Genode::Entrypoint &sig_ep)
:
	_overlay(env, "overlay"), _overlay_request(env, "overlay_request"),
	_panel_button("Panel"), _app_bar(env, sig_ep)
{
	_layout->addWidget(_panel_button);
	_layout->addWidget(new Spacer(), 1);
	_layout->addWidget(_app_bar);
	_layout->addWidget(new Spacer(), 1);
	_layout->addWidget(_wifi_button);

	_panel_button->setCheckable(false);
	_panel_button->setToolTip("This panel is just an example.");

	_wifi_button->setObjectName("wifi");
	_wifi_button->setToolTip("Open WiFi overlay");

	_overlay_request.enabled(true);

	_wifi_toggled(_wifi_button->isChecked());

	connect(_wifi_button, SIGNAL(toggled(bool)), SLOT(_wifi_toggled(bool)));
}


Panel::~Panel() { }
