/*
 * \brief   KByte loadbar implementation
 * \author  Christian Prochaska
 * \date    2008-04-05
 */

#include "kbyte_loadbar.h"


Kbyte_loadbar::Kbyte_loadbar(QWidget *parent)
: QProgressBar(parent) { }


QString Kbyte_loadbar::text() const
{
	return QString::number(value()) + " KByte / " + 
	                       QString::number(maximum()) + " KByte";
}
