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
 * Copyright 2008 - 2014 Red Hat, Inc.
 */

#ifndef __CE_PAGE_H__
#define __CE_PAGE_H__

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include <NetworkManager.h>

#include "nm-connection-editor.h"
#include "utils.h"

/* for ARPHRD_ETHER / ARPHRD_INFINIBAND for MAC utilies */
#include <net/if_arp.h>

struct _func_tag_page_new_connection_result;
#define FUNC_TAG_PAGE_NEW_CONNECTION_RESULT_IMPL struct _func_tag_page_new_connection_result *_dummy
#define FUNC_TAG_PAGE_NEW_CONNECTION_RESULT_CALL ((struct _func_tag_page_new_connection_result *) NULL)
typedef void (*PageNewConnectionResultFunc) (FUNC_TAG_PAGE_NEW_CONNECTION_RESULT_IMPL,
                                             NMConnection *connection, /* allow-none, don't transfer reference, allow-keep */
                                             gboolean canceled,
                                             GError *error,
                                             gpointer user_data);

typedef GSList * (*PageGetConnectionsFunc) (gpointer user_data);

struct _func_tag_page_new_connection;
#define FUNC_TAG_PAGE_NEW_CONNECTION_IMPL struct _func_tag_page_new_connection *_dummy
#define FUNC_TAG_PAGE_NEW_CONNECTION_CALL ((struct _func_tag_page_new_connection *) NULL)
typedef void (*PageNewConnectionFunc) (FUNC_TAG_PAGE_NEW_CONNECTION_IMPL,
                                       GtkWindow *parent,
                                       const char *detail,
                                       gpointer detail_data,
                                       NMConnection *connection,
                                       NMClient *client,
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

	gulong secrets_done_validate;

	NMConnectionEditor *editor;
	NMConnection *connection;
	GtkWindow *parent_window;
	NMClient *client;
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
                                 const char **out_secrets_setting_name,
                                 GError **error);

#define CE_TOOLTIP_ADDR_AUTO _("IP addresses identify your computer on the network. " \
                               "Click the “Add” button to add static IP address to be " \
                               "configured in addition to the automatic ones.")
#define CE_TOOLTIP_ADDR_MANUAL _("IP addresses identify your computer on the network. " \
                                 "Click the “Add” button to add an IP address.")
#define CE_TOOLTIP_ADDR_SHARED _("The IP address identify your computer on the network and " \
                                 "determines the address range distributed to other computers. " \
                                 "Click the “Add” button to add an IP address. "\
                                 "If no address is provided, range will be determined automatically.")

#define CE_LABEL_ADDR_AUTO _("Additional static addresses")
#define CE_LABEL_ADDR_MANUAL _("Addresses")
#define CE_LABEL_ADDR_SHARED _("Address (optional)")

GType ce_page_get_type (void);

GtkWidget *  ce_page_get_page (CEPage *self);

const char * ce_page_get_title (CEPage *self);

gboolean ce_page_validate (CEPage *self, NMConnection *connection, GError **error);
gboolean ce_page_last_update (CEPage *self, NMConnection *connection, GError **error);
gboolean ce_page_inter_page_change (CEPage *self);

void ce_page_setup_mac_combo (CEPage *self, GtkComboBox *combo,
                              const char *mac, char **mac_list);
void ce_page_setup_data_combo (CEPage *self, GtkComboBox *combo,
                               const char *data, char **list);
void ce_page_setup_cloned_mac_combo (GtkComboBoxText *combo, const char *current);
void ce_page_setup_device_combo (CEPage *self,
                                 GtkComboBox *combo,
                                 GType device_type,
                                 const char *ifname,
                                 const char *mac,
                                 const char *mac_property);
gboolean ce_page_mac_entry_valid (GtkEntry *entry, int type, const char *property_name, GError **error);
gboolean ce_page_interface_name_valid (const char *iface, const char *property_name, GError **error);
gboolean ce_page_device_entry_get (GtkEntry *entry, int type,
                                   gboolean check_ifname,
                                   char **ifname, char **mac,
                                   const char *device_name,
                                   GError **error);
char *ce_page_cloned_mac_get (GtkComboBoxText *combo);
gboolean ce_page_cloned_mac_combo_valid (GtkComboBoxText *combo, int type, const char *property_name, GError **error);

void ce_page_changed (CEPage *self);

void ce_spin_automatic_val (GtkSpinButton *spin, int defvalue);
void ce_spin_default_val (GtkSpinButton *spin, int defvalue);

int ce_get_property_default (NMSetting *setting, const char *property_name);

void ce_page_complete_init (CEPage *self,
                            const char *setting_name,
                            GVariant *secrets,
                            GError *error);

gboolean ce_page_get_initialized (CEPage *self);

char *ce_page_get_next_available_name (const GPtrArray *connections, const char *format);

/* Only for subclasses */
void ce_page_complete_connection (NMConnection *connection,
                                  const char *format,
                                  const char *ctype,
                                  gboolean autoconnect,
                                  NMClient *client);

static inline NMConnection *
_ensure_connection_own (NMConnection **connection)
{
	return (*connection) ?: (*connection = nm_simple_connection_new ());
}

static inline NMConnection *
_ensure_connection_other (NMConnection *connection, NMConnection **connection_to_free)
{
	if (connection) {
		*connection_to_free = NULL;
		return connection;
	}
	return (*connection_to_free = nm_simple_connection_new ());
}

CEPage *ce_page_new (GType page_type,
                     NMConnectionEditor *editor,
                     NMConnection *connection,
                     GtkWindow *parent_window,
                     NMClient *client,
                     const char *ui_resource,
                     const char *widget_name,
                     const char *title);


#endif  /* __CE_PAGE_H__ */

