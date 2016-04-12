/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
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
 * Copyright 2015 Red Hat, Inc.
 */

#include "nm-default.h"

#include <string.h>

#include "nma-ui-utils.h"

/*---------------------------------------------------------------------------*/
/* Password storage icon */

#define PASSWORD_STORAGE_MENU_TAG  "password-storage-menu"
#define MENU_WITH_NOT_REQUIRED_TAG "menu-with-not-required"
#define SENSITIVE_ASK_ENTRY        "sensitive-ask-entry"

typedef enum {
	ITEM_STORAGE_USER    = 0,
	ITEM_STORAGE_SYSTEM  = 1,
	ITEM_STORAGE_ASK     = 2,
	ITEM_STORAGE_UNUSED  = 3,
	__ITEM_STORAGE_MAX,
	ITEM_STORAGE_MAX = __ITEM_STORAGE_MAX - 1,
} MenuItem;

static const char *icon_name_table[ITEM_STORAGE_MAX + 1] = {
	[ITEM_STORAGE_USER]    = "user-info-symbolic",
	[ITEM_STORAGE_SYSTEM]  = "system-users-symbolic",
	[ITEM_STORAGE_ASK]     = "dialog-question-symbolic",
	[ITEM_STORAGE_UNUSED]  = "edit-clear-all-symbolic",
};
static const char *icon_desc_table[ITEM_STORAGE_MAX + 1] = {
	[ITEM_STORAGE_USER]    = N_("Store the password only for this user"),
	[ITEM_STORAGE_SYSTEM]  = N_("Store the password for all users"),
	[ITEM_STORAGE_ASK]     = N_("Ask for this password every time"),
	[ITEM_STORAGE_UNUSED]  = N_("The password is not required"),
};

static void
g_free_str0 (gpointer mem)
{
	/* g_free a char pointer and set it to 0 before (for passwords). */
	if (mem) {
		char *p = mem;
		memset (p, 0, strlen (p));
		g_free (p);
	}
}

static void
change_password_storage_icon (GtkWidget *passwd_entry, MenuItem item)
{
	const char *old_pwd;
	gboolean sensitive_ask;

	g_return_if_fail (item >= 0 && item <= ITEM_STORAGE_MAX);

	gtk_entry_set_icon_from_icon_name (GTK_ENTRY (passwd_entry),
	                                   GTK_ENTRY_ICON_SECONDARY,
	                                   icon_name_table[item]);
	gtk_entry_set_icon_tooltip_text (GTK_ENTRY (passwd_entry),
	                                 GTK_ENTRY_ICON_SECONDARY,
	                                 icon_desc_table[item]);

	/* We want to make entry insensitive when ITEM_STORAGE_ASK is selected
	 * Unfortunately, making GtkEntry insensitive will also make the icon
	 * insensitive, which prevents user from reverting the action.
	 * Let's workaround that by disabling focus for entry instead of
	 * sensitivity change.
	*/
	sensitive_ask = !!g_object_get_data (G_OBJECT (passwd_entry), SENSITIVE_ASK_ENTRY);
	if (   (item == ITEM_STORAGE_ASK && !sensitive_ask)
	    || item == ITEM_STORAGE_UNUSED) {
		/* Store the old password */
		old_pwd = gtk_entry_get_text (GTK_ENTRY (passwd_entry));
		if (old_pwd && *old_pwd)
			g_object_set_data_full (G_OBJECT (passwd_entry), "password-old",
		                                g_strdup (old_pwd), g_free_str0);
		gtk_entry_set_text (GTK_ENTRY (passwd_entry), "");

		if (gtk_widget_is_focus (passwd_entry))
			gtk_widget_child_focus ((gtk_widget_get_toplevel (passwd_entry)), GTK_DIR_TAB_BACKWARD);
		gtk_widget_set_can_focus (passwd_entry, FALSE);
	} else {
		/* Set the old password to the entry */
		old_pwd = g_object_get_data (G_OBJECT (passwd_entry), "password-old");
		if (old_pwd && *old_pwd)
			gtk_entry_set_text (GTK_ENTRY (passwd_entry), old_pwd);
		g_object_set_data (G_OBJECT (passwd_entry), "password-old", NULL);

		if (!gtk_widget_get_can_focus (passwd_entry)) {
			gtk_widget_set_can_focus (passwd_entry, TRUE);
			gtk_widget_grab_focus (passwd_entry);
		}
	}
}

static MenuItem
secret_flags_to_menu_item (NMSettingSecretFlags flags, gboolean with_not_required)
{
	MenuItem idx;

	if (flags & NM_SETTING_SECRET_FLAG_NOT_SAVED)
		idx = ITEM_STORAGE_ASK;
	else if (with_not_required && (flags & NM_SETTING_SECRET_FLAG_NOT_REQUIRED))
		idx = ITEM_STORAGE_UNUSED;
	else if (flags & NM_SETTING_SECRET_FLAG_AGENT_OWNED)
		idx = ITEM_STORAGE_USER;
	else
		idx = ITEM_STORAGE_SYSTEM;

	return idx;
}

static NMSettingSecretFlags
menu_item_to_secret_flags (MenuItem item)
{
	NMSettingSecretFlags flags = NM_SETTING_SECRET_FLAG_NONE;

	switch (item) {
	case ITEM_STORAGE_USER:
		flags |= NM_SETTING_SECRET_FLAG_AGENT_OWNED;
		break;
	case ITEM_STORAGE_ASK:
		flags |= NM_SETTING_SECRET_FLAG_NOT_SAVED;
		break;
	case ITEM_STORAGE_UNUSED:
		flags |= NM_SETTING_SECRET_FLAG_NOT_REQUIRED;
		break;
	case ITEM_STORAGE_SYSTEM:
	default:
		break;
	}
	return flags;
}

typedef struct {
	NMSetting *setting;
	char *password_flags_name;
	MenuItem item_number;
	GtkWidget *passwd_entry;
} PopupMenuItemInfo;

static void
popup_menu_item_info_destroy (gpointer data, GClosure *closure)
{
	PopupMenuItemInfo *info = (PopupMenuItemInfo *) data;

	if (info->setting)
		g_object_unref (info->setting);
	g_clear_pointer (&info->password_flags_name, g_free);
	if (info->passwd_entry)
		g_object_remove_weak_pointer (G_OBJECT (info->passwd_entry), (gpointer *) &info->passwd_entry);
	g_slice_free (PopupMenuItemInfo, info);
}

static void
activate_menu_item_cb (GtkMenuItem *menuitem, gpointer user_data)
{
	PopupMenuItemInfo *info = (PopupMenuItemInfo *) user_data;
	NMSettingSecretFlags flags;

	/* Update password flags according to the password-storage popup menu */
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem))) {
		flags = menu_item_to_secret_flags (info->item_number);

		/* Update the secret flags in the setting */
		if (info->setting)
			nm_setting_set_secret_flags (info->setting, info->password_flags_name,
			                             flags, NULL);

		/* Change icon */
		if (info->passwd_entry) {
			change_password_storage_icon (info->passwd_entry, info->item_number);

			/* Emit "changed" signal on the entry */
			g_signal_emit_by_name (G_OBJECT (info->passwd_entry), "changed");
		}
	}
}

static void
popup_menu_item_info_register (GtkWidget *item,
                               NMSetting *setting,
                               const char *password_flags_name,
                               MenuItem item_number,
                               GtkWidget *passwd_entry)
{
	PopupMenuItemInfo *info;

	info = g_slice_new0 (PopupMenuItemInfo);
	info->setting = setting ? g_object_ref (setting) : NULL;
	info->password_flags_name = g_strdup (password_flags_name);
	info->item_number = item_number;
	info->passwd_entry = passwd_entry;

	if (info->passwd_entry)
		g_object_add_weak_pointer (G_OBJECT (info->passwd_entry), (gpointer *) &info->passwd_entry);

	g_signal_connect_data (item, "activate",
	                       G_CALLBACK (activate_menu_item_cb),
	                       info,
	                       (GClosureNotify) popup_menu_item_info_destroy, 0);
}

static void
icon_release_cb (GtkEntry *entry,
                 GtkEntryIconPosition position,
                 GdkEventButton *event,
                 gpointer data)
{
	GtkMenu *menu = GTK_MENU (data);
	if (position == GTK_ENTRY_ICON_SECONDARY) {
		gtk_widget_show_all (GTK_WIDGET (data));
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL,
		                event->button, event->time);
	}
}

/**
 * nma_utils_setup_password_storage:
 * @passwd_entry: password #GtkEntry which the icon is attached to
 * @initial_flags: initial secret flags to setup password menu from
 * @setting: #NMSetting containing the password, or NULL
 * @password_flags_name: name of the secret flags (like psk-flags), or NULL
 * @with_not_required: whether to include "Not required" menu item
 * @sensitive_ask: %TRUE if entry should be sensivive on selected "always-ask"
 *   icon (this is e.f. for nm-applet asking for password)
 *
 * Adds a secondary icon and creates a popup menu for password entry.
 * The active menu item is set up according to initial_flags, or
 * from @setting/@password_flags_name (if they are not NULL).
 * If the @setting/@password_flags_name are not NULL, secret flags will
 * be automatically updated in the setting when menu is changed.
 */
void
nma_utils_setup_password_storage (GtkWidget *passwd_entry,
                                  NMSettingSecretFlags initial_flags,
                                  NMSetting *setting,
                                  const char *password_flags_name,
                                  gboolean with_not_required,
                                  gboolean sensitive_ask)
{
	GtkWidget *popup_menu;
	GtkWidget *item[4];
	GSList *group;
	MenuItem idx;
	NMSettingSecretFlags secret_flags;

	/* Whether entry should be sensitive if "always-ask" is active " */
	g_object_set_data (G_OBJECT (passwd_entry), SENSITIVE_ASK_ENTRY, GUINT_TO_POINTER (sensitive_ask));

	popup_menu = gtk_menu_new ();
	g_object_set_data (G_OBJECT (popup_menu), PASSWORD_STORAGE_MENU_TAG, GUINT_TO_POINTER (TRUE));
	g_object_set_data (G_OBJECT (popup_menu), MENU_WITH_NOT_REQUIRED_TAG, GUINT_TO_POINTER (with_not_required));
	group = NULL;
	item[0] = gtk_radio_menu_item_new_with_label (group, icon_desc_table[0]);
	group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item[0]));
	item[1] = gtk_radio_menu_item_new_with_label (group, icon_desc_table[1]);
	item[2] = gtk_radio_menu_item_new_with_label (group, icon_desc_table[2]);
	if (with_not_required)
		item[3] = gtk_radio_menu_item_new_with_label (group, icon_desc_table[3]);

	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item[0]);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item[1]);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item[2]);
	if (with_not_required)
		gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item[3]);

	popup_menu_item_info_register (item[0], setting, password_flags_name, ITEM_STORAGE_USER, passwd_entry);
	popup_menu_item_info_register (item[1], setting, password_flags_name, ITEM_STORAGE_SYSTEM, passwd_entry);
	popup_menu_item_info_register (item[2], setting, password_flags_name, ITEM_STORAGE_ASK, passwd_entry);
	if (with_not_required)
		popup_menu_item_info_register (item[3], setting, password_flags_name, ITEM_STORAGE_UNUSED, passwd_entry);

	g_signal_connect (passwd_entry, "icon-release", G_CALLBACK (icon_release_cb), popup_menu);
	gtk_menu_attach_to_widget (GTK_MENU (popup_menu), passwd_entry, NULL);

	/* Initialize active item for password-storage popup menu */
	if (setting && password_flags_name)
		nm_setting_get_secret_flags (setting, password_flags_name, &secret_flags, NULL);
	else
		secret_flags = initial_flags;

	idx = secret_flags_to_menu_item (secret_flags, with_not_required);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item[idx]), TRUE);
	change_password_storage_icon (passwd_entry, idx);
}

/**
 * nma_utils_menu_to_secret_flags:
 * @passwd_entry: password #GtkEntry which the password icon/menu is attached to
 *
 * Returns secret flags corresponding to the selected password storage menu
 * in the attached icon
 *
 * Returns: secret flags corresponding to the active item in password menu
 */
NMSettingSecretFlags
nma_utils_menu_to_secret_flags (GtkWidget *passwd_entry)
{
	GList *menu_list, *iter;
	GtkWidget *menu = NULL;
	NMSettingSecretFlags flags = NM_SETTING_SECRET_FLAG_NONE;

	menu_list = gtk_menu_get_for_attach_widget (passwd_entry);
	for (iter = menu_list; iter; iter = g_list_next (iter)) {
		if (g_object_get_data (G_OBJECT (iter->data), PASSWORD_STORAGE_MENU_TAG)) {
			menu = iter->data;
			break;
		}
	}

	/* Translate password popup menu to secret flags */
	if (menu) {
		MenuItem idx = 0;
		GList *list;
		int i, length;

		list = gtk_container_get_children (GTK_CONTAINER (menu));
		length = g_list_length (list);
		for (i = 0; i < length; i++) {
			if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (list->data))) {
				idx =  (MenuItem) i;
				break;
			}
			list = g_list_next (list);
		}

		flags = menu_item_to_secret_flags (idx);
	}
	return flags;
}

/**
 * nma_utils_update_password_storage:
 * @passwd_entry: #GtkEntry with the password
 * @secret_flags: secret flags to set
 * @setting: #NMSetting containing the password, or NULL
 * @password_flags_name: name of the secret flags (like psk-flags), or NULL
 *
 * Updates secret flags in the password storage popup menu and also
 * in the @setting (if @setting and @password_flags_name are not NULL).
 *
 */
void
nma_utils_update_password_storage (GtkWidget *passwd_entry,
                                   NMSettingSecretFlags secret_flags,
                                   NMSetting *setting,
                                   const char *password_flags_name)
{
	GList *menu_list, *iter;
	GtkWidget *menu = NULL;

	/* Update secret flags (WEP_KEY_FLAGS, PSK_FLAGS, ...) in the security setting */
	if (setting && password_flags_name)
		nm_setting_set_secret_flags (setting, password_flags_name, secret_flags, NULL);

	/* Update password-storage popup menu to reflect secret flags */
	menu_list = gtk_menu_get_for_attach_widget (passwd_entry);
	for (iter = menu_list; iter; iter = g_list_next (iter)) {
		if (g_object_get_data (G_OBJECT (iter->data), PASSWORD_STORAGE_MENU_TAG)) {
			menu = iter->data;
			break;
		}
	}

	if (menu) {
		GtkRadioMenuItem *item;
		MenuItem idx;
		GSList *group;
		gboolean with_not_required;
		int i, last;

		/* radio menu group list contains the menu items in reverse order */
		item = (GtkRadioMenuItem *) gtk_menu_get_active (GTK_MENU (menu));
		group = gtk_radio_menu_item_get_group (item);
		with_not_required = !!g_object_get_data (G_OBJECT (menu), MENU_WITH_NOT_REQUIRED_TAG);

		idx = secret_flags_to_menu_item (secret_flags, with_not_required);
		last = g_slist_length (group) - idx - 1;
		for (i = 0; i < last; i++)
			group = g_slist_next (group);

		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (group->data), TRUE);
		change_password_storage_icon (passwd_entry, idx);
	}
}
/*---------------------------------------------------------------------------*/

