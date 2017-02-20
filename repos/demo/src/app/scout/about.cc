/*
 * \brief   Content generator for "About" section
 * \date    2008-07-24
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "elements.h"
#include "styles.h"

namespace Scout { Document *create_about(); }


Scout::Document *Scout::create_about()
{
	using namespace Scout;

	Document *doc = new Document();
	doc->title = "";

	/**
	 * Table of contents
	 */

	Chapter *toc = new Chapter();
	Center *tc = new Center();
	tc->append(new Spacer(1, 20));

	/* anchor for section "Scout Tutorial Browser" */
	Anchor *anchor0 = new Anchor();

	Block *b0 = new Block();
	b0->append_linktext("Scout Tutorial Browser", &link_style, anchor0);
	tc->append(b0);

	/* anchor for section "Technical background" */
	Anchor *anchor1 = new Anchor();

	b0 = new Block();
	b0->append_linktext("Technical background", &link_style, anchor1);
	tc->append(b0);

	/* anchor for section "Credits" */
	Anchor *anchor2 = new Anchor();

	b0 = new Block();
	b0->append_linktext("Credits", &link_style, anchor2);
	tc->append(b0);
	toc->append(tc);
	toc->append(new Spacer(1, 20));
	doc->toc = toc;
	doc->append(new Spacer(1, 10));
	Block *title = new Block(Block::CENTER);
	title->append_plaintext("", &chapter_style);
	doc->append(new Center(title));
	doc->append(new Spacer(1, 10));
	Block *authors = new Block(Block::CENTER);
	authors->append_plaintext("", &section_style);
	doc->append(new Center(authors));
	doc->append(new Spacer(1, 10));
	Block *date = new Block(Block::CENTER);
	date->append_plaintext("2006-02-06", &subsection_style);
	doc->append(new Center(date));
	doc->append(new Spacer(1, 10));

	/**
	 * Chapter "Scout Tutorial Browser"
	 */

	Chapter *chapter = new Chapter();
	chapter->append(anchor0);
	chapter->append(new Spacer(1, 20));

	b0 = new Block();
	b0->append_plaintext("Scout Tutorial Browser", &chapter_style);
	chapter->append(b0);
	chapter->append(anchor1);
	chapter->append(new Spacer(1, 15));

	b0 = new Block();
	b0->append_plaintext("Technical background", &section_style);
	chapter->append(b0);

	b0 = new Block();
	b0->append_plaintext("The Scout Tutorial Browser was created as a low-complexity example application", &plain_style);
	b0->append_plaintext("for the Nitpicker GUI. It is implemented with less than", &plain_style);
	b0->append_plaintext("4,000", &mono_style);
	b0->append_plaintext("lines", &plain_style);
	b0->append_plaintext("of C++ code. For PNG image support, libpng is additionally required.", &plain_style);
	b0->append_plaintext("Scout demonstrates that neat and useful applications can be built ontop of an extremely", &plain_style);
	b0->append_plaintext("small underlying code base.", &plain_style);
	chapter->append(b0);

	b0 = new Block();
	b0->append_plaintext("The most interesting features of Scout are:", &plain_style);
	chapter->append(b0);

	Item *i0 = new Item(&plain_style, " o", 20);

	Block *b1 = new Block();
	b1->append_plaintext("Antialiased fonts by the use of an intermediate pixel font format,", &plain_style);
	i0->append(b1);
	chapter->append(i0);

	i0 = new Item(&plain_style, " o", 20);

	b1 = new Block();
	b1->append_plaintext("Procedurally textured background that is generated in runtime,", &plain_style);
	i0->append(b1);
	chapter->append(i0);

	i0 = new Item(&plain_style, " o", 20);

	b1 = new Block();
	b1->append_plaintext("Refraction and alpha blending of the icons of the Nitpicker-version,", &plain_style);
	i0->append(b1);
	chapter->append(i0);

	i0 = new Item(&plain_style, " o", 20);

	b1 = new Block();
	b1->append_plaintext("Text elements such as hyperlinks, accentuations, items, verbatim, and sections,", &plain_style);
	i0->append(b1);
	chapter->append(i0);

	i0 = new Item(&plain_style, " o", 20);

	b1 = new Block();
	b1->append_plaintext("Smooth acceleration and deceleration of scrolling (e.g., when hitting the bottom of", &plain_style);
	b1->append_plaintext("a page),", &plain_style);
	i0->append(b1);
	chapter->append(i0);

	i0 = new Item(&plain_style, " o", 20);

	b1 = new Block();
	b1->append_plaintext("Fading icons and links on mouse-over, and", &plain_style);
	i0->append(b1);
	chapter->append(i0);

	i0 = new Item(&plain_style, " o", 20);

	b1 = new Block();
	b1->append_plaintext("Document history browsing.", &plain_style);
	i0->append(b1);
	chapter->append(i0);

	b0 = new Block();
	b0->append_plaintext("All graphics are runtime generated or created via POV-Ray.", &plain_style);
	chapter->append(b0);

	b0 = new Block();
	b0->append_plaintext("Although Scout was originally designed as a Nitpicker client application, it also", &plain_style);
	b0->append_plaintext("runs natively on the DOpE window server, on the L4 Console, and as libSDL program", &plain_style);
	b0->append_plaintext("on Linux/X11.", &plain_style);
	chapter->append(b0);
	chapter->append(anchor2);
	chapter->append(new Spacer(1, 15));

	b0 = new Block();
	b0->append_plaintext("Credits", &section_style);
	chapter->append(b0);

	b0 = new Block();
	b0->append_plaintext("The graphics, design, and code of Scout were created by Norman Feske.", &plain_style);
	chapter->append(b0);

	Block *descitem = new Block(32);
	descitem->append_plaintext("E-Mail", &bold_style);


	chapter->append(descitem);

	Verbatim *v0 = new Verbatim(verbatim_bgcol);
	v0->append_textline(" norman.feske@genode-labs.com", &mono_style);
	chapter->append(v0);

	descitem = new Block(32);
	descitem->append_plaintext("Website", &bold_style);


	chapter->append(descitem);

	v0 = new Verbatim(verbatim_bgcol);
	v0->append_textline(" http://os.inf.tu-dresden.de/~nf2", &mono_style);
	chapter->append(v0);


	return chapter;
}
