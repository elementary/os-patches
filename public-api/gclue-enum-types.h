
/* Generated data (by glib-mkenums) */

#include "gclue-enums.h"
#ifndef __GCLUE_ENUM_TYPES_H__
#define __GCLUE_ENUM_TYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* enumerations from "./gclue-enums.h" */
GType gclue_accuracy_level_get_type (void) G_GNUC_CONST;
#define GCLUE_TYPE_ACCURACY_LEVEL (gclue_accuracy_level_get_type ())

/* Define type-specific symbols */
#undef __GCLUE_IS_ENUM__
#undef __GCLUE_IS_FLAGS__
#define __GCLUE_IS_ENUM__

#if defined __GCLUE_IS_ENUM__
const gchar *gclue_accuracy_level_get_string (GClueAccuracyLevel val);
#endif

#if defined __GCLUE_IS_FLAGS__
gchar *gclue_accuracy_level_build_string_from_mask (GClueAccuracyLevel mask);
#endif

G_END_DECLS

#endif /* __GCLUE_ENUM_TYPES_H__ */

/* Generated data ends here */

