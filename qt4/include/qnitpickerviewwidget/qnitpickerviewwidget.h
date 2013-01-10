/*
 * \brief  A Qt Widget that shows a nitpicker view
 * \author Christian Prochaska
 * \date   2010-08-26
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef QNITPICKERVIEWWIDGET_H
#define QNITPICKERVIEWWIDGET_H

#include <QtGui>
#include <qwindowsystem_qws.h>

#include <nitpicker_view/capability.h>
#include <nitpicker_view/client.h>

class QNitpickerViewWidget : public QWidget
{
    Q_OBJECT

private:
    QHash<QScrollBar*, bool> _scrollbars;

private slots:
    void windowEvent(QWSWindow *window,
                     QWSServer::WindowEvent eventType);
    void valueChanged();
    void destroyed(QObject *obj = 0);

protected:
    Nitpicker::View_client *vc;
    int orig_w;
    int orig_h;
    int orig_buf_x;
    int orig_buf_y;

    virtual void showEvent(QShowEvent *event);
    virtual void hideEvent(QHideEvent *event);
    virtual void paintEvent(QPaintEvent *event);

public:
    QNitpickerViewWidget(QWidget *parent =0);
    ~QNitpickerViewWidget();
    void setNitpickerView(Nitpicker::View_capability view, int buf_x, int buf_y, int w, int h);
};

#endif // QNITPICKERVIEWWIDGET_H
