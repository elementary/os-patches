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
 * Bluetooth implementaion which uses org.bluez on DBus 
 */
public class Bluez: Bluetooth, Object
{
  uint next_device_id = 1;
  org.bluez.Manager manager;
  org.bluez.Adapter default_adapter;

  private bool _powered = false;

  private bool powered {
    get { return _powered; }
    set { _powered = value; update_enabled(); }
  }

  private KillSwitch killswitch = new RfKillSwitch ();

  private string adapter_path = null;

  private DBusConnection bus = null;

  /* maps an org.bluez.Device's object_path to the org.bluez.Device proxy */
  HashTable<string,org.bluez.Device> path_to_proxy;

  /* maps an org.bluez.Device's object_path to our arbitrary unique id */
  HashTable<string,uint> path_to_id;

  /* maps our arbitrary unique id to an org.bluez.Device's object path */
  HashTable<uint,string> id_to_path;

  /* maps our arbitrary unique id to a Bluetooth.Device struct for public consumption */
  HashTable<uint,Device> id_to_device;

  public Bluez (KillSwitch? killswitch)
  {
    try
      {
        bus = Bus.get_sync (BusType.SYSTEM);
      }
    catch (Error e)
      {
        critical (@"$(e.message)");
      }

    if ((killswitch != null) && (killswitch.is_valid()))
      {
        this.killswitch = killswitch;
        killswitch.notify["blocked"].connect (() => update_enabled());
        update_enabled ();
      }

    id_to_path    = new HashTable<uint,string> (direct_hash, direct_equal);
    id_to_device  = new HashTable<uint,Device> (direct_hash, direct_equal);
    path_to_id    = new HashTable<string,uint> (str_hash, str_equal);
    path_to_proxy = new HashTable<string,org.bluez.Device> (str_hash, str_equal);

    reset_manager ();
  }

  private void reset_manager ()
  {
    string new_adapter_path = null;
    try
      {
        manager = bus.get_proxy_sync ("org.bluez", "/");

        // if the default adapter changes, update our connections
        manager.default_adapter_changed.connect ((object_path)
            => on_default_adapter_changed (object_path));

        // if the current adapter disappears, call clear_adapter()
        manager.adapter_removed.connect ((object_path) => { 
            if (object_path == adapter_path)
              clear_adapter ();
        });

        // get the current default adapter & watch for future default adapters
        new_adapter_path = manager.default_adapter ();
      }
    catch (Error e)
      {
        critical (@"$(e.message)");
      }

    on_default_adapter_changed (new_adapter_path);
  }

  private void clear_adapter ()
  {
    if (adapter_path != null)
      debug (@"clearing adapter; was using $adapter_path");

    path_to_proxy.remove_all ();
    path_to_id.remove_all ();
    id_to_path.remove_all ();
    id_to_device.remove_all ();

    default_adapter = null;
    adapter_path = null;

    discoverable = false;
    powered = false;
  }

  void on_default_adapter_changed (string? object_path)
  {
    clear_adapter ();

    if (object_path != null) try
      {
        adapter_path = object_path;
        default_adapter = Bus.get_proxy_sync (BusType.SYSTEM,
                                              "org.bluez",
                                              adapter_path);

        default_adapter.property_changed.connect (()
            => on_default_adapter_properties_changed ());

        default_adapter.device_removed.connect ((adapter, path) => {
          var id = path_to_id.lookup (path);
          path_to_id.remove (path);
          id_to_path.remove (id);
          id_to_device.remove (id);
          devices_changed ();
        });

        default_adapter.device_created.connect ((adapter, path)
            => add_device (path));

        foreach (var device_path in default_adapter.list_devices ())
          add_device (device_path);
      }
    catch (Error e)
     {
       critical (@"$(e.message)");
     }

    supported = object_path != null;

    on_default_adapter_properties_changed ();
  }

  /* When the default adapter's properties change,
     update our own properties "powered" and "discoverable" */
  private void on_default_adapter_properties_changed ()
  {
    bool is_discoverable = false;
    bool is_powered = false;

    if (default_adapter != null) try
      {
        var properties = default_adapter.get_properties ();

        var v = properties.lookup ("Discoverable");
        is_discoverable = (v != null) && v.get_boolean ();

        v = properties.lookup ("Powered");
        is_powered = (v != null) && v.get_boolean ();
      }
    catch (Error e) 
     {
       critical (@"$(e.message)");
     }

    powered = is_powered;
    discoverable = is_discoverable;
  }

  ////
  ////  bluetooth device UUIDs
  ////

  private static uint16 get_uuid16_from_uuid_string (string uuid)
  {
    uint16 uuid16;

    string[] tokens = uuid.split ("-", 1);
    if (tokens.length > 0)
      uuid16 = (uint16) uint64.parse ("0x"+tokens[0]);
    else
      uuid16 = 0;

    return uuid16;
  }

  // A device supports file transfer if OBEXObjectPush is in its uuid list
  private bool device_supports_file_transfer (uint16[] uuids)
  {
    foreach (var uuid16 in uuids)
      if (uuid16 == 0x1105) // OBEXObjectPush
        return true;

    return false;
  }

  // A device supports browsing if OBEXFileTransfer is in its uuid list
  private bool device_supports_browsing (uint16[] uuids)
  {
    foreach (var uuid16 in uuids)
      if (uuid16 == 0x1106) // OBEXFileTransfer
        return true;

    return false;
  }

  ////
  ////  Connectable Interfaces
  ////

  /* Headsets, Audio Sinks, and Input devices are connectable.
   *
   * This continues the behavior of the old gnome-bluetooth indicator.
   * But are there other interfaces we care about? */
  private DBusInterfaceInfo[] get_connectable_interfaces (DBusProxy device)
  {
    DBusInterfaceInfo[] connectable_interfaces = {};

    try
      {
        var iname = "org.freedesktop.DBus.Introspectable.Introspect";
        var intro = device.call_sync (iname, null, DBusCallFlags.NONE, -1);

        if ((intro != null) && (intro.n_children() > 0))
          {
            var xml = intro.get_child_value(0).get_string();
            var info = new DBusNodeInfo.for_xml (xml);
            if (info != null)
              {
                foreach (var i in info.interfaces)
                  {
                    if ((i.name == "org.bluez.AudioSink") ||
                        (i.name == "org.bluez.Headset") ||
                        (i.name == "org.bluez.Input"))
                      {
                        connectable_interfaces += i;
                      }
                  }
              }
          }
      }
    catch (Error e)
      {
       critical (@"$(e.message)");
      }

    return connectable_interfaces;
  }

  private bool device_is_connectable (DBusProxy device)
  {
    return get_connectable_interfaces (device).length > 0;
  }

  // call "Connect" on the specified interface
  private void device_connect_on_interface (DBusProxy proxy,
                                            string interface_name)
  {
    var object_path = proxy.get_object_path ();

    debug (@"trying to connect to $object_path: $(interface_name)");

    try
      {
        bus.call_sync ("org.bluez", object_path, interface_name,
                       "Connect", null, null, DBusCallFlags.NONE, -1);
      }
    catch (Error e)
      {
        debug (@"$object_path $interface_name.Connect() failed: $(e.message)");
      }
  }

  private void device_connect (org.bluez.Device device)
  {
    DBusProxy proxy = device as DBusProxy;

    // call "Connect" on all the interfaces that support it
    foreach (var i in get_connectable_interfaces (proxy))
      device_connect_on_interface (proxy, i.name);
  }

  private void device_disconnect (org.bluez.Device device)
  {
    try
      {
        device.disconnect ();
      }
    catch (Error e)
      {
        var object_path = (device as DBusProxy).get_object_path ();
        critical (@"Unable to disconnect $object_path: $(e.message)");
      }
  }

  ////
  ////  Device Upkeep
  ////

  private void add_device (string object_path)
  {
    if (!path_to_proxy.contains (object_path))
      {
        try
          {
            org.bluez.Device device = Bus.get_proxy_sync (BusType.SYSTEM,
                                                          "org.bluez",
                                                          object_path);
            path_to_proxy.insert (object_path, device);
            device.property_changed.connect(() => update_device (device)); 
            update_device (device);
          }
        catch (Error e)
          {
            critical (@"$(e.message)");
          }
      }
  }

  /* Update our public Device struct from the org.bluez.Device's properties.
   *
   * This is called when we first walk through bluez' Devices on startup,
   * when the org.bluez.Adapter gets a new device,
   * and when a device's properties change s.t. we need to rebuild the proxy.
   */
  private void update_device (org.bluez.Device device_proxy)
  {
    HashTable<string, GLib.Variant> properties;

    try {
      properties = device_proxy.get_properties ();
    } catch (Error e) {
      critical (@"$(e.message)");
      return;
    }

    // look up our id for this device.
    // if we don't have one yet, create one.
    var object_path = (device_proxy as DBusProxy).get_object_path();
    var id = path_to_id.lookup (object_path);
    if (id == 0)
      {
        id = next_device_id ++;
        id_to_path.insert (id, object_path);
        path_to_id.insert (object_path, id);
      }

    // look up the device's type
    Device.Type type;
    var v = properties.lookup ("Class");
    if (v == null)
      type = Device.Type.OTHER;
    else 
      type = Device.class_to_device_type (v.get_uint32());

    // look up the device's human-readable name
    v = properties.lookup ("Alias");
    if (v == null)
      v = properties.lookup ("Name");
    var name = v == null ? _("Unknown") : v.get_string ();

    // look up the device's bus address
    v = properties.lookup ("Address");
    var address = v.get_string ();

    // look up the device's bus address
    v = properties.lookup ("Icon");
    var icon = new ThemedIcon (v != null ? v.get_string() : "unknown");

    // derive a Connectable flag for this device
    var is_connectable = device_is_connectable (device_proxy as DBusProxy);

    // look up the device's Connected flag
    v = properties.lookup ("Connected");
    var is_connected = (v != null) && v.get_boolean ();

    // derive the uuid-related attributes we care about
    v = properties.lookup ("UUIDs");
    string[] uuid_strings = v.dup_strv ();
    uint16[] uuids = {};
    foreach (var s in uuid_strings)
      uuids += get_uuid16_from_uuid_string (s);
    var supports_browsing = device_supports_browsing (uuids);
    var supports_file_transfer = device_supports_file_transfer (uuids);

    // update our lookup table with these new attributes
    id_to_device.insert (id, new Device (id,
                                         type,
                                         name,
                                         address,
                                         icon,
                                         is_connectable,
                                         is_connected,
                                         supports_browsing,
                                         supports_file_transfer));

    devices_changed ();
    update_connected ();
  }

  /* update the 'enabled' property by looking at the killswitch state
     and the 'powered' property state */
  void update_enabled ()
  {
    var blocked = (killswitch != null) && killswitch.blocked;
    debug (@"in upate_enabled, powered is $powered, blocked is $blocked");
    enabled = powered && !blocked;
  }

  private bool have_connected_device ()
  {
    var devices = get_devices();

    foreach (var device in devices)
      if (device.is_connected)
        return true;

    return false;
  }

  private void update_connected ()
  {
    connected = have_connected_device ();
  }

  ////
  ////  Public API
  ////

  public void set_device_connected (uint id, bool connected)
  {
    var device = id_to_device.lookup (id);
    var path = id_to_path.lookup (id);
    var proxy = (path != null) ? path_to_proxy.lookup (path) : null;

    if ((proxy != null)
        && (device != null)
        && (device.is_connected != connected))
      {
        if (connected)
          device_connect (proxy);
        else
          device_disconnect (proxy);

        update_connected ();
      }
  }

  public void try_set_discoverable (bool b)
  {
    if (discoverable != b)
      {
        default_adapter.set_property.begin ("Discoverable", new Variant.boolean (b));
      }
  }

  public List<unowned Device> get_devices ()
  {
    return id_to_device.get_values();
  }

  public bool supported { get; protected set; default = false; }
  public bool discoverable { get; protected set; default = false; }
  public bool enabled { get; protected set; default = false; }
  public bool connected { get; protected set; default = false; }

  public void try_set_enabled (bool b)
  {
    if (killswitch != null)
      {
        debug (@"setting killswitch blocked to $(!b)");
        killswitch.try_set_blocked (!b);
      }
    else if (default_adapter != null)
      {
        debug (@"setting bluez Adapter's Powered property to $b");
        default_adapter.set_property.begin ("Powered", new Variant.boolean (b));
        powered = b;
      }
  }
}
