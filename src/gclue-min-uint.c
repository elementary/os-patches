/* vim: set et ts=8 sw=8: */
/* gclue-min-uint.c
 *
 * Copyright 2018 Collabora Ltd.
 *
 * Geoclue is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Geoclue is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Geoclue; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 */

#include "gclue-min-uint.h"

/**
 * SECTION:gclue-min-uint
 * @short_description: Easy way to keep track of minimum of a bunch of values
 * @include: gclue-glib/gclue-location-source.h
 *
 * This is a helper class that keeps a list of guint values and the minimum
 * value from this list. It is used by location sources to use the minimum
 * time-threshold (location update rate) from all the time-thresholds requested
 * by different applications.
 **/

struct _GClueMinUINTPrivate
{
        GHashTable *all_values;

        gboolean notify_value;
};

G_DEFINE_TYPE_WITH_CODE (GClueMinUINT,
                         gclue_min_uint,
                         G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GClueMinUINT))

enum
{
        PROP_0,
        PROP_VALUE,
        LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

typedef struct
{
        GClueMinUINT *muint;
        GObject *owner;
} OwnerData;

static gboolean
on_owner_weak_ref_notify_defered (OwnerData *data)
{
        gclue_min_uint_drop_value (data->muint, data->owner);
        g_object_unref (data->muint);
        g_slice_free (OwnerData, data);

        return FALSE;
}

static void
on_owner_weak_ref_notify (gpointer data, GObject *object)
{
        OwnerData *owner_data = g_slice_new (OwnerData);
        owner_data->muint = GCLUE_MIN_UINT (data);
        g_object_ref (owner_data->muint);
        owner_data->owner = object;

        // Let's ensure owner is really gone before we drop its value
        g_idle_add ((GSourceFunc) on_owner_weak_ref_notify_defered, owner_data);
}

static void
gclue_min_uint_finalize (GObject *object)
{
        g_clear_pointer (&GCLUE_MIN_UINT (object)->priv->all_values,
                         g_hash_table_unref);

        /* Chain up to the parent class */
        G_OBJECT_CLASS (gclue_min_uint_parent_class)->finalize (object);
}

static void
gclue_min_uint_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
        GClueMinUINT *muint = GCLUE_MIN_UINT (object);

        switch (prop_id) {
        case PROP_VALUE:
                g_value_set_uint (value, gclue_min_uint_get_value (muint));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_min_uint_class_init (GClueMinUINTClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gclue_min_uint_finalize;
        object_class->get_property = gclue_min_uint_get_property;

        gParamSpecs[PROP_VALUE] = g_param_spec_uint ("value",
                                                     "Value",
                                                     "The minimum value",
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE);
        g_object_class_install_property (object_class,
                                         PROP_VALUE,
                                         gParamSpecs[PROP_VALUE]);
}

static void
gclue_min_uint_init (GClueMinUINT *muint)
{
        muint->priv = G_TYPE_INSTANCE_GET_PRIVATE (muint,
                                                   GCLUE_TYPE_MIN_UINT,
                                                   GClueMinUINTPrivate);
        muint->priv->all_values = g_hash_table_new (g_direct_hash,
                                                    g_direct_equal);
        muint->priv->notify_value = TRUE;
}

/**
 * gclue_min_uint_new
 *
 * Returns: A new #GClueMinUINT instance.
 **/
GClueMinUINT *
gclue_min_uint_new (void)
{
        return g_object_new (GCLUE_TYPE_MIN_UINT, NULL);
}

/**
 * gclue_min_uint_get_value
 * @muint: a #GClueMinUINT
 *
 * Returns: The current minimum value from the list.
 **/
guint
gclue_min_uint_get_value (GClueMinUINT *muint)
{
        guint value;
        GList *values, *l;

        g_return_val_if_fail (GCLUE_IS_MIN_UINT(muint), 0);

        if (g_hash_table_size (muint->priv->all_values) == 0)
                return 0;

        values = g_hash_table_get_values (muint->priv->all_values);
        value = GPOINTER_TO_UINT (values->data);

        for (l = values->next; l; l = l->next) {
                guint i = GPOINTER_TO_UINT (l->data);

                if (value > i) {
                        value = i;
                }
        }

        g_list_free (values);

        return value;
}

/**
 * gclue_min_uint_add_value
 * @muint: a #GClueMinUINT
 * @value: A value to add to the list
 * @owner: the object adding this value
 *
 * If @owner has already added a value previously, this call will simply replace
 * that. i-e Each object can only add one value at a time.
 **/
void
gclue_min_uint_add_value (GClueMinUINT *muint,
                          guint         value,
                          GObject      *owner)
{
        g_return_if_fail (GCLUE_IS_MIN_UINT(muint));

        g_hash_table_replace (muint->priv->all_values,
                              owner,
                              GUINT_TO_POINTER (value));
        g_object_weak_ref (owner, on_owner_weak_ref_notify, muint);

        g_object_notify_by_pspec (G_OBJECT (muint), gParamSpecs[PROP_VALUE]);
}

/**
 * gclue_min_uint_drop_value
 * @muint: a #GClueMinUINT
 * @owner: the object that adadded a value previously
 **/
void
gclue_min_uint_drop_value (GClueMinUINT *muint,
                           GObject      *owner)
{
        g_return_if_fail (GCLUE_IS_MIN_UINT(muint));

        if (!g_hash_table_remove (muint->priv->all_values, owner)) {
                return;
        }

        g_object_notify_by_pspec (G_OBJECT (muint), gParamSpecs[PROP_VALUE]);
}
