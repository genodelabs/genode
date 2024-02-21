/*
 * \brief  Widget for the software tabs
 * \author Norman Feske
 * \date   2022-09-l9
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__SOFTWARE_TABS_WIDGET_H_
#define _VIEW__SOFTWARE_TABS_WIDGET_H_

#include <view/dialog.h>
#include <model/presets.h>

namespace Sculpt { struct Software_tabs_widget; }


struct Sculpt::Software_tabs_widget : Widget<Frame>
{
	enum class Tab { PRESETS, RUNTIME, ADD, OPTIONS, UPDATE, STATUS };

	Tab _selected = Tab::RUNTIME;

	struct Tab_button : Select_button<Tab>
	{
		using Select_button::Select_button;

		void view(Scope<Button> &s, Tab selected_value, bool ready) const
		{
			bool const selected = (selected_value == _value),
			           hovered  = (s.hovered() && !s.dragged() && !selected && ready);

			if (selected) s.attribute("selected", "yes");
			if (hovered)  s.attribute("hovered",  "yes");
			if (!ready)   s.attribute("style",    "unimportant");

			s.sub_scope<Label>(s.id.value);
		}
	};

	Hosted<Frame, Hbox, Tab_button> const
		_status  { Id { "Status"  }, Tab::STATUS  },
		_presets { Id { "Presets" }, Tab::PRESETS },
		_runtime { Id { "Runtime" }, Tab::RUNTIME },
		_add     { Id { "Add"     }, Tab::ADD     },
		_options { Id { "Options" }, Tab::OPTIONS },
		_update  { Id { "Update"  }, Tab::UPDATE  };

	void view(Scope<Frame>          &s,
	          Storage_target  const &storage_target,
	          Presets         const &presets,
	          bool            const  status_available) const
	{
		s.sub_scope<Hbox>([&] (Scope<Frame, Hbox> &s) {
			s.widget(_status,  _selected, status_available);
			s.widget(_presets, _selected, storage_target.valid() && presets.available());
			s.widget(_runtime, _selected, true);
			s.widget(_add,     _selected, storage_target.valid());
			s.widget(_options, _selected, storage_target.valid());
			s.widget(_update,  _selected, storage_target.valid());
		});
	}

	void click(Clicked_at const &at, auto const &fn)
	{
		_status .propagate(at, [&] (Tab t) { _selected = t; });
		_presets.propagate(at, [&] (Tab t) { _selected = t; });
		_runtime.propagate(at, [&] (Tab t) { _selected = t; });
		_add    .propagate(at, [&] (Tab t) { _selected = t; });
		_options.propagate(at, [&] (Tab t) { _selected = t; });
		_update .propagate(at, [&] (Tab t) { _selected = t; });

		fn();
	}

	bool presets_selected() const { return _selected == Tab::PRESETS; }
	bool runtime_selected() const { return _selected == Tab::RUNTIME; }
	bool options_selected() const { return _selected == Tab::OPTIONS; }
	bool add_selected()     const { return _selected == Tab::ADD;     }
	bool update_selected()  const { return _selected == Tab::UPDATE;  }
	bool status_selected()  const { return _selected == Tab::STATUS;  }
};

#endif /* _VIEW__SOFTWARE_TABS_WIDGET_H_ */
