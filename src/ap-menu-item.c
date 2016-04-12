/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ap-menu-item.c - Class to represent a Wifi access point 
 *
 * Jonathan Blandford <jrb@redhat.com>
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
 * Copyright 2005 - 2014 Red Hat, Inc.
 */

#include "nm-default.h"

#include <stdio.h>
#include <string.h>

#include <NetworkManager.h>

#include "ap-menu-item.h"
#include "nm-access-point.h"
#include "mobile-helpers.h"


G_DEFINE_TYPE (NMNetworkMenuItem, nm_network_menu_item, GTK_TYPE_MENU_ITEM);

#define NM_NETWORK_MENU_ITEM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_NETWORK_MENU_ITEM, NMNetworkMenuItemPrivate))

typedef struct {
	GtkWidget * ssid;
	GtkWidget * strength;
	GtkWidget * hbox;

	char *      ssid_string;
	guint32     int_strength;
	gchar *     hash;
	GSList *    dupes;
	gboolean    has_connections;
	gboolean    is_adhoc;
	gboolean    is_encrypted;
} NMNetworkMenuItemPrivate;

/******************************************************************/

const char *
nm_network_menu_item_get_ssid (NMNetworkMenuItem *item)
{
	g_return_val_if_fail (NM_IS_NETWORK_MENU_ITEM (item), NULL);

	return NM_NETWORK_MENU_ITEM_GET_PRIVATE (item)->ssid_string;
}

guint32
nm_network_menu_item_get_strength (NMNetworkMenuItem *item)
{
	g_return_val_if_fail (NM_IS_NETWORK_MENU_ITEM (item), 0);

	return NM_NETWORK_MENU_ITEM_GET_PRIVATE (item)->int_strength;
}

static void
update_atk_desc (NMNetworkMenuItem *item)
{
	NMNetworkMenuItemPrivate *priv = NM_NETWORK_MENU_ITEM_GET_PRIVATE (item);
	GString *desc = NULL;

	desc = g_string_new ("");
	g_string_append_printf (desc, "%s: ", priv->ssid_string);

	if (priv->is_adhoc)
		g_string_append (desc, _("ad-hoc"));
	else {
		g_string_append_printf (desc, "%d%%", priv->int_strength);
		if (priv->is_encrypted) {
			g_string_append (desc, ", ");
			g_string_append (desc, _("secure."));
		}
	}

	atk_object_set_name (gtk_widget_get_accessible (GTK_WIDGET (item)), desc->str);
	g_string_free (desc, TRUE);
}

static void
update_icon (NMNetworkMenuItem *item, NMApplet *applet)
{
	NMNetworkMenuItemPrivate *priv = NM_NETWORK_MENU_ITEM_GET_PRIVATE (item);
	gs_unref_object GdkPixbuf *icon_free = NULL, *icon_free2 = NULL;
	GdkPixbuf *icon;
	const char *icon_name = NULL;

	if (priv->is_adhoc)
		icon_name = "nm-adhoc";
	else
		icon_name = mobile_helper_get_quality_icon_name (priv->int_strength);

	icon = nma_icon_check_and_load (icon_name, applet);
	if (icon) {
		if (priv->is_encrypted) {
			GdkPixbuf *encrypted = nma_icon_check_and_load ("nm-secure-lock", applet);

			if (encrypted) {
				icon = icon_free = gdk_pixbuf_copy (icon);

				gdk_pixbuf_composite (encrypted, icon, 0, 0,
				                      gdk_pixbuf_get_width (encrypted),
				                      gdk_pixbuf_get_height (encrypted),
				                      0, 0, 1.0, 1.0,
				                      GDK_INTERP_NEAREST, 255);
			}
		}

		/* Scale to menu size if larger so the menu doesn't look awful */
		if (gdk_pixbuf_get_height (icon) > 24 || gdk_pixbuf_get_width (icon) > 24)
			icon = icon_free2 = gdk_pixbuf_scale_simple (icon, 24, 24, GDK_INTERP_BILINEAR);
	}

	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->strength), icon);
}

void
nm_network_menu_item_set_strength (NMNetworkMenuItem *item,
                                   guint8 strength,
                                   NMApplet *applet)
{
	NMNetworkMenuItemPrivate *priv;

	g_return_if_fail (NM_IS_NETWORK_MENU_ITEM (item));

	priv = NM_NETWORK_MENU_ITEM_GET_PRIVATE (item);

	strength = MIN (strength, 100);
	if (strength > priv->int_strength) {
		priv->int_strength = strength;
		update_icon (item, applet);
		update_atk_desc (item);
	}
}

const char *
nm_network_menu_item_get_hash (NMNetworkMenuItem *item)
{
	g_return_val_if_fail (NM_IS_NETWORK_MENU_ITEM (item), NULL);

	return NM_NETWORK_MENU_ITEM_GET_PRIVATE (item)->hash;
}

gboolean
nm_network_menu_item_find_dupe (NMNetworkMenuItem *item, NMAccessPoint *ap)
{
	NMNetworkMenuItemPrivate *priv;
	const char *path;
	GSList *iter;

	g_return_val_if_fail (NM_IS_NETWORK_MENU_ITEM (item), FALSE);
	g_return_val_if_fail (NM_IS_ACCESS_POINT (ap), FALSE);

	priv = NM_NETWORK_MENU_ITEM_GET_PRIVATE (item);

	path = nm_object_get_path (NM_OBJECT (ap));
	for (iter = priv->dupes; iter; iter = g_slist_next (iter)) {
		if (!strcmp (path, iter->data))
			return TRUE;
	}
	return FALSE;
}

static void
update_label (NMNetworkMenuItem *item, gboolean use_bold)
{
	NMNetworkMenuItemPrivate *priv = NM_NETWORK_MENU_ITEM_GET_PRIVATE (item);

	gtk_label_set_use_markup (GTK_LABEL (priv->ssid), use_bold);
	if (use_bold) {
		char *markup = g_markup_printf_escaped ("<b>%s</b>", priv->ssid_string);

		gtk_label_set_markup (GTK_LABEL (priv->ssid), markup);
		g_free (markup);
	} else
		gtk_label_set_text (GTK_LABEL (priv->ssid), priv->ssid_string);
}

void
nm_network_menu_item_set_active (NMNetworkMenuItem *item, gboolean active)
{
	g_return_if_fail (NM_IS_NETWORK_MENU_ITEM (item));

	update_label (item, active);
}

void
nm_network_menu_item_add_dupe (NMNetworkMenuItem *item, NMAccessPoint *ap)
{
	NMNetworkMenuItemPrivate *priv;
	const char *path;

	g_return_if_fail (NM_IS_NETWORK_MENU_ITEM (item));
	g_return_if_fail (NM_IS_ACCESS_POINT (ap));

	priv = NM_NETWORK_MENU_ITEM_GET_PRIVATE (item);
	path = nm_object_get_path (NM_OBJECT (ap));
	priv->dupes = g_slist_prepend (priv->dupes, g_strdup (path));
}

gboolean
nm_network_menu_item_get_has_connections (NMNetworkMenuItem *item)
{
	g_return_val_if_fail (NM_IS_NETWORK_MENU_ITEM (item), FALSE);

	return NM_NETWORK_MENU_ITEM_GET_PRIVATE (item)->has_connections;
}

gboolean
nm_network_menu_item_get_is_adhoc (NMNetworkMenuItem *item)
{
	g_return_val_if_fail (NM_IS_NETWORK_MENU_ITEM (item), FALSE);

	return NM_NETWORK_MENU_ITEM_GET_PRIVATE (item)->is_adhoc;
}

gboolean
nm_network_menu_item_get_is_encrypted (NMNetworkMenuItem *item)
{
	g_return_val_if_fail (NM_IS_NETWORK_MENU_ITEM (item), FALSE);

	return NM_NETWORK_MENU_ITEM_GET_PRIVATE (item)->is_encrypted;
}

/******************************************************************/

GtkWidget *
nm_network_menu_item_new (NMAccessPoint *ap,
                          guint32 dev_caps,
                          const char *hash,
                          gboolean has_connections,
                          NMApplet *applet)
{
	NMNetworkMenuItem *item;
	NMNetworkMenuItemPrivate *priv;
	guint32 ap_flags, ap_wpa, ap_rsn;
	GBytes *ssid;

	item = g_object_new (NM_TYPE_NETWORK_MENU_ITEM, NULL);
	g_assert (item);

	priv = NM_NETWORK_MENU_ITEM_GET_PRIVATE (item);

	nm_network_menu_item_add_dupe (item, ap);

	ssid = nm_access_point_get_ssid (ap);
	if (ssid) {
		priv->ssid_string = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL),
		                                           g_bytes_get_size (ssid));
	}
	if (!priv->ssid_string)
		priv->ssid_string = g_strdup ("<unknown>");

	priv->has_connections = has_connections;
	priv->hash = g_strdup (hash);
	priv->int_strength = nm_access_point_get_strength (ap);

	if (nm_access_point_get_mode (ap) == NM_802_11_MODE_ADHOC)
		priv->is_adhoc = TRUE;

	ap_flags = nm_access_point_get_flags (ap);
	ap_wpa = nm_access_point_get_wpa_flags (ap);
	ap_rsn = nm_access_point_get_rsn_flags (ap);
	if ((ap_flags & NM_802_11_AP_FLAGS_PRIVACY) || ap_wpa || ap_rsn)
		priv->is_encrypted = TRUE;

	/* Don't enable the menu item the device can't even connect to the AP */
	if (   !nm_utils_security_valid (NMU_SEC_NONE, dev_caps, TRUE, priv->is_adhoc, ap_flags, ap_wpa, ap_rsn)
        && !nm_utils_security_valid (NMU_SEC_STATIC_WEP, dev_caps, TRUE, priv->is_adhoc, ap_flags, ap_wpa, ap_rsn)
	    && !nm_utils_security_valid (NMU_SEC_LEAP, dev_caps, TRUE, priv->is_adhoc, ap_flags, ap_wpa, ap_rsn)
	    && !nm_utils_security_valid (NMU_SEC_DYNAMIC_WEP, dev_caps, TRUE, priv->is_adhoc, ap_flags, ap_wpa, ap_rsn)
	    && !nm_utils_security_valid (NMU_SEC_WPA_PSK, dev_caps, TRUE, priv->is_adhoc, ap_flags, ap_wpa, ap_rsn)
	    && !nm_utils_security_valid (NMU_SEC_WPA2_PSK, dev_caps, TRUE, priv->is_adhoc, ap_flags, ap_wpa, ap_rsn)
	    && !nm_utils_security_valid (NMU_SEC_WPA_ENTERPRISE, dev_caps, TRUE, priv->is_adhoc, ap_flags, ap_wpa, ap_rsn)
	    && !nm_utils_security_valid (NMU_SEC_WPA2_ENTERPRISE, dev_caps, TRUE, priv->is_adhoc, ap_flags, ap_wpa, ap_rsn)) {
		gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
	}

	update_label (item, FALSE);
	update_icon (item, applet);
	update_atk_desc (item);

	return GTK_WIDGET (item);
}

static void
nm_network_menu_item_init (NMNetworkMenuItem *item)
{
	NMNetworkMenuItemPrivate *priv = NM_NETWORK_MENU_ITEM_GET_PRIVATE (item);

	priv->hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	priv->ssid = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->ssid), 0.0, 0.5);

	gtk_container_add (GTK_CONTAINER (item), priv->hbox);
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->ssid, TRUE, TRUE, 0);

	priv->strength = gtk_image_new ();
	gtk_box_pack_end (GTK_BOX (priv->hbox), priv->strength, FALSE, TRUE, 0);
	gtk_widget_show (priv->strength);

	gtk_widget_show (priv->ssid);
	gtk_widget_show (priv->hbox);
}

static void
finalize (GObject *object)
{
	NMNetworkMenuItemPrivate *priv = NM_NETWORK_MENU_ITEM_GET_PRIVATE (object);

	g_free (priv->hash);
	g_free (priv->ssid_string);

	g_slist_foreach (priv->dupes, (GFunc) g_free, NULL);
	g_slist_free (priv->dupes);

	G_OBJECT_CLASS (nm_network_menu_item_parent_class)->finalize (object);
}

static void
nm_network_menu_item_class_init (NMNetworkMenuItemClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (NMNetworkMenuItemPrivate));

	/* virtual methods */
	object_class->finalize = finalize;
}

