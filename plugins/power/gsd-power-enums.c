


#include "gsm-inhibitor-flag.h"
#include "gsm-presence-flag.h"
#include "gsd-power-enums.h"

/* enumerations from "./gsm-inhibitor-flag.h" */
GType
gsm_inhibitor_flag_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GFlagsValue values[] = {
      { GSM_INHIBITOR_FLAG_LOGOUT, "GSM_INHIBITOR_FLAG_LOGOUT", "logout" },
      { GSM_INHIBITOR_FLAG_SWITCH_USER, "GSM_INHIBITOR_FLAG_SWITCH_USER", "switch-user" },
      { GSM_INHIBITOR_FLAG_SUSPEND, "GSM_INHIBITOR_FLAG_SUSPEND", "suspend" },
      { GSM_INHIBITOR_FLAG_IDLE, "GSM_INHIBITOR_FLAG_IDLE", "idle" },
      { GSM_INHIBITOR_FLAG_AUTOMOUNT, "GSM_INHIBITOR_FLAG_AUTOMOUNT", "automount" },
      { 0, NULL, NULL }
    };
    etype = g_flags_register_static ("GsmInhibitorFlag", values);
  }
  return etype;
}

/* enumerations from "./gsm-presence-flag.h" */
GType
gsm_presence_status_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { GSM_PRESENCE_STATUS_AVAILABLE, "GSM_PRESENCE_STATUS_AVAILABLE", "available" },
      { GSM_PRESENCE_STATUS_INVISIBLE, "GSM_PRESENCE_STATUS_INVISIBLE", "invisible" },
      { GSM_PRESENCE_STATUS_BUSY, "GSM_PRESENCE_STATUS_BUSY", "busy" },
      { GSM_PRESENCE_STATUS_IDLE, "GSM_PRESENCE_STATUS_IDLE", "idle" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("GsmPresenceStatus", values);
  }
  return etype;
}



