/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
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

#include <gudev/gudev.h>

#include <nm-device.h>
#include <nm-device-bt.h>

#include "nm-ui-utils.h"

static char *ignored_words[] = {
	"Semiconductor",
	"Components",
	"Corporation",
	"Communications",
	"Company",
	"Corp.",
	"Corp",
	"Co.",
	"Inc.",
	"Inc",
	"Incorporated",
	"Ltd.",
	"Limited.",
	"Intel?",
	"chipset",
	"adapter",
	"[hex]",
	"NDIS",
	"Module",
	NULL
};

static char *ignored_phrases[] = {
	"Multiprotocol MAC/baseband processor",
	"Wireless LAN Controller",
	"Wireless LAN Adapter",
	"Wireless Adapter",
	"Network Connection",
	"Wireless Cardbus Adapter",
	"Wireless CardBus Adapter",
	"54 Mbps Wireless PC Card",
	"Wireless PC Card",
	"Wireless PC",
	"PC Card with XJACK(r) Antenna",
	"Wireless cardbus",
	"Wireless LAN PC Card",
	"Technology Group Ltd.",
	"Communication S.p.A.",
	"Business Mobile Networks BV",
	"Mobile Broadband Minicard Composite Device",
	"Mobile Communications AB",
	"(PC-Suite Mode)",
	NULL
};

static char *
fixup_desc_string (const char *desc)
{
	char *p, *temp;
	char **words, **item;
	GString *str;

	p = temp = g_strdup (desc);
	while (*p) {
		if (*p == '_' || *p == ',')
			*p = ' ';
		p++;
	}

	/* Attempt to shorten ID by ignoring certain phrases */
	for (item = ignored_phrases; *item; item++) {
		guint32 ignored_len = strlen (*item);

		p = strstr (temp, *item);
		if (p)
			memmove (p, p + ignored_len, strlen (p + ignored_len) + 1); /* +1 for the \0 */
	}

	/* Attmept to shorten ID by ignoring certain individual words */
	words = g_strsplit (temp, " ", 0);
	str = g_string_new_len (NULL, strlen (temp));
	g_free (temp);

	for (item = words; *item; item++) {
		int i = 0;
		gboolean ignore = FALSE;

		if (g_ascii_isspace (**item) || (**item == '\0'))
			continue;

		while (ignored_words[i] && !ignore) {
			if (!strcmp (*item, ignored_words[i]))
				ignore = TRUE;
			i++;
		}

		if (!ignore) {
			if (str->len)
				g_string_append_c (str, ' ');
			g_string_append (str, *item);
		}
	}
	g_strfreev (words);

	temp = str->str;
	g_string_free (str, FALSE);

	return temp;
}

#define VENDOR_TAG "nma_utils_get_device_vendor"
#define PRODUCT_TAG "nma_utils_get_device_product"
#define DESCRIPTION_TAG "nma_utils_get_device_description"

static void
get_description (NMDevice *device)
{
	char *description = NULL;
	const char *dev_product;
	const char *dev_vendor;
	char *product = NULL;
	char *vendor = NULL;
	GString *str;

	dev_product = nm_device_get_product (device);
	dev_vendor = nm_device_get_vendor (device);
	if (!dev_product || !dev_vendor) {
		g_object_set_data (G_OBJECT (device),
		                   DESCRIPTION_TAG,
		                   (char *) nm_device_get_iface (device));
		return;
	}

	product = fixup_desc_string (dev_product);
	vendor = fixup_desc_string (dev_vendor);

	str = g_string_new_len (NULL, strlen (vendor) + strlen (product) + 1);

	/* Another quick hack; if all of the fixed up vendor string
	 * is found in product, ignore the vendor.
	 */
	if (!strcasestr (product, vendor)) {
		g_string_append (str, vendor);
		g_string_append_c (str, ' ');
	}

	g_string_append (str, product);

	description = g_string_free (str, FALSE);

	g_object_set_data_full (G_OBJECT (device),
	                        VENDOR_TAG, vendor,
	                        (GDestroyNotify) g_free);
	g_object_set_data_full (G_OBJECT (device),
	                        PRODUCT_TAG, product,
	                        (GDestroyNotify) g_free);
	g_object_set_data_full (G_OBJECT (device),
	                        DESCRIPTION_TAG, description,
	                        (GDestroyNotify) g_free);
}

/**
 * nma_utils_get_device_vendor:
 * @device: an #NMDevice
 *
 * Gets a cleaned-up version of #NMDevice:vendor for @device. This
 * removes strings like "Inc." that would just take up unnecessary
 * space in the UI.
 *
 * Returns: a cleaned-up vendor string, or %NULL if the vendor is
 *   not known
 */
const char *
nma_utils_get_device_vendor (NMDevice *device)
{
	const char *vendor;

	g_return_val_if_fail (device != NULL, NULL);

	vendor = g_object_get_data (G_OBJECT (device), VENDOR_TAG);
	if (!vendor) {
		get_description (device);
		vendor = g_object_get_data (G_OBJECT (device), VENDOR_TAG);
	}

	return vendor;
}

/**
 * nma_utils_get_device_product:
 * @device: an #NMDevice
 *
 * Gets a cleaned-up version of #NMDevice:product for @device. This
 * removes strings like "Wireless LAN Adapter" that would just take up
 * unnecessary space in the UI.
 *
 * Returns: a cleaned-up product string, or %NULL if the product name
 *   is not known
 */
const char *
nma_utils_get_device_product (NMDevice *device)
{
	const char *product;

	g_return_val_if_fail (device != NULL, NULL);

	product = g_object_get_data (G_OBJECT (device), PRODUCT_TAG);
	if (!product) {
		get_description (device);
		product = g_object_get_data (G_OBJECT (device), PRODUCT_TAG);
	}

	return product;
}

/**
 * nma_utils_get_device_description:
 * @device: an #NMDevice
 *
 * Gets a description of @device, incorporating the results of
 * nma_utils_get_device_vendor() and
 * nma_utils_get_device_product().
 *
 * Returns: a description of @device. If either the vendor or the
 *   product name is unknown, this returns the interface name.
 */
const char *
nma_utils_get_device_description (NMDevice *device)
{
	const char *description;

	g_return_val_if_fail (device != NULL, NULL);

	description = g_object_get_data (G_OBJECT (device), DESCRIPTION_TAG);
	if (!description) {
		get_description (device);
		description = g_object_get_data (G_OBJECT (device), DESCRIPTION_TAG);
	}

	return description;
}

static gboolean
find_duplicates (char     **names,
                 gboolean  *duplicates,
                 int        num_devices)
{
	int i, j;
	gboolean found_any = FALSE;

	memset (duplicates, 0, num_devices * sizeof (gboolean));
	for (i = 0; i < num_devices; i++) {
		if (duplicates[i])
			continue;
		for (j = i + 1; j < num_devices; j++) {
			if (duplicates[j])
				continue;
			if (!strcmp (names[i], names[j]))
				duplicates[i] = duplicates[j] = found_any = TRUE;
		}
	}

	return found_any;
}

/**
 * nma_utils_get_device_generic_type_name:
 * @device: an #NMDevice
 *
 * Gets a "generic" name for the type of @device.
 *
 * Returns: @device's generic type name
 */
const char *
nma_utils_get_device_generic_type_name (NMDevice *device)
{
	switch (nm_device_get_device_type (device)) {
	case NM_DEVICE_TYPE_ETHERNET:
	case NM_DEVICE_TYPE_INFINIBAND:
		return _("Wired");
	default:
		return nma_utils_get_device_type_name (device);
	}
}

/**
 * nma_utils_get_device_type_name:
 * @device: an #NMDevice
 *
 * Gets a specific name for the type of @device.
 *
 * Returns: @device's generic type name
 */
const char *
nma_utils_get_device_type_name (NMDevice *device)
{
	switch (nm_device_get_device_type (device)) {
	case NM_DEVICE_TYPE_ETHERNET:
		return _("Ethernet");
	case NM_DEVICE_TYPE_WIFI:
		return _("Wi-Fi");
	case NM_DEVICE_TYPE_BT:
		return _("Bluetooth");
	case NM_DEVICE_TYPE_OLPC_MESH:
		return _("OLPC Mesh");
	case NM_DEVICE_TYPE_MODEM:
		return _("Mobile Broadband");
	case NM_DEVICE_TYPE_INFINIBAND:
		return _("InfiniBand");
	case NM_DEVICE_TYPE_BOND:
		return _("Bond");
	case NM_DEVICE_TYPE_TEAM:
		return _("Team");
	case NM_DEVICE_TYPE_BRIDGE:
		return _("Bridge");
	case NM_DEVICE_TYPE_VLAN:
		return _("VLAN");
	case NM_DEVICE_TYPE_ADSL:
		return _("ADSL");
	default:
		return _("Unknown");
	}
}

static char *
get_device_type_name_with_iface (NMDevice *device)
{
	const char *type_name = nma_utils_get_device_type_name (device);

	switch (nm_device_get_device_type (device)) {
	case NM_DEVICE_TYPE_BOND:
	case NM_DEVICE_TYPE_TEAM:
	case NM_DEVICE_TYPE_BRIDGE:
	case NM_DEVICE_TYPE_VLAN:
		return g_strdup_printf ("%s (%s)", type_name, nm_device_get_iface (device));
	default:
		return g_strdup (type_name);
	}
}

static char *
get_device_generic_type_name_with_iface (NMDevice *device)
{
	switch (nm_device_get_device_type (device)) {
	case NM_DEVICE_TYPE_ETHERNET:
	case NM_DEVICE_TYPE_INFINIBAND:
		return g_strdup (_("Wired"));
	default:
		return get_device_type_name_with_iface (device);
	}
}

#define BUS_TAG "nm-ui-utils.c:get_bus_name"

static const char *
get_bus_name (GUdevClient *uclient, NMDevice *device)
{
	GUdevDevice *udevice;
	const char *ifname, *bus;
	char *display_bus;

	bus = g_object_get_data (G_OBJECT (device), BUS_TAG);
	if (bus) {
		if (*bus)
			return bus;
		else
			return NULL;
	}

	ifname = nm_device_get_iface (device);
	if (!ifname)
		return NULL;

	udevice = g_udev_client_query_by_subsystem_and_name (uclient, "net", ifname);
	if (!udevice)
		udevice = g_udev_client_query_by_subsystem_and_name (uclient, "tty", ifname);
	if (!udevice)
		return NULL;

	bus = g_udev_device_get_property (udevice, "ID_BUS");
	if (!g_strcmp0 (bus, "pci"))
		display_bus = g_strdup (_("PCI"));
	else if (!g_strcmp0 (bus, "usb"))
		display_bus = g_strdup (_("USB"));
	else {
		/* Use "" instead of NULL so we can tell later that we've
		 * already tried.
		 */
		display_bus = g_strdup ("");
	}

	g_object_set_data_full (G_OBJECT (device),
	                        BUS_TAG, display_bus,
	                        (GDestroyNotify) g_free);
	if (*display_bus)
		return display_bus;
	else
		return NULL;
}

/**
 * nma_utils_disambiguate_device_names:
 * @devices: (array length=num_devices): a set of #NMDevice
 * @num_devices: length of @devices
 *
 * Generates a list of short-ish unique presentation names for the
 * devices in @devices.
 *
 * Returns: (transfer full) (array zero-terminated=1): the device names
 */
char **
nma_utils_disambiguate_device_names (NMDevice **devices,
                                     int        num_devices)
{
	static const char *subsys[3] = { "net", "tty", NULL };
	GUdevClient *uclient;
	char **names;
	gboolean *duplicates;
	int i;

	names = g_new (char *, num_devices + 1);
	duplicates = g_new (gboolean, num_devices);

	/* Generic device name */
	for (i = 0; i < num_devices; i++)
		names[i] = get_device_generic_type_name_with_iface (devices[i]);
	if (!find_duplicates (names, duplicates, num_devices))
		goto done;

	/* Try specific names (eg, "Ethernet" and "InfiniBand" rather
	 * than "Wired")
	 */
	for (i = 0; i < num_devices; i++) {
		if (duplicates[i]) {
			g_free (names[i]);
			names[i] = get_device_type_name_with_iface (devices[i]);
		}
	}
	if (!find_duplicates (names, duplicates, num_devices))
		goto done;

	/* Try prefixing bus name (eg, "PCI Ethernet" vs "USB Ethernet") */
	uclient = g_udev_client_new (subsys);
	for (i = 0; i < num_devices; i++) {
		if (duplicates[i]) {
			const char *bus = get_bus_name (uclient, devices[i]);
			char *name;

			if (!bus)
				continue;

			g_free (names[i]);
			name = get_device_type_name_with_iface (devices[i]);
			/* Translators: the first %s is a bus name (eg, "USB") or
			 * product name, the second is a device type (eg,
			 * "Ethernet"). You can change this to something like
			 * "%2$s (%1$s)" if there's no grammatical way to combine
			 * the strings otherwise.
			 */
			names[i] = g_strdup_printf (C_("long device name", "%s %s"),
			                            bus, name);
			g_free (name);
		}
	}
	g_object_unref (uclient);
	if (!find_duplicates (names, duplicates, num_devices))
		goto done;

	/* Try prefixing vendor name */
	for (i = 0; i < num_devices; i++) {
		if (duplicates[i]) {
			const char *vendor = nma_utils_get_device_vendor (devices[i]);
			char *name;

			if (!vendor)
				continue;

			g_free (names[i]);
			name = get_device_type_name_with_iface (devices[i]);
			names[i] = g_strdup_printf (C_("long device name", "%s %s"),
			                            vendor,
			                            nma_utils_get_device_type_name (devices[i]));
			g_free (name);
		}
	}
	if (!find_duplicates (names, duplicates, num_devices))
		goto done;

	/* If dealing with Bluetooth devices, try to distinguish them by
	 * device name.
	 */
	for (i = 0; i < num_devices; i++) {
		if (duplicates[i] && NM_IS_DEVICE_BT (devices[i])) {
			const char *devname = nm_device_bt_get_name (NM_DEVICE_BT (devices[i]));

			if (!devname)
				continue;

			g_free (names[i]);
			names[i] = g_strdup_printf ("%s (%s)",
						    nma_utils_get_device_type_name (devices[i]),
						    devname);
		}
	}
	if (!find_duplicates (names, duplicates, num_devices))
		goto done;

	/* We have multiple identical network cards, so we have to differentiate
	 * them by interface name.
	 */
	for (i = 0; i < num_devices; i++) {
		if (duplicates[i]) {
			const char *interface = nm_device_get_iface (devices[i]);

			if (!interface)
				continue;

			g_free (names[i]);
			names[i] = g_strdup_printf ("%s (%s)",
			                            nma_utils_get_device_type_name (devices[i]),
			                            interface);
		}
	}

 done:
	g_free (duplicates);
	names[num_devices] = NULL;
	return names;
}

/**
 * nma_utils_get_connection_device_name:
 * @connection: an #NMConnection for a virtual device type
 *
 * Returns the name that nma_utils_disambiguate_device_names() would
 * return for the virtual device that would be created for @connection.
 * Eg, "VLAN (eth1.1)".
 *
 * Returns: (transfer full): the name of @connection's device
 */
char *
nma_utils_get_connection_device_name (NMConnection *connection)
{
	const char *iface, *type, *display_type;
	NMSettingConnection *s_con;

	iface = nm_connection_get_virtual_iface_name (connection);
	g_return_val_if_fail (iface != NULL, NULL);

	s_con = nm_connection_get_setting_connection (connection);
	g_return_val_if_fail (s_con != NULL, NULL);
	type = nm_setting_connection_get_connection_type (s_con);

	if (!strcmp (type, NM_SETTING_BOND_SETTING_NAME))
		display_type = _("Bond");
	else if (!strcmp (type, NM_SETTING_TEAM_SETTING_NAME))
		display_type = _("Team");
	else if (!strcmp (type, NM_SETTING_BRIDGE_SETTING_NAME))
		display_type = _("Bridge");
	else if (!strcmp (type, NM_SETTING_VLAN_SETTING_NAME))
		display_type = _("VLAN");
	else {
		g_warning ("Unrecognized virtual device type '%s'", type);
		display_type = type;
	}

	return g_strdup_printf ("%s (%s)", display_type, iface);
}

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

