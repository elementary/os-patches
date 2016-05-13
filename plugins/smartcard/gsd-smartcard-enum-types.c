
/* Generated data (by glib-mkenums) */


#include <glib-object.h>

#include "gsd-smartcard-manager.h"
/* enumerations from "gsd-smartcard-manager.h" */
GType gsd_smartcard_manager_error_get_type (void) G_GNUC_CONST;

GType
gsd_smartcard_manager_error_get_type (void)
{
 static GType etype = 0;

 if (G_UNLIKELY(etype == 0)) {
 static const GEnumValue values[] = {
 { GSD_SMARTCARD_MANAGER_ERROR_GENERIC, "GSD_SMARTCARD_MANAGER_ERROR_GENERIC", "generic" },
 { GSD_SMARTCARD_MANAGER_ERROR_WITH_NSS, "GSD_SMARTCARD_MANAGER_ERROR_WITH_NSS", "with-nss" },
 { GSD_SMARTCARD_MANAGER_ERROR_LOADING_DRIVER, "GSD_SMARTCARD_MANAGER_ERROR_LOADING_DRIVER", "loading-driver" },
 { GSD_SMARTCARD_MANAGER_ERROR_WATCHING_FOR_EVENTS, "GSD_SMARTCARD_MANAGER_ERROR_WATCHING_FOR_EVENTS", "watching-for-events" },
 { GSD_SMARTCARD_MANAGER_ERROR_REPORTING_EVENTS, "GSD_SMARTCARD_MANAGER_ERROR_REPORTING_EVENTS", "reporting-events" },
 { GSD_SMARTCARD_MANAGER_ERROR_FINDING_SMARTCARD, "GSD_SMARTCARD_MANAGER_ERROR_FINDING_SMARTCARD", "finding-smartcard" },
 { 0, NULL, NULL }
 };

 etype = g_enum_register_static (g_intern_static_string ("GsdSmartcardManagerError"), values);
 }

 return etype;
}

 /**/

/* Generated data ends here */

