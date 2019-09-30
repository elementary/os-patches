


#ifndef __GNOME_BLUETOOTH_ENUM_TYPES_H__
#define __GNOME_BLUETOOTH_ENUM_TYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS
/* enumerations from "bluetooth-enums.h" */
GType bluetooth_category_get_type (void);
#define BLUETOOTH_TYPE_CATEGORY (bluetooth_category_get_type())
GType bluetooth_type_get_type (void);
#define BLUETOOTH_TYPE_TYPE (bluetooth_type_get_type())
GType bluetooth_column_get_type (void);
#define BLUETOOTH_TYPE_COLUMN (bluetooth_column_get_type())
GType bluetooth_status_get_type (void);
#define BLUETOOTH_TYPE_STATUS (bluetooth_status_get_type())
G_END_DECLS

#endif /* __GNOME_BLUETOOTH_ENUM_TYPES_H__ */



