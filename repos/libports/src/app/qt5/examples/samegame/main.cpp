/*
 * \brief   QtQuick 'samegame' example
 * \author  Christian Prochaska
 * \date    2013-11-26
 */

#include <QGuiApplication>
#include <QQuickView>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQuickView view;
    view.setSource(QUrl("/samegame.qml"));
    view.show();

    return app.exec();
}
