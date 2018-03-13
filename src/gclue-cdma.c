/* vim: set et ts=8 sw=8: */
/*
 * Copyright (C) 2014 Red Hat, Inc.
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
 * Authors: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 */

#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include "gclue-cdma.h"
#include "gclue-modem-manager.h"
#include "gclue-location.h"

/**
 * SECTION:gclue-cdma
 * @short_description: WiFi-based geolocation
 * @include: gclue-glib/gclue-cdma.h
 *
 * Contains functions to get the geolocation from a CDMA cell tower.
 **/

struct _GClueCDMAPrivate {
        GClueModem *modem;

        GCancellable *cancellable;

        gulong cdma_notify_id;
};


G_DEFINE_TYPE (GClueCDMA, gclue_cdma, GCLUE_TYPE_LOCATION_SOURCE)

static gboolean
gclue_cdma_start (GClueLocationSource *source);
static gboolean
gclue_cdma_stop (GClueLocationSource *source);

static void
refresh_accuracy_level (GClueCDMA *source)
{
        GClueAccuracyLevel new, existing;

        existing = gclue_location_source_get_available_accuracy_level
                        (GCLUE_LOCATION_SOURCE (source));

        if (gclue_modem_get_is_cdma_available (source->priv->modem))
                new = GCLUE_ACCURACY_LEVEL_NEIGHBORHOOD;
        else
                new = GCLUE_ACCURACY_LEVEL_NONE;

        if (new != existing) {
                g_debug ("Available accuracy level from %s: %u",
                         G_OBJECT_TYPE_NAME (source), new);
                g_object_set (G_OBJECT (source),
                              "available-accuracy-level", new,
                              NULL);
        }
}

static void
on_cdma_enabled (GObject      *source_object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
        GClueCDMA *source = GCLUE_CDMA (user_data);
        GError *error = NULL;

        if (!gclue_modem_enable_cdma_finish (source->priv->modem,
                                             result,
                                             &error)) {
                g_warning ("Failed to enable CDMA: %s", error->message);
                g_error_free (error);
        }
}

static void
on_is_cdma_available_notify (GObject    *gobject,
                             GParamSpec *pspec,
                             gpointer    user_data)
{
        GClueCDMA *source = GCLUE_CDMA (user_data);
        GClueCDMAPrivate *priv = source->priv;

        refresh_accuracy_level (source);

        if (gclue_location_source_get_active (GCLUE_LOCATION_SOURCE (source)) &&
            gclue_modem_get_is_cdma_available (priv->modem))
                gclue_modem_enable_cdma (priv->modem,
                                       priv->cancellable,
                                       on_cdma_enabled,
                                       source);
}

static void
gclue_cdma_finalize (GObject *gcdma)
{
        GClueCDMAPrivate *priv = GCLUE_CDMA (gcdma)->priv;

        G_OBJECT_CLASS (gclue_cdma_parent_class)->finalize (gcdma);

        g_signal_handler_disconnect (priv->modem,
                                     priv->cdma_notify_id);
        priv->cdma_notify_id = 0;

        g_cancellable_cancel (priv->cancellable);
        g_clear_object (&priv->cancellable);
        g_clear_object (&priv->modem);
}

static void
gclue_cdma_class_init (GClueCDMAClass *klass)
{
        GClueLocationSourceClass *source_class = GCLUE_LOCATION_SOURCE_CLASS (klass);
        GObjectClass *gcdma_class = G_OBJECT_CLASS (klass);

        gcdma_class->finalize = gclue_cdma_finalize;

        source_class->start = gclue_cdma_start;
        source_class->stop = gclue_cdma_stop;

        g_type_class_add_private (klass, sizeof (GClueCDMAPrivate));
}

static void
gclue_cdma_init (GClueCDMA *source)
{
        GClueCDMAPrivate *priv;

        source->priv = G_TYPE_INSTANCE_GET_PRIVATE ((source), GCLUE_TYPE_CDMA, GClueCDMAPrivate);
        priv = source->priv;

        priv->cancellable = g_cancellable_new ();

        priv->modem = gclue_modem_manager_get_singleton ();
        priv->cdma_notify_id =
                        g_signal_connect (priv->modem,
                                          "notify::is-cdma-available",
                                          G_CALLBACK (on_is_cdma_available_notify),
                                          source);
}

static void
on_cdma_destroyed (gpointer data,
                 GObject *where_the_object_was)
{
        GClueCDMA **source = (GClueCDMA **) data;

        *source = NULL;
}

/**
 * gclue_cdma_get_singleton:
 *
 * Get the #GClueCDMA singleton.
 *
 * Returns: (transfer full): a new ref to #GClueCDMA. Use g_object_unref()
 * when done.
 **/
GClueCDMA *
gclue_cdma_get_singleton (void)
{
        static GClueCDMA *source = NULL;

        if (source == NULL) {
                source = g_object_new (GCLUE_TYPE_CDMA, NULL);
                g_object_weak_ref (G_OBJECT (source),
                                   on_cdma_destroyed,
                                   &source);
        } else
                g_object_ref (source);

        return source;
}

static void
on_fix_cdma (GClueModem *modem,
             gdouble     latitude,
             gdouble     longitude,
             gpointer    user_data)
{
        GClueLocation *location;

        location = gclue_location_new (latitude,
                                       longitude,
                                       1000);     /* Assume 1 km accuracy */

        gclue_location_source_set_location (GCLUE_LOCATION_SOURCE (user_data),
                                            location);
        g_object_unref (location);
}

static gboolean
gclue_cdma_start (GClueLocationSource *source)
{
        GClueLocationSourceClass *base_class;
        GClueCDMAPrivate *priv;

        g_return_val_if_fail (GCLUE_IS_LOCATION_SOURCE (source), FALSE);
        priv = GCLUE_CDMA (source)->priv;

        base_class = GCLUE_LOCATION_SOURCE_CLASS (gclue_cdma_parent_class);
        if (!base_class->start (source))
                return FALSE;

        g_signal_connect (priv->modem,
                          "fix-cdma",
                          G_CALLBACK (on_fix_cdma),
                          source);

        if (gclue_modem_get_is_cdma_available (priv->modem))
                gclue_modem_enable_cdma (priv->modem,
                                         priv->cancellable,
                                         on_cdma_enabled,
                                         source);

        return TRUE;
}

static gboolean
gclue_cdma_stop (GClueLocationSource *source)
{
        GClueCDMAPrivate *priv = GCLUE_CDMA (source)->priv;
        GClueLocationSourceClass *base_class;
        GError *error = NULL;

        g_return_val_if_fail (GCLUE_IS_LOCATION_SOURCE (source), FALSE);

        base_class = GCLUE_LOCATION_SOURCE_CLASS (gclue_cdma_parent_class);
        if (!base_class->stop (source))
                return FALSE;

        g_signal_handlers_disconnect_by_func (G_OBJECT (priv->modem),
                                              G_CALLBACK (on_fix_cdma),
                                              source);

        if (gclue_modem_get_is_cdma_available (priv->modem))
                if (!gclue_modem_disable_cdma (priv->modem,
                                               priv->cancellable,
                                               &error)) {
                        g_warning ("Failed to disable CDMA: %s",
                                   error->message);
                        g_error_free (error);
                }

        return TRUE;
}
