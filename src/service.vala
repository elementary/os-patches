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
 *   Robert Ancell <robert.ancell@canonical.com>
 */

/**
 * Boilerplate class to own the name on the bus,
 * to create the profiles, and to export them on the bus.
 */ 
public class Service: Object
{
  private MainLoop loop;
  private SimpleActionGroup actions;
  private HashTable<string,Profile> profiles;
  private DBusConnection connection;
  private uint exported_action_id;
  private const string OBJECT_PATH = "/com/canonical/indicator/bluetooth";

  private void unexport ()
  {
    if (connection != null)
      {
        profiles.for_each ((name, profile)
          => profile.unexport_menu (connection));

        if (exported_action_id != 0)
          {
            debug (@"unexporting action group '$(OBJECT_PATH)'");
            connection.unexport_action_group (exported_action_id);
            exported_action_id = 0;
          }
      }
  }

  public Service (Bluetooth bluetooth)
  {
    actions = new SimpleActionGroup ();

    profiles = new HashTable<string,Profile> (str_hash, str_equal);
    profiles.insert ("phone", new Phone (bluetooth, actions));
    profiles.insert ("desktop", new Desktop (bluetooth, actions));
  }

  public int run ()
  {
    if (loop != null)
      {
        warning ("service is already running");
        return Posix.EXIT_FAILURE;
      }

    var own_name_id = Bus.own_name (BusType.SESSION,
                                    "com.canonical.indicator.bluetooth",
                                    BusNameOwnerFlags.NONE,
                                    on_bus_acquired,
                                    null,
                                    on_name_lost);

    loop = new MainLoop (null, false);
    loop.run ();

    // cleanup
    unexport ();
    Bus.unown_name (own_name_id);
    return Posix.EXIT_SUCCESS;
  }

  void on_bus_acquired (DBusConnection connection, string name)
  {
    debug (@"bus acquired: $name");
    this.connection = connection;

    try
      {
        debug (@"exporting action group '$(OBJECT_PATH)'");
        exported_action_id = connection.export_action_group (OBJECT_PATH,
                                                             actions);
      }
    catch (Error e)
      {
        critical (@"Unable to export actions on $OBJECT_PATH: $(e.message)");
      }

    profiles.for_each ((name, profile)
        => profile.export_menu (connection, @"$OBJECT_PATH/$name"));
  }

  void on_name_lost (DBusConnection connection, string name)
  {
    debug (@"name lost: $name");
    loop.quit ();
  }
}
