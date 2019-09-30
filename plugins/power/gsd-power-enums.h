


#ifndef GSD_POWER_ENUMS_H
#define GSD_POWER_ENUMS_H

#include <glib-object.h>

G_BEGIN_DECLS
/* enumerations from "./gsm-inhibitor-flag.h" */
GType gsm_inhibitor_flag_get_type (void) G_GNUC_CONST;
#define GSD_POWER_TYPE_INHIBITOR_FLAG (gsm_inhibitor_flag_get_type())
/* enumerations from "./gsm-presence-flag.h" */
GType gsm_presence_status_get_type (void) G_GNUC_CONST;
#define GSD_POWER_TYPE_PRESENCE_STATUS (gsm_presence_status_get_type())
G_END_DECLS

#endif /* !GSD_POWER_ENUMS_H */



