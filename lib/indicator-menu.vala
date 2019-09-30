/*
 * Copyright 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: William Hua <william.hua@canonical.com>
 */

public class Indicator.Keyboard.IndicatorMenu : MenuModel {

	public enum Options {
		NONE     = 0x0,
		DCONF    = 0x1,
		IBUS     = 0x2,
		SETTINGS = 0x4
	}

	private Options options;

	private Menu indicator_menu;
	private Menu sources_section;
	private IBusMenu properties_section;

	public IndicatorMenu (ActionMap? action_map = null, Options options = Options.NONE) {
		var submenu = new Menu ();

		sources_section = new Menu ();
		submenu.append_section (null, sources_section);

		if ((options & Options.IBUS) != Options.NONE) {
			properties_section = new IBusMenu (action_map);
			properties_section.activate.connect ((property, state) => { activate (property, state); });
			submenu.append_section (null, properties_section);
		}

		if ((options & Options.SETTINGS) != Options.NONE) {
			var settings_section = new Menu ();
			settings_section.append (_ ("Character Map"), "indicator.map");
			settings_section.append (_ ("Keyboard Layout Chart"), "indicator.chart");
			settings_section.append (_ ("Text Entry Settings..."), "indicator.settings");
			submenu.append_section (null, settings_section);
		}

		var indicator = new MenuItem.submenu (null, submenu);
		indicator.set_detailed_action ("indicator.indicator");
		indicator.set_attribute ("x-canonical-type", "s", "com.canonical.indicator.root");

		/* We need special mouse actions on the lock screen. */
		if ((options & Options.DCONF) != Options.NONE) {
			indicator.set_attribute ("x-canonical-secondary-action", "s", "indicator.next");
			indicator.set_attribute ("x-canonical-scroll-action", "s", "indicator.scroll");
		} else {
			indicator.set_attribute ("x-canonical-secondary-action", "s", "indicator.locked_next");
			indicator.set_attribute ("x-canonical-scroll-action", "s", "indicator.locked_scroll");
		}

		indicator_menu = new Menu ();
		indicator_menu.append_item (indicator);

		this.options = options;
	}

	public signal void activate (IBus.Property property, IBus.PropState state);

	public void set_sources (Source[] sources) {
		sources_section.remove_all ();

		for (var i = 0; i < sources.length; i++) {
			if (!sources[i].is_ibus || (options & Options.IBUS) != Options.NONE) {
				string action;

				if ((options & Options.DCONF) != Options.NONE) {
					action = "indicator.current";
				} else {
					action = "indicator.active";
				}

				var item = new MenuItem (sources[i].name, action);

				item.set_attribute (Menu.ATTRIBUTE_TARGET, "u", i);

				if (sources[i].icon != null) {
					item.set_icon ((!) sources[i].icon);
				}

				sources_section.append_item (item);
			}
		}
	}

	public void set_properties (IBus.PropList properties) {
		if ((options & Options.IBUS) != Options.NONE) {
			properties_section.set_properties (properties);
		}
	}

	public void update_property (IBus.Property property) {
		if ((options & Options.IBUS) != Options.NONE) {
			properties_section.update_property (property);
		}
	}

	public override bool is_mutable () {
		return indicator_menu.is_mutable ();
	}

	public override int get_n_items () {
		return indicator_menu.get_n_items ();
	}

	public override void get_item_attributes (int item_index, out HashTable<string, Variant>? attributes) {
		indicator_menu.get_item_attributes (item_index, out attributes);
	}

	public override void get_item_links (int item_index, out HashTable<string, MenuModel>? links) {
		indicator_menu.get_item_links (item_index, out links);
	}

	public override Variant get_item_attribute_value (int item_index, string attribute, VariantType? expected_type) {
		return indicator_menu.get_item_attribute_value (item_index, attribute, expected_type);
	}

	public override MenuModel get_item_link (int item_index, string link) {
		return indicator_menu.get_item_link (item_index, link);
	}

	public override MenuAttributeIter iterate_item_attributes (int item_index) {
		return indicator_menu.iterate_item_attributes (item_index);
	}

	public override MenuLinkIter iterate_item_links (int item_index) {
		return indicator_menu.iterate_item_links (item_index);
	}
}
