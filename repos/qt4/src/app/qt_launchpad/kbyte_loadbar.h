/*
 * \brief   KByte loadbar interface
 * \author  Christian Prochaska
 * \date    2008-04-05
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef KBYTE_LOADBAR_H
#define KBYTE_LOADBAR_H

#include <QtGui/QProgressBar>

class Kbyte_loadbar : public QProgressBar
{
    Q_OBJECT

public:
    Kbyte_loadbar(QWidget *parent = 0);
    ~Kbyte_loadbar();

    virtual QString text() const;
    
protected:

};

#endif // KBYTE_LOADBAR_H
