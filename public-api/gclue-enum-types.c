
/* Generated data (by glib-mkenums) */

#include "gclue-enum-types.h"
/* enumerations from "./gclue-enums.h" */
static const GEnumValue gclue_accuracy_level_values[] = {
    { GCLUE_ACCURACY_LEVEL_NONE, "GCLUE_ACCURACY_LEVEL_NONE", "none" },
    { GCLUE_ACCURACY_LEVEL_COUNTRY, "GCLUE_ACCURACY_LEVEL_COUNTRY", "country" },
    { GCLUE_ACCURACY_LEVEL_CITY, "GCLUE_ACCURACY_LEVEL_CITY", "city" },
    { GCLUE_ACCURACY_LEVEL_NEIGHBORHOOD, "GCLUE_ACCURACY_LEVEL_NEIGHBORHOOD", "neighborhood" },
    { GCLUE_ACCURACY_LEVEL_STREET, "GCLUE_ACCURACY_LEVEL_STREET", "street" },
    { GCLUE_ACCURACY_LEVEL_EXACT, "GCLUE_ACCURACY_LEVEL_EXACT", "exact" },
    { 0, NULL, NULL }
};

/* Define type-specific symbols */
#undef __GCLUE_IS_ENUM__
#undef __GCLUE_IS_FLAGS__
#define __GCLUE_IS_ENUM__

GType
gclue_accuracy_level_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile)) {
        GType g_define_type_id =
            g_enum_register_static (g_intern_static_string ("GClueAccuracyLevel"),
                                      gclue_accuracy_level_values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/**
 * gclue_accuracy_level_get_string:
 * @val: a GClueAccuracyLevel.
 *
 * Gets the nickname string for the #GClueAccuracyLevel specified at @val.
 *
 * Returns: (transfer none): a string with the nickname, or %NULL if not found. Do not free the returned value.
 */
#if defined __GCLUE_IS_ENUM__
const gchar *
gclue_accuracy_level_get_string (GClueAccuracyLevel val)
{
    guint i;

    for (i = 0; gclue_accuracy_level_values[i].value_nick; i++) {
        if (val == gclue_accuracy_level_values[i].value)
            return gclue_accuracy_level_values[i].value_nick;
    }

    return NULL;
}
#endif /* __GCLUE_IS_ENUM_ */

/**
 * gclue_accuracy_level_build_string_from_mask:
 * @mask: bitmask of GClueAccuracyLevel values.
 *
 * Builds a string containing a comma-separated list of nicknames for
 * each #GClueAccuracyLevel in @mask.
 *
 * Returns: (transfer full): a string with the list of nicknames, or %NULL if none given. The returned value should be freed with g_free().
 */
#if defined __GCLUE_IS_FLAGS__
gchar *
gclue_accuracy_level_build_string_from_mask (GClueAccuracyLevel mask)
{
    guint i;
    gboolean first = TRUE;
    GString *str = NULL;

    for (i = 0; gclue_accuracy_level_values[i].value_nick; i++) {
        /* We also look for exact matches */
        if (mask == gclue_accuracy_level_values[i].value) {
            if (str)
                g_string_free (str, TRUE);
            return g_strdup (gclue_accuracy_level_values[i].value_nick);
        }

        /* Build list with single-bit masks */
        if (mask & gclue_accuracy_level_values[i].value) {
            guint c;
            gulong number = gclue_accuracy_level_values[i].value;

            for (c = 0; number; c++)
                number &= number - 1;

            if (c == 1) {
                if (!str)
                    str = g_string_new ("");
                g_string_append_printf (str, "%s%s",
                                        first ? "" : ", ",
                                        gclue_accuracy_level_values[i].value_nick);
                if (first)
                    first = FALSE;
            }
        }
    }

    return (str ? g_string_free (str, FALSE) : NULL);
}
#endif /* __GCLUE_IS_FLAGS__ */


/* Generated data ends here */

