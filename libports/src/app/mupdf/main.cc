/*
 * \brief  MuPDF for Genode
 * \author Norman Feske
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <framebuffer_session/connection.h>
#include <base/sleep.h>

/* MuPDF includes */
extern "C" {
#include <fitz.h>
#include <mupdf.h>
#include <muxps.h>
#include <pdfapp.h>
}

/* libc includes */
#include <unistd.h>


/**************************
 ** Called from pdfapp.c **
 **************************/

void winrepaint(pdfapp_t *)
{
	PDBG("not implemented");
}


void winrepaintsearch(pdfapp_t *)
{
	PDBG("not implemented");
}


void wincursor(pdfapp_t *, int curs)
{
	PDBG("curs=%d - not implemented", curs);
}


void winerror(pdfapp_t *, fz_error error)
{
	PDBG("error=%d", error);
	Genode::sleep_forever();
}


void winwarn(pdfapp_t *, char *msg)
{
	PWRN("MuPDF: %s", msg);
}


void winhelp(pdfapp_t *)
{
	PDBG("not implemented");
}


char *winpassword(pdfapp_t *, char *)
{
	PDBG("not implemented");
	return NULL;
}


void winclose(pdfapp_t *app)
{
	PDBG("not implemented");
}


void winreloadfile(pdfapp_t *)
{
	PDBG("not implemented");
}


void wintitle(pdfapp_t *app, char *s)
{
	PDBG("s=\"%s\" - not implemented", s);
}


void winresize(pdfapp_t *app, int w, int h)
{
	PDBG("not implemented");
}


/******************
 ** Main program **
 ******************/

int main(int, char **)
{

	static Framebuffer::Connection framebuffer;
	Framebuffer::Session::Mode mode;
	int width, height;
	framebuffer.info(&width, &height, &mode);

	PDBG("Framebuffer is %dx%d\n", width, height);

	if (mode != Framebuffer::Session::RGB565) {
		PERR("Color modes other than RGB565 are not supported. Exiting.");
		return 1;
	}


	static pdfapp_t pdfapp;
	pdfapp_init(&pdfapp);

	pdfapp.scrw       = width;
	pdfapp.scrh       = height;
	pdfapp.resolution = 75; /* XXX read from config */
	pdfapp.pageno     =  1; /* XXX read from config */

	char const *file_name = "test.pdf"; /* XXX read from config */

	int fd = open(file_name, O_BINARY | O_RDONLY, 0666);
	if (fd < 0) {
		PERR("Could not open input file \"%s\", Exiting.", file_name);
		return 2;
	}
	pdfapp_open(&pdfapp, (char *)file_name, fd, 0);

	Genode::sleep_forever();
	return 0;
}
