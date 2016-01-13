/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
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
 * (C) Copyright 2008 - 2012 Red Hat, Inc.
 */

#ifndef __CE_PAGE_H__
#define __CE_PAGE_H__

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include <dbus/dbus-glib.h>
#include <nm-connection.h>
#include <nm-client.h>
#include <nm-remote-settings.h>
#include "nm-connection-editor.h"
#include "utils.h"

/* for ARPHRD_ETHER / ARPHRD_INFINIBAND for MAC utilies */
#include <net/if_arp.h>

typedef void (*PageNewConnectionResultFunc) (NMConnection *connection,
                                             gboolean canceled,
                                             GError *error,
                                             gpointer user_data);

typedef GSList * (*PageGetConnectionsFunc) (gpointer user_data);

typedef void (*PageNewConnectionFunc) (GtkWindow *parent,
                                       const char *detail,
                                       NMRemoteSettings *settings,
                                       PageNewConnectionResultFunc result_func,
                                       gpointer user_data);

#define CE_TYPE_PAGE            (ce_page_get_type ())
#define CE_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CE_TYPE_PAGE, CEPage))
#define CE_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CE_TYPE_PAGE, CEPageClass))
#define CE_IS_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CE_TYPE_PAGE))
#define CE_IS_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CE_TYPE_PAGE))
#define CE_PAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CE_TYPE_PAGE, CEPageClass))

#define CE_PAGE_CONNECTION    "connection"
#define CE_PAGE_INITIALIZED   "initialized"
#define CE_PAGE_PARENT_WINDOW "parent-window"

typedef struct {
	GObject parent;

	gboolean initialized;
	gboolean inter_page_change_running;
	GtkBuilder *builder;
	GtkWidget *page;
	char *title;

	DBusGProxy *proxy;
	gulong secrets_done_validate;

	NMConnectionEditor *editor;
	NMConnection *connection;
	GtkWindow *parent_window;
	NMClient *client;
	NMRemoteSettings *settings;
} CEPage;

typedef struct {
	GObjectClass parent;

	/* Virtual functions */
	gboolean    (*ce_page_validate_v) (CEPage *self, NMConnection *connection, GError **error);
	gboolean    (*last_update)  (CEPage *self, NMConnection *connection, GError **error);
	gboolean    (*inter_page_change)  (CEPage *self);

	/* Signals */
	void        (*changed)     (CEPage *self);
	void        (*initialized) (CEPage *self, GError *error);
} CEPageClass;


typedef CEPage* (*CEPageNewFunc)(NMConnectionEditor *editor,
                                 NMConnection *connection,
                                 GtkWindow *parent,
                                 NMClient *client,
                                 NMRemoteSettings *settings,
                                 const char **out_secrets_setting_name,
                                 GError **error);


GType ce_page_get_type (void);

GtkWidget *  ce_page_get_page (CEPage *self);

const char * ce_page_get_title (CEPage *self);

gboolean ce_page_validate (CEPage *self, NMConnection *connection, GError **error);
gboolean ce_page_last_update (CEPage *self, NMConnection *connection, GError **error);
gboolean ce_page_inter_page_change (CEPage *self);

void ce_page_setup_mac_combo (CEPage *self, GtkComboBox *combo,
                              const GByteArray *mac, int type, char **mac_list);
void ce_page_setup_data_combo (CEPage *self, GtkComboBox *combo,
                               const char *data, char **list);
void ce_page_setup_device_combo (CEPage *self,
                                 GtkComboBox *combo,
                                 GType device_type,
                                 const char *ifname,
                                 const GByteArray *mac,
                                 int mac_type,
                                 const char *mac_property,
                                 gboolean ifname_first);
gboolean ce_page_mac_entry_valid (GtkEntry *entry, int type, const char *property_name, GError **error);
gboolean ce_page_interface_name_valid (const char *iface, const char *property_name, GError **error);
gboolean ce_page_device_entry_get (GtkEntry *entry, int type,
                                   gboolean check_ifname,
                                   char **ifname, GByteArray **mac,
                                   const char *device_name,
                                   GError **error);
void ce_page_mac_to_entry (const GByteArray *mac, int type, GtkEntry *entry);
GByteArray *ce_page_entry_to_mac (GtkEntry *entry, int type, gboolean *invalid);

void ce_page_changed (CEPage *self);

gboolean ce_spin_output_with_automatic (GtkSpinButton *spin, gpointer user_data);

gboolean ce_spin_output_with_default (GtkSpinButton *spin, gpointer user_data);

int ce_get_property_default (NMSetting *setting, const char *property_name);

void ce_page_complete_init (CEPage *self,
                            const char *setting_name,
                            GHashTable *secrets,
                            GError *error);

gboolean ce_page_get_initialized (CEPage *self);

char *ce_page_get_next_available_name (GSList *connections, const char *format);

/* Only for subclasses */
NMConnection *ce_page_new_connection (const char *format,
                                      const char *ctype,
                                      gboolean autoconnect,
                                      NMRemoteSettings *settings,
                                      gpointer user_data);

CEPage *ce_page_new (GType page_type,
                     NMConnectionEditor *editor,
                     NMConnection *connection,
                     GtkWindow *parent_window,
                     NMClient *client,
                     NMRemoteSettings *settings,
                     const char *ui_file,
                     const char *widget_name,
                     const char *title);


#endif  /* __CE_PAGE_H__ */

