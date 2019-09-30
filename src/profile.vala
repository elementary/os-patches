/*
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 */

class Profile: Object
{
  protected Bluetooth bluetooth;
  protected string profile_name;
  protected Menu root;
  protected Menu menu;
  private uint menu_export_id;
  protected SimpleAction root_action;

  protected bool visible { get; set; default = true; }

  public Profile (Bluetooth bluetooth, string profile_name)
  {
    this.bluetooth = bluetooth;
    this.profile_name = profile_name;

    root_action = new SimpleAction.stateful (@"root-$profile_name",
                                             null,
                                             action_state_for_root());
    notify["visible"].connect (() => update_root_action_state());
    bluetooth.notify["enabled"].connect (() => update_root_action_state());
    bluetooth.notify["connected"].connect (() => update_root_action_state());
    menu = new Menu ();

    var item = create_root_menuitem ();
    item.set_submenu (menu);

    root = new Menu ();
    root.append_item (item);
  }

  public void export_menu (DBusConnection connection, string object_path)
  {
    try
      {
        debug (@"exporting menu '$profile_name'");
        menu_export_id = connection.export_menu_model (object_path, root);
      }
    catch (Error e)
      {
        critical (@"Unable to export menu on $object_path: $(e.message)");
      }
  }

  public void unexport_menu (DBusConnection connection)
  {
    if (menu_export_id != 0)
      {
        debug (@"unexporting menu '$profile_name'");
        connection.unexport_menu_model (menu_export_id);
        menu_export_id = 0;
      }
  }

  protected void spawn_command_line_async (string command)
  {
    try {
      Process.spawn_command_line_async (command);
    } catch (Error e) {
      warning (@"Unable to launch '$command': $(e.message)");
    }
  }

  ///
  ///  Menu Items
  ///

  protected MenuItem create_enabled_menuitem ()
  {
    var item = new MenuItem (_("Bluetooth"), "indicator.bluetooth-enabled");

    item.set_attribute ("x-canonical-type", "s",
                        "com.canonical.indicator.switch");

    return item;
  }

  private MenuItem create_root_menuitem ()
  {
    var item = new MenuItem (null, @"indicator.root-$profile_name");

    item.set_attribute ("x-canonical-type", "s",
                        "com.canonical.indicator.root");

    return item;
  }

  ///
  ///  Actions
  ///

  protected Action create_supported_action (Bluetooth bluetooth)
  {
    var action = new SimpleAction.stateful ("bluetooth-supported",
                                            null,
                                            new Variant.boolean (bluetooth.supported));

    bluetooth.notify["supported"].connect (()
        => action.set_state (new Variant.boolean (bluetooth.supported)));

    return action;
  }

  protected Action create_enabled_action (Bluetooth bluetooth)
  {
    var action = new SimpleAction.stateful ("bluetooth-enabled",
                                            null,
                                            new Variant.boolean (bluetooth.enabled));

    action.activate.connect (()
        => action.change_state (new Variant.boolean (!action.get_state().get_boolean())));

    action.change_state.connect ((action, requestedValue)
        => bluetooth.try_set_enabled (requestedValue.get_boolean()));

    bluetooth.notify["enabled"].connect (()
        => action.set_state (new Variant.boolean (bluetooth.enabled)));

    return action;
  }

  protected void update_root_action_state ()
  {
    root_action.set_state (action_state_for_root ());
  }

  protected Variant action_state_for_root ()
  {
    string a11y;
    string icon_name;

    if (bluetooth.connected)
      {
        a11y = "Bluetooth (connections)";
        icon_name = "bluetooth-paired";
      }
    else if (bluetooth.enabled)
      {
        a11y = "Bluetooth (on)";
        icon_name = "bluetooth-active";
      }
    else
      {
        a11y = "Bluetooth (off)";
        icon_name = "bluetooth-disabled";
      }

    var icon = new ThemedIcon.with_default_fallbacks (icon_name);
    var builder = new VariantBuilder (new VariantType ("a{sv}"));
    builder.add ("{sv}", "visible", new Variant.boolean (visible));
    builder.add ("{sv}", "accessible-desc", new Variant.string (a11y));
    builder.add ("{sv}", "icon", icon.serialize());
    builder.add ("{sv}", "title", new Variant.string (_("Bluetooth")));
    return builder.end ();
  }
}
