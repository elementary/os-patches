/* vim: set et ts=8 sw=8: */
/*
 * Copyright 2014 Red Hat, Inc.
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
#include <libsoup/soup.h>
#include <string.h>
#include "gclue-3g.h"
#include "gclue-modem-manager.h"
#include "gclue-location.h"
#include "gclue-mozilla.h"

/**
 * SECTION:gclue-3g
 * @short_description: 3GPP-based geolocation
 *
 * Contains functions to get the geolocation based on 3GPP cell towers.
 **/

struct _GClue3GPrivate {
        GClueModem *modem;

        GCancellable *cancellable;

        gulong threeg_notify_id;

        GClue3GTower *tower;
};

G_DEFINE_TYPE_WITH_CODE (GClue3G,
                         gclue_3g,
                         GCLUE_TYPE_WEB_SOURCE,
                         G_ADD_PRIVATE (GClue3G))

static gboolean
gclue_3g_start (GClueLocationSource *source);
static gboolean
gclue_3g_stop (GClueLocationSource *source);
static SoupMessage *
gclue_3g_create_query (GClueWebSource *web,
                       GError        **error);
static SoupMessage *
gclue_3g_create_submit_query (GClueWebSource  *web,
                              GClueLocation   *location,
                              GError         **error);
static GClueAccuracyLevel
gclue_3g_get_available_accuracy_level (GClueWebSource *web,
                                       gboolean available);
static GClueLocation *
gclue_3g_parse_response (GClueWebSource *web,
                         const char     *xml,
                         GError        **error);

static void
on_3g_enabled (GObject      *source_object,
               GAsyncResult *result,
               gpointer      user_data)
{
        GClue3G *source = GCLUE_3G (user_data);
        GError *error = NULL;

        if (!gclue_modem_enable_3g_finish (source->priv->modem,
                                           result,
                                           &error)) {
                g_warning ("Failed to enable 3GPP: %s", error->message);
                g_error_free (error);
        }
}

static void
on_is_3g_available_notify (GObject    *gobject,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
        GClue3G *source = GCLUE_3G (user_data);
        GClue3GPrivate *priv = source->priv;

        gclue_web_source_refresh (GCLUE_WEB_SOURCE (source));

        if (gclue_location_source_get_active (GCLUE_LOCATION_SOURCE (source)) &&
            gclue_modem_get_is_3g_available (priv->modem))
                gclue_modem_enable_3g (priv->modem,
                                       priv->cancellable,
                                       on_3g_enabled,
                                       source);
}

static GClueLocation *
gclue_3g_parse_response (GClueWebSource *web,
                         const char     *content,
                         GError        **error)
{
        return gclue_mozilla_parse_response (content, error);
}

static void
gclue_3g_finalize (GObject *g3g)
{
        GClue3G *source = (GClue3G *) g3g;
        GClue3GPrivate *priv = source->priv;

        G_OBJECT_CLASS (gclue_3g_parent_class)->finalize (g3g);

        g_cancellable_cancel (priv->cancellable);

        g_signal_handler_disconnect (priv->modem,
                                     priv->threeg_notify_id);
        priv->threeg_notify_id = 0;

        g_clear_object (&priv->modem);
        g_clear_object (&priv->cancellable);
}

static void
gclue_3g_class_init (GClue3GClass *klass)
{
        GClueWebSourceClass *web_class = GCLUE_WEB_SOURCE_CLASS (klass);
        GClueLocationSourceClass *source_class = GCLUE_LOCATION_SOURCE_CLASS (klass);
        GObjectClass *g3g_class = G_OBJECT_CLASS (klass);

        g3g_class->finalize = gclue_3g_finalize;

        source_class->start = gclue_3g_start;
        source_class->stop = gclue_3g_stop;
        web_class->create_query = gclue_3g_create_query;
        web_class->create_submit_query = gclue_3g_create_submit_query;
        web_class->parse_response = gclue_3g_parse_response;
        web_class->get_available_accuracy_level =
                gclue_3g_get_available_accuracy_level;
}

static void
gclue_3g_init (GClue3G *source)
{
        GClue3GPrivate *priv;

        source->priv = G_TYPE_INSTANCE_GET_PRIVATE ((source), GCLUE_TYPE_3G, GClue3GPrivate);
        priv = source->priv;

        priv->cancellable = g_cancellable_new ();

        priv->modem = gclue_modem_manager_get_singleton ();
        priv->threeg_notify_id =
                        g_signal_connect (priv->modem,
                                          "notify::is-3g-available",
                                          G_CALLBACK (on_is_3g_available_notify),
                                          source);
}

static void
on_3g_destroyed (gpointer data,
                 GObject *where_the_object_was)
{
        GClue3G **source = (GClue3G **) data;

        *source = NULL;
}

/**
 * gclue_3g_new:
 *
 * Get the #GClue3G singleton.
 *
 * Returns: (transfer full): a new ref to #GClue3G. Use g_object_unref()
 * when done.
 **/
GClue3G *
gclue_3g_get_singleton (void)
{
        static GClue3G *source = NULL;

        if (source == NULL) {
                source = g_object_new (GCLUE_TYPE_3G, NULL);
                g_object_weak_ref (G_OBJECT (source),
                                   on_3g_destroyed,
                                   &source);
        } else
                g_object_ref (source);

        return source;
}

static SoupMessage *
gclue_3g_create_query (GClueWebSource *web,
                       GError        **error)
{
        GClue3GPrivate *priv = GCLUE_3G (web)->priv;

        if (priv->tower == NULL) {
                g_set_error_literal (error,
                                     G_IO_ERROR,
                                     G_IO_ERROR_NOT_INITIALIZED,
                                     "3GPP cell tower info unavailable");
                return NULL; /* Not initialized yet */
        }

        return gclue_mozilla_create_query (NULL, priv->tower, error);
}

static SoupMessage *
gclue_3g_create_submit_query (GClueWebSource  *web,
                              GClueLocation   *location,
                              GError         **error)
{
        GClue3GPrivate *priv = GCLUE_3G (web)->priv;

        if (priv->tower == NULL) {
                g_set_error_literal (error,
                                     G_IO_ERROR,
                                     G_IO_ERROR_NOT_INITIALIZED,
                                     "3GPP cell tower info unavailable");
                return NULL; /* Not initialized yet */
        }

        return gclue_mozilla_create_submit_query (location,
                                                  NULL,
                                                  priv->tower,
                                                  error);
}

static GClueAccuracyLevel
gclue_3g_get_available_accuracy_level (GClueWebSource *web,
                                       gboolean        network_available)
{
        if (gclue_modem_get_is_3g_available (GCLUE_3G (web)->priv->modem) &&
            network_available)
                return GCLUE_ACCURACY_LEVEL_NEIGHBORHOOD;
        else
                return GCLUE_ACCURACY_LEVEL_NONE;
}

static void
on_fix_3g (GClueModem *modem,
           guint       mcc,
           guint       mnc,
           gulong      lac,
           gulong      cell_id,
           gpointer    user_data)
{
        GClue3GPrivate *priv = GCLUE_3G (user_data)->priv;

        if (priv->tower == NULL)
                priv->tower = g_slice_new0 (GClue3GTower);
        priv->tower->mcc = mcc;
        priv->tower->mnc = mnc;
        priv->tower->lac = lac;
        priv->tower->cell_id = cell_id;

        gclue_web_source_refresh (GCLUE_WEB_SOURCE (user_data));
}

static gboolean
gclue_3g_start (GClueLocationSource *source)
{
        GClueLocationSourceClass *base_class;
        GClue3GPrivate *priv;

        g_return_val_if_fail (GCLUE_IS_LOCATION_SOURCE (source), FALSE);
        priv = GCLUE_3G (source)->priv;

        base_class = GCLUE_LOCATION_SOURCE_CLASS (gclue_3g_parent_class);
        if (!base_class->start (source))
                return FALSE;

        if (priv->tower != NULL) {
                g_slice_free (GClue3GTower, priv->tower);
                priv->tower = NULL;
        }

        g_signal_connect (priv->modem,
                          "fix-3g",
                          G_CALLBACK (on_fix_3g),
                          source);

        if (gclue_modem_get_is_3g_available (priv->modem))
                gclue_modem_enable_3g (priv->modem,
                                       priv->cancellable,
                                       on_3g_enabled,
                                       source);
        return TRUE;
}

static gboolean
gclue_3g_stop (GClueLocationSource *source)
{
        GClue3GPrivate *priv = GCLUE_3G (source)->priv;
        GClueLocationSourceClass *base_class;
        GError *error = NULL;

        g_return_val_if_fail (GCLUE_IS_LOCATION_SOURCE (source), FALSE);

        base_class = GCLUE_LOCATION_SOURCE_CLASS (gclue_3g_parent_class);
        if (!base_class->stop (source))
                return FALSE;

        g_signal_handlers_disconnect_by_func (G_OBJECT (priv->modem),
                                              G_CALLBACK (on_fix_3g),
                                              source);

        if (gclue_modem_get_is_3g_available (priv->modem))
                if (!gclue_modem_disable_3g (priv->modem,
                                             priv->cancellable,
                                             &error)) {
                        g_warning ("Failed to disable 3GPP: %s",
                                   error->message);
                        g_error_free (error);
                }

        return TRUE;
}
