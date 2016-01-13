/* NetworkManager -- Network link manager
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
 * (C) Copyright 2005 Red Hat, Inc.
 */

#ifndef GCONF_UPGRADE_H
#define GCONF_UPGRADE_H

#include <gconf/gconf-client.h>

void nm_gconf_migrate_0_6_connections (GConfClient *client);

void nm_gconf_migrate_0_7_wireless_security (GConfClient *client);

void nm_gconf_migrate_0_7_keyring_items (GConfClient *client);

void nm_gconf_migrate_0_7_netmask_to_prefix (GConfClient *client);

void nm_gconf_migrate_0_7_ip4_method (GConfClient *client);

void nm_gconf_migrate_0_7_ignore_dhcp_dns (GConfClient *client);

void nm_gconf_migrate_0_7_vpn_routes (GConfClient *client);

void nm_gconf_migrate_0_7_vpn_properties (GConfClient *client);

void nm_gconf_migrate_0_7_openvpn_properties (GConfClient *client);

void nm_gconf_migrate_0_7_connection_uuid (GConfClient *client);

void nm_gconf_migrate_0_7_vpn_never_default (GConfClient *client);

void nm_gconf_migrate_0_7_autoconnect_default (GConfClient *client);

void nm_gconf_migrate_0_7_ca_cert_ignore (GConfClient *client);

void nm_gconf_migrate_0_7_certs (GConfClient *client);

void nm_gconf_migrate_09_secret_flags (GConfClient *client,
                                       NMConnection *connection,
                                       const char *setting_name);

#endif	/* GCONF_UPGRADE_H */

