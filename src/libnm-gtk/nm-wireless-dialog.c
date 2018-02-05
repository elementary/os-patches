/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
 *
 * Dan Williams <dcbw@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2007 - 2012 Red Hat, Inc.
 */

#include "nm-default.h"

#include <nm-client.h>

#include "nm-wireless-dialog.h"
#include "nm-wifi-dialog.h"

GType
nma_wireless_dialog_get_type (void)
{
	return nma_wifi_dialog_get_type ();
}

NMConnection *
nma_wireless_dialog_get_connection (NMAWirelessDialog *self,
                                    NMDevice **out_device,
                                    NMAccessPoint **ap)
{
	return nma_wifi_dialog_get_connection ((NMAWifiDialog *)self, out_device, ap);
}

GtkWidget *
nma_wireless_dialog_new (NMClient *client,
                         NMRemoteSettings *settings,
                         NMConnection *connection,
                         NMDevice *device,
                         NMAccessPoint *ap,
                         gboolean secrets_only)
{
	return nma_wifi_dialog_new (client, settings, connection, device, ap, secrets_only);
}

GtkWidget *
nma_wireless_dialog_new_for_other (NMClient *client, NMRemoteSettings *settings)
{
	return nma_wifi_dialog_new_for_hidden (client, settings);
}

GtkWidget *
nma_wireless_dialog_new_for_create (NMClient *client, NMRemoteSettings *settings)
{
	return nma_wifi_dialog_new_for_create (client, settings);
}

