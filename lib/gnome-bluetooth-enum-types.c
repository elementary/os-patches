


#include <bluetooth-enums.h>
#include "gnome-bluetooth-enum-types.h"
#include <glib-object.h>

/* enumerations from "bluetooth-enums.h" */
GType
bluetooth_category_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { BLUETOOTH_CATEGORY_ALL, "BLUETOOTH_CATEGORY_ALL", "all" },
      { BLUETOOTH_CATEGORY_PAIRED, "BLUETOOTH_CATEGORY_PAIRED", "paired" },
      { BLUETOOTH_CATEGORY_TRUSTED, "BLUETOOTH_CATEGORY_TRUSTED", "trusted" },
      { BLUETOOTH_CATEGORY_NOT_PAIRED_OR_TRUSTED, "BLUETOOTH_CATEGORY_NOT_PAIRED_OR_TRUSTED", "not-paired-or-trusted" },
      { BLUETOOTH_CATEGORY_PAIRED_OR_TRUSTED, "BLUETOOTH_CATEGORY_PAIRED_OR_TRUSTED", "paired-or-trusted" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("BluetoothCategory", values);
  }
  return etype;
}
GType
bluetooth_type_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GFlagsValue values[] = {
      { BLUETOOTH_TYPE_ANY, "BLUETOOTH_TYPE_ANY", "any" },
      { BLUETOOTH_TYPE_PHONE, "BLUETOOTH_TYPE_PHONE", "phone" },
      { BLUETOOTH_TYPE_MODEM, "BLUETOOTH_TYPE_MODEM", "modem" },
      { BLUETOOTH_TYPE_COMPUTER, "BLUETOOTH_TYPE_COMPUTER", "computer" },
      { BLUETOOTH_TYPE_NETWORK, "BLUETOOTH_TYPE_NETWORK", "network" },
      { BLUETOOTH_TYPE_HEADSET, "BLUETOOTH_TYPE_HEADSET", "headset" },
      { BLUETOOTH_TYPE_HEADPHONES, "BLUETOOTH_TYPE_HEADPHONES", "headphones" },
      { BLUETOOTH_TYPE_OTHER_AUDIO, "BLUETOOTH_TYPE_OTHER_AUDIO", "other-audio" },
      { BLUETOOTH_TYPE_KEYBOARD, "BLUETOOTH_TYPE_KEYBOARD", "keyboard" },
      { BLUETOOTH_TYPE_MOUSE, "BLUETOOTH_TYPE_MOUSE", "mouse" },
      { BLUETOOTH_TYPE_CAMERA, "BLUETOOTH_TYPE_CAMERA", "camera" },
      { BLUETOOTH_TYPE_PRINTER, "BLUETOOTH_TYPE_PRINTER", "printer" },
      { BLUETOOTH_TYPE_JOYPAD, "BLUETOOTH_TYPE_JOYPAD", "joypad" },
      { BLUETOOTH_TYPE_TABLET, "BLUETOOTH_TYPE_TABLET", "tablet" },
      { BLUETOOTH_TYPE_VIDEO, "BLUETOOTH_TYPE_VIDEO", "video" },
      { 0, NULL, NULL }
    };
    etype = g_flags_register_static ("BluetoothType", values);
  }
  return etype;
}
GType
bluetooth_column_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { BLUETOOTH_COLUMN_PROXY, "BLUETOOTH_COLUMN_PROXY", "proxy" },
      { BLUETOOTH_COLUMN_ADDRESS, "BLUETOOTH_COLUMN_ADDRESS", "address" },
      { BLUETOOTH_COLUMN_ALIAS, "BLUETOOTH_COLUMN_ALIAS", "alias" },
      { BLUETOOTH_COLUMN_NAME, "BLUETOOTH_COLUMN_NAME", "name" },
      { BLUETOOTH_COLUMN_TYPE, "BLUETOOTH_COLUMN_TYPE", "type" },
      { BLUETOOTH_COLUMN_ICON, "BLUETOOTH_COLUMN_ICON", "icon" },
      { BLUETOOTH_COLUMN_DEFAULT, "BLUETOOTH_COLUMN_DEFAULT", "default" },
      { BLUETOOTH_COLUMN_PAIRED, "BLUETOOTH_COLUMN_PAIRED", "paired" },
      { BLUETOOTH_COLUMN_TRUSTED, "BLUETOOTH_COLUMN_TRUSTED", "trusted" },
      { BLUETOOTH_COLUMN_CONNECTED, "BLUETOOTH_COLUMN_CONNECTED", "connected" },
      { BLUETOOTH_COLUMN_DISCOVERABLE, "BLUETOOTH_COLUMN_DISCOVERABLE", "discoverable" },
      { BLUETOOTH_COLUMN_DISCOVERING, "BLUETOOTH_COLUMN_DISCOVERING", "discovering" },
      { BLUETOOTH_COLUMN_LEGACYPAIRING, "BLUETOOTH_COLUMN_LEGACYPAIRING", "legacypairing" },
      { BLUETOOTH_COLUMN_POWERED, "BLUETOOTH_COLUMN_POWERED", "powered" },
      { BLUETOOTH_COLUMN_SERVICES, "BLUETOOTH_COLUMN_SERVICES", "services" },
      { BLUETOOTH_COLUMN_UUIDS, "BLUETOOTH_COLUMN_UUIDS", "uuids" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("BluetoothColumn", values);
  }
  return etype;
}
GType
bluetooth_status_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { BLUETOOTH_STATUS_INVALID, "BLUETOOTH_STATUS_INVALID", "invalid" },
      { BLUETOOTH_STATUS_DISCONNECTED, "BLUETOOTH_STATUS_DISCONNECTED", "disconnected" },
      { BLUETOOTH_STATUS_CONNECTED, "BLUETOOTH_STATUS_CONNECTED", "connected" },
      { BLUETOOTH_STATUS_CONNECTING, "BLUETOOTH_STATUS_CONNECTING", "connecting" },
      { BLUETOOTH_STATUS_PLAYING, "BLUETOOTH_STATUS_PLAYING", "playing" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("BluetoothStatus", values);
  }
  return etype;
}



