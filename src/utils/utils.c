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
 * Copyright 2007 - 2015 Red Hat, Inc.
 */

#include "nm-default.h"

#include <string.h>
#include <netinet/ether.h>

#include "utils.h"

/*
 * utils_ether_addr_valid
 *
 * Compares an Ethernet address against known invalid addresses.
 *
 */
gboolean
utils_ether_addr_valid (const struct ether_addr *test_addr)
{
	guint8 invalid_addr1[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	guint8 invalid_addr2[ETH_ALEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	guint8 invalid_addr3[ETH_ALEN] = {0x44, 0x44, 0x44, 0x44, 0x44, 0x44};
	guint8 invalid_addr4[ETH_ALEN] = {0x00, 0x30, 0xb4, 0x00, 0x00, 0x00}; /* prism54 dummy MAC */

	g_return_val_if_fail (test_addr != NULL, FALSE);

	/* Compare the AP address the card has with invalid ethernet MAC addresses. */
	if (!memcmp (test_addr->ether_addr_octet, &invalid_addr1, ETH_ALEN))
		return FALSE;

	if (!memcmp (test_addr->ether_addr_octet, &invalid_addr2, ETH_ALEN))
		return FALSE;

	if (!memcmp (test_addr->ether_addr_octet, &invalid_addr3, ETH_ALEN))
		return FALSE;

	if (!memcmp (test_addr->ether_addr_octet, &invalid_addr4, ETH_ALEN))
		return FALSE;

	if (test_addr->ether_addr_octet[0] & 1)			/* Multicast addresses */
		return FALSE;

	return TRUE;
}

char *
utils_hash_ap (
#ifdef LIBNM_BUILD
               GBytes *ssid,
#else
               const GByteArray *ssid,
#endif
               NM80211Mode mode,
               guint32 flags,
               guint32 wpa_flags,
               guint32 rsn_flags)
{
	unsigned char input[66];

	memset (&input[0], 0, sizeof (input));

	if (ssid) {
#ifdef LIBNM_BUILD
		memcpy (input, g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
#else
		memcpy (input, ssid->data, ssid->len);
#endif
	}

	if (mode == NM_802_11_MODE_INFRA)
		input[32] |= (1 << 0);
	else if (mode == NM_802_11_MODE_ADHOC)
		input[32] |= (1 << 1);
	else
		input[32] |= (1 << 2);

	/* Separate out no encryption, WEP-only, and WPA-capable */
	if (  !(flags & NM_802_11_AP_FLAGS_PRIVACY)
	    && (wpa_flags == NM_802_11_AP_SEC_NONE)
	    && (rsn_flags == NM_802_11_AP_SEC_NONE))
		input[32] |= (1 << 3);
	else if (   (flags & NM_802_11_AP_FLAGS_PRIVACY)
	         && (wpa_flags == NM_802_11_AP_SEC_NONE)
	         && (rsn_flags == NM_802_11_AP_SEC_NONE))
		input[32] |= (1 << 4);
	else if (   !(flags & NM_802_11_AP_FLAGS_PRIVACY)
	         &&  (wpa_flags != NM_802_11_AP_SEC_NONE)
	         &&  (rsn_flags != NM_802_11_AP_SEC_NONE))
		input[32] |= (1 << 5);
	else
		input[32] |= (1 << 6);

	/* duplicate it */
	memcpy (&input[33], &input[0], 32);
	return g_compute_checksum_for_data (G_CHECKSUM_MD5, input, sizeof (input));
}

typedef struct {
	const char *tag;
	const char *replacement;
} Tag;

static Tag escaped_tags[] = {
	{ "<center>", NULL },
	{ "</center>", NULL },
	{ "<p>", "\n" },
	{ "</p>", NULL },
	{ "<B>", "<b>" },
	{ "</B>", "</b>" },
	{ "<I>", "<i>" },
	{ "</I>", "</i>" },
	{ "<u>", "<u>" },
	{ "</u>", "</u>" },
	{ "&", "&amp;" },
	{ NULL, NULL }
};

char *
utils_escape_notify_message (const char *src)
{
	const char *p = src;
	GString *escaped;

	/* Filter the source text and get rid of some HTML tags since the
	 * notification spec only allows a subset of HTML.  Substitute
	 * HTML code for characters like & that are invalid in HTML.
	 */

	escaped = g_string_sized_new (strlen (src) + 5);
	while (*p) {
		Tag *t = &escaped_tags[0];
		gboolean found = FALSE;

		while (t->tag) {
			if (strncasecmp (p, t->tag, strlen (t->tag)) == 0) {
				p += strlen (t->tag);
				if (t->replacement)
					g_string_append (escaped, t->replacement);
				found = TRUE;
				break;
			}
			t++;
		}
		if (!found)
			g_string_append_c (escaped, *p++);
	}

	return g_string_free (escaped, FALSE);
}

char *
utils_create_mobile_connection_id (const char *provider, const char *plan_name)
{
	g_return_val_if_fail (provider != NULL, NULL);

	if (plan_name)
		return g_strdup_printf ("%s %s", provider, plan_name);

	/* The %s is a mobile provider name, eg "T-Mobile" */
	return g_strdup_printf (_("%s connection"), provider);
}

void
utils_show_error_dialog (const char *title,
                         const char *text1,
                         const char *text2,
                         gboolean modal,
                         GtkWindow *parent)
{
	GtkWidget *err_dialog;

	g_return_if_fail (text1 != NULL);

	err_dialog = gtk_message_dialog_new (parent,
	                                     GTK_DIALOG_DESTROY_WITH_PARENT,
	                                     GTK_MESSAGE_ERROR,
	                                     GTK_BUTTONS_CLOSE,
	                                     "%s",
	                                     text1);

	if (text2)
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (err_dialog), "%s", text2);
	if (title)
		gtk_window_set_title (GTK_WINDOW (err_dialog), title);

	if (modal) {
		gtk_dialog_run (GTK_DIALOG (err_dialog));
		gtk_widget_destroy (err_dialog);
	} else {
		g_signal_connect (err_dialog, "delete-event", G_CALLBACK (gtk_widget_destroy), NULL);
		g_signal_connect (err_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

		gtk_widget_show_all (err_dialog);
		gtk_window_present (GTK_WINDOW (err_dialog));
	}
}


gboolean
utils_char_is_ascii_print (char character)
{
	return g_ascii_isprint (character);
}

gboolean
utils_char_is_ascii_digit (char character)
{
	return g_ascii_isdigit (character);
}

gboolean
utils_char_is_ascii_ip4_address (char character)
{
	return g_ascii_isdigit (character) || character == '.';
}

gboolean
utils_char_is_ascii_ip6_address (char character)
{
	return g_ascii_isxdigit (character) || character == ':';
}

gboolean
utils_char_is_ascii_apn (char character)
{
	return g_ascii_isalnum (character)
	       || character == '.'
	       || character == '_'
	       || character == '-';
}

/**
 * Filters the characters from a text that was just input into GtkEditable.
 * Returns FALSE, if after filtering no characters were left. TRUE means,
 * that valid characters were added and the content of the GtkEditable changed.
 **/
gboolean
utils_filter_editable_on_insert_text (GtkEditable *editable,
                                      const gchar *text,
                                      gint length,
                                      gint *position,
                                      void *user_data,
                                      UtilsFilterGtkEditableFunc validate_character,
                                      gpointer block_func)
{
	int i, count = 0;
	gchar *result = g_new (gchar, length+1);

	for (i = 0; i < length; i++) {
		if (validate_character (text[i]))
			result[count++] = text[i];
	}
	result[count] = 0;

	if (count > 0) {
		if (block_func) {
			g_signal_handlers_block_by_func (G_OBJECT (editable),
			                                 G_CALLBACK (block_func),
			                                 user_data);
		}
		gtk_editable_insert_text (editable, result, count, position);
		if (block_func) {
			g_signal_handlers_unblock_by_func (G_OBJECT (editable),
			                                   G_CALLBACK (block_func),
			                                   user_data);
		}
	}
	g_signal_stop_emission_by_name (G_OBJECT (editable), "insert-text");

	g_free (result);

	return count > 0;
}

/**
 * utils_override_bg_color:
 *
 * The function can be used to set background color for a widget.
 * There are functions for that in Gtk2 [1] and Gtk3 [2]. Unfortunately, they
 * have been deprecated, and moreover gtk_widget_override_background_color()
 * stopped working at some point for some Gtk themes, including the default
 * Adwaita theme.
 * [1] gtk_widget_modify_bg() or gtk_widget_modify_base()
 * [2] gtk_widget_override_background_color()
 *
 * Related links:
 * https://bugzilla.gnome.org/show_bug.cgi?id=656461
 * https://mail.gnome.org/archives/gtk-list/2015-February/msg00053.html
 */
void
utils_override_bg_color (GtkWidget *widget, GdkRGBA *rgba)
{
	GtkCssProvider *provider;
	char *css;

	provider = (GtkCssProvider *) g_object_get_data (G_OBJECT (widget), "our-css-provider");
	if (G_UNLIKELY (!provider)) {
		provider = gtk_css_provider_new ();
		gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
		                                GTK_STYLE_PROVIDER (provider),
		                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		g_object_set_data_full (G_OBJECT (widget), "our-css-provider",
		                        provider, (GDestroyNotify) g_object_unref);
	}

	if (rgba) {
		css = g_strdup_printf ("* { background-color: %s; background-image: none; }",
		                       gdk_rgba_to_string (rgba));
		gtk_css_provider_load_from_data (provider, css, -1, NULL);
		g_free (css);
	} else
		gtk_css_provider_load_from_data (provider, "", -1, NULL);
}

void
utils_set_cell_background (GtkCellRenderer *cell,
                           const char *color,
                           const char *value)
{
	if (color) {
		if (!value || !*value) {
			g_object_set (G_OBJECT (cell),
			              "cell-background-set", TRUE,
			              "cell-background", color,
			              NULL);
		} else {
			char *markup;
			markup = g_markup_printf_escaped ("<span background='%s'>%s</span>",
			                                  color, value);
			g_object_set (G_OBJECT (cell), "markup", markup, NULL);
			g_free (markup);
			g_object_set (G_OBJECT (cell), "cell-background-set", FALSE, NULL);
		}
	} else
		g_object_set (G_OBJECT (cell), "cell-background-set", FALSE, NULL);
}

/* Change key in @event to 'Enter' key. */
void
utils_fake_return_key (GdkEventKey *event)
{
	GdkKeymapKey *keys = NULL;
	gint n_keys;

	/* Get hardware keycode for GDK_KEY_Return */
	if (gdk_keymap_get_entries_for_keyval (gdk_keymap_get_default (), GDK_KEY_Return, &keys, &n_keys)) {
		event->keyval = GDK_KEY_Return;
		event->hardware_keycode = keys[0].keycode;
		event->state = 0;
	}
	g_free (keys);
}

