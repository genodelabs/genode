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


/***************
 ** Dithering **
 ***************/

/*
 * XXX blatantly copied from 'demo/src/app/backdrop/main.cc'
 *
 * We should factor-out the dithering support into a separate header file.
 * But where is a good place to put it?
 */

enum { DITHER_SIZE = 16, DITHER_MASK = DITHER_SIZE - 1 };

static const int dither_matrix[DITHER_SIZE][DITHER_SIZE] = {
  {   0,192, 48,240, 12,204, 60,252,  3,195, 51,243, 15,207, 63,255 },
  { 128, 64,176,112,140, 76,188,124,131, 67,179,115,143, 79,191,127 },
  {  32,224, 16,208, 44,236, 28,220, 35,227, 19,211, 47,239, 31,223 },
  { 160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95 },
  {   8,200, 56,248,  4,196, 52,244, 11,203, 59,251,  7,199, 55,247 },
  { 136, 72,184,120,132, 68,180,116,139, 75,187,123,135, 71,183,119 },
  {  40,232, 24,216, 36,228, 20,212, 43,235, 27,219, 39,231, 23,215 },
  { 168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87 },
  {   2,194, 50,242, 14,206, 62,254,  1,193, 49,241, 13,205, 61,253 },
  { 130, 66,178,114,142, 78,190,126,129, 65,177,113,141, 77,189,125 },
  {  34,226, 18,210, 46,238, 30,222, 33,225, 17,209, 45,237, 29,221 },
  { 162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93 },
  {  10,202, 58,250,  6,198, 54,246,  9,201, 57,249,  5,197, 53,245 },
  { 138, 74,186,122,134, 70,182,118,137, 73,185,121,133, 69,181,117 },
  {  42,234, 26,218, 38,230, 22,214, 41,233, 25,217, 37,229, 21,213 },
  { 170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85 }
};


static inline uint16_t rgb565(int r, int g, int b)
{
	enum {
		R_MASK = 0xf800, R_LSHIFT = 8,
		G_MASK = 0x07e0, G_LSHIFT = 3,
		B_MASK = 0x001f, B_RSHIFT = 3
	};
	return ((r << R_LSHIFT) & R_MASK)
	     | ((g << G_LSHIFT) & G_MASK)
	     | ((b >> B_RSHIFT) & B_MASK);
}


static void convert_line_rgba_to_rgb565(const unsigned char *rgba_src,
                                        uint16_t *dst, int num_pixels, int line)
{
	using namespace Genode;

	enum { CHANNEL_MAX = 255 };

	int const *dm = dither_matrix[line & DITHER_MASK];

	for (int i = 0; i < num_pixels; i++) {
		int v = dm[i & DITHER_MASK] >> 5;

		*dst++ = rgb565(min(v + (int)rgba_src[0], (int)CHANNEL_MAX),
		                min(v + (int)rgba_src[1], (int)CHANNEL_MAX),
		                min(v + (int)rgba_src[2], (int)CHANNEL_MAX));

		/* we ignore the alpha channel */

		rgba_src += 4; /* next pixel */
	}
}


/******************
 ** Main program **
 ******************/

int main(int, char **)
{
	static Framebuffer::Connection framebuffer;
	Framebuffer::Session::Mode mode;
	int fb_width, fb_height;
	framebuffer.info(&fb_width, &fb_height, &mode);

	typedef uint16_t pixel_t;
	pixel_t *fb_base = Genode::env()->rm_session()->attach(framebuffer.dataspace());

	PDBG("Framebuffer is %dx%d\n", fb_width, fb_height);

	if (mode != Framebuffer::Session::RGB565) {
		PERR("Color modes other than RGB565 are not supported. Exiting.");
		return 1;
	}

	static pdfapp_t pdfapp;
	pdfapp_init(&pdfapp);

	pdfapp.scrw       = fb_width;
	pdfapp.scrh       = fb_height;
	pdfapp.resolution = 2*75; /* XXX read from config */
	pdfapp.pageno     = 9;    /* XXX read from config */

	char const *file_name = "test.pdf"; /* XXX read from config */

	int fd = open(file_name, O_BINARY | O_RDONLY, 0666);
	if (fd < 0) {
		PERR("Could not open input file \"%s\", Exiting.", file_name);
		return 2;
	}
	pdfapp_open(&pdfapp, (char *)file_name, fd, 0);

	if (pdfapp.image->n != 4) {
		PERR("Unexpected color depth, expected 4, got %d, Exiting.",
		     pdfapp.image->n);
		return 3;
	}

	int const x_max = Genode::min(fb_width,  pdfapp.image->w);
	int const y_max = Genode::min(fb_height, pdfapp.image->h);

	Genode::size_t src_line_bytes = pdfapp.image->n * pdfapp.image->w;
	unsigned char *src_line       = pdfapp.image->samples;

	Genode::size_t dst_line_width = fb_width; /* in pixels */
	pixel_t       *dst_line       = fb_base;

	for (int y = 0; y < y_max; y++) {
		convert_line_rgba_to_rgb565(src_line, dst_line, x_max, y);
		src_line += src_line_bytes;
		dst_line += dst_line_width;
	}

	framebuffer.refresh(0, 0, fb_width, fb_height);

	Genode::sleep_forever();
	return 0;
}
