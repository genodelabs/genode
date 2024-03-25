/*
 * \brief  Storage management widget
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__STORAGE_WIDGET_H_
#define _VIEW__STORAGE_WIDGET_H_

#include <types.h>
#include <model/storage_devices.h>
#include <view/storage_device_widget.h>

namespace Sculpt { struct Storage_devices_widget_base; }


struct Sculpt::Storage_devices_widget_base : Widget<Vbox>
{
	Storage_devices const &_storage_devices;
	Storage_target  const &_used_target;

	Constructible<Hosted<Vbox, Frame, Storage_device_widget>> _storage_device_widget { };

	Block_device::Label _selected_device { };

	Storage_devices_widget_base(Storage_devices const &storage_devices,
	                            Storage_target  const &used_target)
	:
		_storage_devices(storage_devices), _used_target(used_target)
	{ }

	template <typename DEVICE, typename BUTTON>
	void _view_device(Scope<Vbox> &s, DEVICE const &dev, BUTTON const &button) const
	{
		bool const selected = ( _selected_device == dev.name() );

		s.widget(button, dev, selected, _used_target);

		if (_storage_device_widget.constructed() && selected) {
			s.sub_scope<Frame>([&] (Scope<Vbox, Frame> &s) {
				s.attribute("style", "invisible");
				s.widget(*_storage_device_widget, dev, _used_target);
			});
		}
	}

	void _with_selected_device(auto const &fn) const
	{
		if (_selected_device.valid() && _storage_device_widget.constructed())
			_storage_devices.for_each([&] (Storage_device const &dev) {
				if (_selected_device == dev.name())
					fn(dev); });
	}

	template <typename BUTTON>
	void _click_device(Clicked_at const &at, Storage_device_widget::Action &action)
	{
		/* select device */
		Id const id = at.matching_id<Vbox, BUTTON>();
		if (id.valid()) {
			if (id.value == _selected_device) {
				_selected_device = { };
				_storage_device_widget.destruct();
			} else {
				_selected_device = id.value;
				_storage_device_widget.construct(Id { id });
			}
		}

		_with_selected_device([&] (Storage_device const &dev) {
			_storage_device_widget->propagate(at, dev, _used_target, action); });
	}

	void _clack_device(Clacked_at const &at, Storage_device_widget::Action &action)
	{
		_with_selected_device([&] (Storage_device const &dev) {
			_storage_device_widget->propagate(at, dev, action); });
	}

	void reset_operation()
	{
		if (_storage_device_widget.constructed())
			_storage_device_widget->reset_operation();
	}

	void reset()
	{
		_storage_device_widget.destruct();
		_selected_device = { };
	}
};


namespace Sculpt { struct Storage_device_button; }

struct Sculpt::Storage_device_button : Widget<Button>
{
	using Model = String<64>;
	Model const model;

	Storage_device_button(Model const &model) : model(model) { }

	void view(Scope<Button> &s, Storage_device const &dev, bool const selected,
	          Storage_target const &used_target) const
	{
		if (s.hovered()) s.attribute("hovered",  "yes");
		if (selected)    s.attribute("selected", "yes");

		s.sub_scope<Hbox>([&] (Scope<Button, Hbox> &s) {
			s.sub_scope<Left_floating_hbox>(
				[&] (Scope<Button, Hbox, Left_floating_hbox> &s) {
					s.sub_scope<Label>(dev.name());
					if (model.length() > 1)
						s.sub_scope<Label>(String<80>(" (", model, ") "));
					if (used_target.device_and_port() == dev.name())
						s.sub_scope<Label>("* ");
			});

			s.sub_scope<Right_floating_hbox>(
				[&] (Scope<Button, Hbox, Right_floating_hbox> &s) {
					s.sub_scope<Label>(String<64>(dev.capacity)); });
		});
	}
};


namespace Sculpt { struct Block_devices_widget; }

struct Sculpt::Block_devices_widget : Storage_devices_widget_base
{
	using Storage_devices_widget_base::Storage_devices_widget_base;

	void view(Scope<Vbox> &s) const
	{
		s.sub_scope<Min_ex>(35);
		_storage_devices.block_devices.for_each([&] (Block_device const &dev) {
			Hosted<Vbox, Storage_device_button> button { Id { dev.name() }, dev.model };
			_view_device(s, dev, button);
		});
	}

	void click(Clicked_at const &at, Storage_device_widget::Action &action)
	{
		_click_device<Storage_device_button>(at, action);
	}

	void clack(Clacked_at const &at, Storage_device_widget::Action &action)
	{
		_clack_device(at, action);
	}
};


namespace Sculpt { struct Ahci_devices_widget; }

struct Sculpt::Ahci_devices_widget : Storage_devices_widget_base
{
	using Storage_devices_widget_base::Storage_devices_widget_base;

	void view(Scope<Vbox> &s) const
	{
		s.sub_scope<Min_ex>(35);
		_storage_devices.ahci_devices.for_each([&] (Ahci_device const &dev) {
			Hosted<Vbox, Storage_device_button> button { Id { dev.name() }, dev.model };
			_view_device(s, dev, button);
		});
	}

	void click(Clicked_at const &at, Storage_device_widget::Action &action)
	{
		_click_device<Storage_device_button>(at, action);
	}

	void clack(Clacked_at const &at, Storage_device_widget::Action &action)
	{
		_clack_device(at, action);
	}
};


namespace Sculpt { struct Nvme_devices_widget; }

struct Sculpt::Nvme_devices_widget : Storage_devices_widget_base
{
	using Storage_devices_widget_base::Storage_devices_widget_base;

	void view(Scope<Vbox> &s) const
	{
		s.sub_scope<Min_ex>(35);
		_storage_devices.nvme_devices.for_each([&] (Nvme_device const &dev) {
			Hosted<Vbox, Storage_device_button> button { Id { dev.name() }, dev.model };
			_view_device(s, dev, button);
		});
	}

	void click(Clicked_at const &at, Storage_device_widget::Action &action)
	{
		_click_device<Storage_device_button>(at, action);
	}

	void clack(Clacked_at const &at, Storage_device_widget::Action &action)
	{
		_clack_device(at, action);
	}
};


namespace Sculpt { struct Mmc_devices_widget; }

struct Sculpt::Mmc_devices_widget : Storage_devices_widget_base
{
	using Storage_devices_widget_base::Storage_devices_widget_base;

	void view(Scope<Vbox> &s) const
	{
		s.sub_scope<Min_ex>(35);
		_storage_devices.mmc_devices.for_each([&] (Mmc_device const &dev) {
			Hosted<Vbox, Storage_device_button> button { Id { dev.name() }, dev.model };
			_view_device(s, dev, button);
		});
	}

	void click(Clicked_at const &at, Storage_device_widget::Action &action)
	{
		_click_device<Storage_device_button>(at, action);
	}

	void clack(Clacked_at const &at, Storage_device_widget::Action &action)
	{
		_clack_device(at, action);
	}
};


namespace Sculpt { struct Usb_storage_device_button; }

struct Sculpt::Usb_storage_device_button : Widget<Button>
{
	void view(Scope<Button> &s, Usb_storage_device const &dev, bool const selected,
	          Storage_target const &used_target) const
	{
		bool const discarded = dev.discarded();

		if (s.hovered() && !discarded) s.attribute("hovered",  "yes");
		if (selected)                  s.attribute("selected", "yes");

		s.sub_scope<Hbox>([&] (Scope<Button, Hbox> &s) {
			s.sub_scope<Left_floating_hbox>(
				[&] (Scope<Button, Hbox, Left_floating_hbox> &s) {
					s.sub_scope<Label>(dev.name());
					if (dev.driver_info.constructed()) {
						String<16> const vendor { dev.driver_info->vendor };
						s.sub_scope<Label>(String<64>(" (", vendor, ") "));
					}
					if (used_target.device_and_port() == dev.name())
						s.sub_scope<Label>("* ");
			});

			using Info = String<64>;
			Info const info = dev.discarded() ? Info(" unsupported")
			                                  : Info(" ", dev.capacity);

			s.sub_scope<Right_floating_hbox>(
				[&] (Scope<Button, Hbox, Right_floating_hbox> &s) {
					s.sub_scope<Label>(info); });
		});
	}
};


namespace Sculpt { struct Usb_devices_widget; }

struct Sculpt::Usb_devices_widget : Storage_devices_widget_base
{
	using Storage_devices_widget_base::Storage_devices_widget_base;

	void view(Scope<Vbox> &s) const
	{
		s.sub_scope<Min_ex>(35);
		_storage_devices.usb_storage_devices.for_each([&] (Usb_storage_device const &dev) {
			Hosted<Vbox, Usb_storage_device_button> button { Id { dev.name() } };
			_view_device(s, dev, button);
		});
	}

	void click(Clicked_at const &at, Storage_device_widget::Action &action)
	{
		_click_device<Usb_storage_device_button>(at, action);
	}

	void clack(Clacked_at const &at, Storage_device_widget::Action &action)
	{
		_clack_device(at, action);
	}
};

#endif /* _VIEW__STORAGE_WIDGET_H_ */
