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
#include "gclue-modem.h"
#include "gclue-marshal.h"

/**
 * SECTION:gclue-modem
 * @short_description: Modem handler
 *
 * This interface is implemented by Modem handing modules. Currently there is
 * only one class, ModemManager implementing this interface. Normally it
 * wouldn't make sense to have it separate from the class but intention is to
 * make it very easy to add alternative modem sources, e.g ofono-based.
 **/

G_DEFINE_INTERFACE (GClueModem, gclue_modem, 0);

static void
gclue_modem_default_init (GClueModemInterface *iface)
{
        GParamSpec *spec;

        spec = g_param_spec_boolean ("is-3g-available",
                                     "Is3GAvailable",
                                     "Is 3G Available?",
                                     FALSE,
                                     G_PARAM_READABLE);
        g_object_interface_install_property (iface, spec);

        spec = g_param_spec_boolean ("is-cdma-available",
                                     "IsCDMAAvailable",
                                     "Is CDMA Available?",
                                     FALSE,
                                     G_PARAM_READABLE);
        g_object_interface_install_property (iface, spec);

        spec = g_param_spec_boolean ("is-gps-available",
                                     "IsGPSAvailable",
                                     "Is GPS Available?",
                                     FALSE,
                                     G_PARAM_READABLE);
        g_object_interface_install_property (iface, spec);

        g_signal_new ("fix-3g",
                      GCLUE_TYPE_MODEM,
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL,
                      NULL,
                      gclue_marshal_VOID__UINT_UINT_ULONG_ULONG,
                      G_TYPE_NONE,
                      4,
                      G_TYPE_UINT,
                      G_TYPE_UINT,
                      G_TYPE_ULONG,
                      G_TYPE_ULONG);

        g_signal_new ("fix-cdma",
                      GCLUE_TYPE_MODEM,
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL,
                      NULL,
                      gclue_marshal_VOID__DOUBLE_DOUBLE,
                      G_TYPE_NONE,
                      2,
                      G_TYPE_DOUBLE,
                      G_TYPE_DOUBLE);

        g_signal_new ("fix-gps",
                      GCLUE_TYPE_MODEM,
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);
}

gboolean
gclue_modem_get_is_3g_available (GClueModem *modem)
{
        g_return_val_if_fail (GCLUE_IS_MODEM (modem), FALSE);

        return GCLUE_MODEM_GET_INTERFACE (modem)->get_is_3g_available (modem);
}

gboolean
gclue_modem_get_is_cdma_available (GClueModem *modem)
{
        g_return_val_if_fail (GCLUE_IS_MODEM (modem), FALSE);

        return GCLUE_MODEM_GET_INTERFACE (modem)->get_is_cdma_available (modem);
}

gboolean
gclue_modem_get_is_gps_available (GClueModem *modem)
{
        g_return_val_if_fail (GCLUE_IS_MODEM (modem), FALSE);

        return GCLUE_MODEM_GET_INTERFACE (modem)->get_is_gps_available (modem);
}

void
gclue_modem_enable_3g (GClueModem         *modem,
                       GCancellable       *cancellable,
                       GAsyncReadyCallback callback,
                       gpointer            user_data)
{
        g_return_if_fail (GCLUE_IS_MODEM (modem));
        g_return_if_fail (gclue_modem_get_is_3g_available (modem));

        GCLUE_MODEM_GET_INTERFACE (modem)->enable_3g (modem,
                                                      cancellable,
                                                      callback,
                                                      user_data);
}

gboolean
gclue_modem_enable_3g_finish (GClueModem   *modem,
                              GAsyncResult *result,
                              GError      **error)
{
        g_return_val_if_fail (GCLUE_IS_MODEM (modem), FALSE);

        return GCLUE_MODEM_GET_INTERFACE (modem)->enable_3g_finish (modem,
                                                                    result,
                                                                    error);
}

void
gclue_modem_enable_cdma (GClueModem         *modem,
                         GCancellable       *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer            user_data)
{
        g_return_if_fail (GCLUE_IS_MODEM (modem));
        g_return_if_fail (gclue_modem_get_is_cdma_available (modem));

        GCLUE_MODEM_GET_INTERFACE (modem)->enable_cdma (modem,
                                                        cancellable,
                                                        callback,
                                                        user_data);
}

gboolean
gclue_modem_enable_cdma_finish (GClueModem   *modem,
                                GAsyncResult *result,
                                GError      **error)
{
        g_return_val_if_fail (GCLUE_IS_MODEM (modem), FALSE);

        return GCLUE_MODEM_GET_INTERFACE (modem)->enable_cdma_finish (modem,
                                                                      result,
                                                                      error);
}

void
gclue_modem_enable_gps (GClueModem         *modem,
                        GCancellable       *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer            user_data)
{
        g_return_if_fail (GCLUE_IS_MODEM (modem));
        g_return_if_fail (gclue_modem_get_is_gps_available (modem));

        GCLUE_MODEM_GET_INTERFACE (modem)->enable_gps (modem,
                                                       cancellable,
                                                       callback,
                                                       user_data);
}

gboolean
gclue_modem_enable_gps_finish (GClueModem   *modem,
                               GAsyncResult *result,
                               GError      **error)
{
        g_return_val_if_fail (GCLUE_IS_MODEM (modem), FALSE);

        return GCLUE_MODEM_GET_INTERFACE (modem)->enable_gps_finish (modem,
                                                                     result,
                                                                     error);
}

gboolean
gclue_modem_disable_3g (GClueModem   *modem,
                        GCancellable *cancellable,
                        GError      **error)
{
        g_return_val_if_fail (GCLUE_IS_MODEM (modem), FALSE);
        g_return_val_if_fail (gclue_modem_get_is_3g_available (modem), FALSE);

        return GCLUE_MODEM_GET_INTERFACE (modem)->disable_3g (modem,
                                                              cancellable,
                                                              error);
}

gboolean
gclue_modem_disable_cdma (GClueModem   *modem,
                          GCancellable *cancellable,
                          GError      **error)
{
        g_return_val_if_fail (GCLUE_IS_MODEM (modem), FALSE);
        g_return_val_if_fail (gclue_modem_get_is_cdma_available (modem), FALSE);

        return GCLUE_MODEM_GET_INTERFACE (modem)->disable_cdma (modem,
                                                                cancellable,
                                                                error);
}

gboolean
gclue_modem_disable_gps (GClueModem   *modem,
                         GCancellable *cancellable,
                         GError      **error)
{
        g_return_val_if_fail (GCLUE_IS_MODEM (modem), FALSE);
        g_return_val_if_fail (gclue_modem_get_is_gps_available (modem), FALSE);

        return GCLUE_MODEM_GET_INTERFACE (modem)->disable_gps (modem,
                                                               cancellable,
                                                               error);
}
