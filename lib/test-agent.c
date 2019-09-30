/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "bluetooth-agent.h"
#include "bluetooth-client-glue.h"

static gboolean
agent_pincode (GDBusMethodInvocation *invocation,
	       GDBusProxy *device,
	       gpointer user_data)
{
	GVariant *value;
	GVariant *result;
	const char *alias, *address;

	result = g_dbus_proxy_call_sync (device, "GetProperties",  NULL,
					 G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
	if (result != NULL) {
		value = g_variant_lookup_value (result, "Address", G_VARIANT_TYPE_STRING);
		address = value ? g_variant_get_string (value, NULL) : "No address";

		value = g_variant_lookup_value (result, "Name", G_VARIANT_TYPE_STRING);
		alias = value ? g_variant_get_string (value, NULL) : address;

		printf("address %s name %s\n", address, alias);

		g_variant_unref (result);
	} else {
		g_message ("Could not get address or name for '%s'",
			   g_dbus_proxy_get_object_path (device));
	}

	g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", "1234"));

	return TRUE;
}

static GMainLoop *mainloop = NULL;

static void
sig_term (int sig)
{
	g_main_loop_quit(mainloop);
}

int main (int argc, char **argv)
{
	struct sigaction sa;
	BluetoothAgent *agent;

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_NOCLDSTOP;
	sa.sa_handler = sig_term;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT,  &sa, NULL);

	g_type_init();

	mainloop = g_main_loop_new(NULL, FALSE);

	agent = bluetooth_agent_new();

	bluetooth_agent_set_pincode_func(agent, agent_pincode, NULL);

	bluetooth_agent_register(agent);

	g_main_loop_run(mainloop);

	bluetooth_agent_unregister(agent);

	g_object_unref(agent);

	g_main_loop_unref(mainloop);

	return 0;
}
