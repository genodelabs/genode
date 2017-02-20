/*
 * \brief  Expat test
 * \author Christian Prochaska
 * \date   2012-06-12
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "expat.h"


static void start_element(void *userdata, const char *name, const char **attr)
{
	printf(" start of element: %s\n", name);

	for (int i = 0; attr[i]; i += 2)
		printf(" attribute: name='%s', value='%s'\n", attr[i], attr[i + 1]);
}


static void end_element(void *userdata, const char *name)
{
	printf(" end of element: %s\n", name);
}


int main(int argc, char *argv[])
{
	char buf[128];

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetElementHandler(parser, start_element, end_element);

	int config_fd = open("config", O_RDONLY);

	if (config_fd < 0) {
		printf(" Error: could not open config file\n");
		return -1;
	}

	read(config_fd, buf, sizeof(buf));

	if (XML_Parse(parser, buf, strlen(buf), 1) == XML_STATUS_ERROR) {
		printf(" Error: %s at line %lu\n",
				XML_ErrorString(XML_GetErrorCode(parser)),
				XML_GetCurrentLineNumber(parser));
		return -1;
	}

	XML_ParserFree(parser);

	return 0;
}
