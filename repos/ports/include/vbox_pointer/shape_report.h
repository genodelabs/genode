/*
 * \brief  shape report
 * \author Christian Prochaska
 * \date   2015-03-20
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VBOX_POINTER__SHAPE_REPORT_H_
#define _INCLUDE__VBOX_POINTER__SHAPE_REPORT_H_

namespace Vbox_pointer {

	enum {
		MAX_WIDTH = 100,
		MAX_HEIGHT = 100,
		MAX_SHAPE_SIZE = MAX_WIDTH*MAX_HEIGHT*4
	};

	struct Shape_report;
}

struct Vbox_pointer::Shape_report
{
	bool          visible;
	unsigned int  x_hot;
	unsigned int  y_hot;
	unsigned int  width;
	unsigned int  height;
	unsigned char shape[MAX_SHAPE_SIZE];
};

#endif /* _INCLUDE__VBOX_POINTER__SHAPE_REPORT_H_ */
