/*
 * \brief  VirtualBox pointer policies implementation
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \date   2015-06-08
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/attached_rom_dataspace.h>
#include <os/attached_ram_dataspace.h>
#include <os/pixel_alpha8.h>
#include <os/pixel_rgb888.h>
#include <os/surface.h>
#include <os/texture_rgb888.h>
#include <vbox_pointer/dither_painter.h>

/* local includes */
#include "policy.h"


/******************************
 ** Entry in policy registry **
 ******************************/

class Vbox_pointer::Policy_entry : public Vbox_pointer::Policy,
                                   public Genode::List<Policy_entry>::Element
{
	private:

		String _label;
		String _domain;

		Pointer_updater &_updater;

		Genode::Attached_ram_dataspace _texture_pixel_ds { Genode::env()->ram_session(),
		                                                   Vbox_pointer::MAX_WIDTH  *
		                                                   Vbox_pointer::MAX_HEIGHT *
		                                                   sizeof(Genode::Pixel_rgb888) };

		Genode::Attached_ram_dataspace _texture_alpha_ds { Genode::env()->ram_session(),
		                                                   Vbox_pointer::MAX_WIDTH  *
		                                                   Vbox_pointer::MAX_HEIGHT };
		Genode::Attached_rom_dataspace _shape_ds;

		Genode::Signal_dispatcher<Policy_entry> shape_signal_dispatcher {
			_updater.signal_receiver(), *this, &Policy_entry::_import_shape };

		Nitpicker::Area  _shape_size;
		Nitpicker::Point _shape_hot;

		void _import_shape(unsigned = 0)
		{
			using namespace Genode;

			_shape_ds.update();

			if (!_shape_ds.valid())
				return;

			if (_shape_ds.size() < sizeof(Vbox_pointer::Shape_report))
				return;

			Vbox_pointer::Shape_report *shape_report =
				_shape_ds.local_addr<Vbox_pointer::Shape_report>();

			if (!shape_report->visible
			 || shape_report->width == 0 || shape_report->height == 0
			 || shape_report->width > Vbox_pointer::MAX_WIDTH
			 || shape_report->height > Vbox_pointer::MAX_HEIGHT) {
				_shape_size = Nitpicker::Area();
				_shape_hot  = Nitpicker::Point();
				_updater.update_pointer(this);
			}

			_shape_size = Nitpicker::Area(shape_report->width, shape_report->height);
			_shape_hot  = Nitpicker::Point(-shape_report->x_hot, -shape_report->y_hot);

			Texture<Pixel_rgb888>
				texture(_texture_pixel_ds.local_addr<Pixel_rgb888>(),
				        _texture_alpha_ds.local_addr<unsigned char>(),
				        _shape_size);

			for (unsigned int y = 0; y < _shape_size.h(); y++) {

				/* convert the shape data from BGRA encoding to RGBA encoding */
				unsigned char *shape     = shape_report->shape;
				unsigned char *bgra_line = &shape[y * _shape_size.w() * 4];
				unsigned char rgba_line[_shape_size.w() * 4];
				for (unsigned int i = 0; i < _shape_size.w() * 4; i += 4) {
					rgba_line[i + 0] = bgra_line[i + 2];
					rgba_line[i + 1] = bgra_line[i + 1];
					rgba_line[i + 2] = bgra_line[i + 0];
					rgba_line[i + 3] = bgra_line[i + 3];
				}

				/* import the RGBA-encoded line into the texture */
				texture.rgba(rgba_line, _shape_size.w(), y);
			}

			_updater.update_pointer(this);
		}

	public:

		Policy_entry(String const &label, String const &domain, String const &rom,
		             Pointer_updater &updater)
		:
			_label(label), _domain(domain), _updater(updater),
			_shape_ds(rom.string())
		{
			_import_shape();

			_shape_ds.sigh(shape_signal_dispatcher);
		}

		/**
		 * Return similarity of policy label and the passed label
		 */
		Genode::size_t match_label(String const &other) const
		{
			if (_label.length() > 1 && other.length() > 1) {
				/* length of string w/o null byte */
				Genode::size_t const len = _label.length() - 1;

				if (Genode::strcmp(other.string(), _label.string(), len) == 0)
					return len;
			}

			return 0;
		}

		/**
		 * Return if policy domain and passed domain match exactly
		 */
		bool match_domain(String const &other) const
		{
			return _domain.length() > 1 && _domain == other;
		}

		/**********************
		 ** Policy interface **
		 **********************/

		Nitpicker::Area shape_size() const override { return _shape_size; }
		Nitpicker::Point shape_hot() const override { return _shape_hot; }

		bool shape_valid() const override { return _shape_size.valid(); }

		void draw_shape(Genode::Pixel_rgb565 *pixel) override
		{
			using namespace Genode;

			if (!_shape_size.valid())
				return;

			Pixel_alpha8 *alpha =
				reinterpret_cast<Pixel_alpha8 *>(pixel + _shape_size.count());

			Surface<Pixel_rgb565> pixel_surface(pixel, _shape_size);
			Surface<Pixel_alpha8> alpha_surface(alpha, _shape_size);

			Texture<Pixel_rgb888>
				texture(_texture_pixel_ds.local_addr<Pixel_rgb888>(),
				        _texture_alpha_ds.local_addr<unsigned char>(),
				        _shape_size);

			Dither_painter::paint(pixel_surface, texture);
			Dither_painter::paint(alpha_surface, texture);
		}
};


/**************
 ** Registry **
 **************/

void Vbox_pointer::Policy_registry::update(Genode::Xml_node config)
{
	/* TODO real update should flush at least */

	try {
		for (Genode::Xml_node policy = config.sub_node("policy");
		     true; policy = policy.next("policy")) {

			String label  = read_string_attribute(policy, "label",  String());
			String domain = read_string_attribute(policy, "domain", String());
			String rom    = read_string_attribute(policy, "rom",    String());

			if (!label.valid() && !domain.valid())
				Genode::warning("policy does not declare label/domain attribute");
			else if (!rom.valid())
				Genode::warning("policy does not declare shape rom");
			else
				insert(new (Genode::env()->heap())
				       Policy_entry(label, domain, rom, _updater));
		}
	} catch (...) { }
}


Vbox_pointer::Policy * Vbox_pointer::Policy_registry::lookup(String const &label,
                                                             String const &domain)
{
	/* try label similarity matching first */
	unsigned      similarity = 0;
	Policy_entry *match      = nullptr;
	for (Policy_entry *p = first(); p; p = p->next()) {
		unsigned s = p->match_label(label);
		if (s > similarity) {
			similarity = s;
			match      = p;
		}
	}
	if (match) return match;

	/* then match domains */
	for (Policy_entry *p = first(); p; p = p->next())
		if (p->match_domain(domain))
			return p;

	return nullptr;
}
