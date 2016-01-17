/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2006-2009  Bastien Nocera <hadess@hadess.net>
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

/**
 * SECTION:bluetooth-killswitch
 * @short_description: a Bluetooth killswitch object
 * @stability: Stable
 * @include: bluetooth-killswitch.h
 *
 * An object to manipulate Bluetooth killswitches.
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <glib.h>

#include "bluetooth-killswitch.h"
#include "rfkill-glib.h"

enum {
	STATE_CHANGED,
	LAST_SIGNAL
};

static int signals[LAST_SIGNAL] = { 0 };

#define BLUETOOTH_KILLSWITCH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
				BLUETOOTH_TYPE_KILLSWITCH, BluetoothKillswitchPrivate))

typedef struct _BluetoothIndKillswitch BluetoothIndKillswitch;
struct _BluetoothIndKillswitch {
	guint index;
	BluetoothKillswitchState state;
};

struct _BluetoothKillswitchPrivate {
	RfkillGlib *rfkill;
	gboolean in_init;
	GList *killswitches; /* a GList of BluetoothIndKillswitch */
	BluetoothKillswitchPrivate *priv;
};

G_DEFINE_TYPE(BluetoothKillswitch, bluetooth_killswitch, G_TYPE_OBJECT)

static BluetoothKillswitchState
event_to_state (guint soft, guint hard)
{
	if (hard)
		return BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED;
	else if (soft)
		return BLUETOOTH_KILLSWITCH_STATE_SOFT_BLOCKED;
	else
		return BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED;
}

static const char *
state_to_string (BluetoothKillswitchState state)
{
	switch (state) {
	case BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER:
		return "no-adapter";
	case BLUETOOTH_KILLSWITCH_STATE_SOFT_BLOCKED:
		return "soft-blocked";
	case BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED:
		return "unblocked";
	case BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED:
		return "hard-blocked";
	default:
		g_assert_not_reached ();
	}
}

const char *
bluetooth_killswitch_state_to_string (BluetoothKillswitchState state)
{
	g_return_val_if_fail (state >= BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER, NULL);
	g_return_val_if_fail (state <= BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED, NULL);

	return state_to_string (state);
}

static void
update_killswitch (BluetoothKillswitch *killswitch,
		   guint index, guint soft, guint hard)
{
	BluetoothKillswitchPrivate *priv;
	gboolean changed;
	GList *l;

	priv = killswitch->priv;
	changed = FALSE;

	for (l = priv->killswitches; l != NULL; l = l->next) {
		BluetoothIndKillswitch *ind = l->data;

		if (ind->index == index) {
			BluetoothKillswitchState state = event_to_state (soft, hard);
			if (state != ind->state) {
				ind->state = state;
				changed = TRUE;
			}
			break;
		}
	}

	if (changed != FALSE) {
		g_debug ("updating killswitch status %d to %s",
			 index, state_to_string (bluetooth_killswitch_get_state (killswitch)));
		g_signal_emit (G_OBJECT (killswitch),
			       signals[STATE_CHANGED],
			       0, bluetooth_killswitch_get_state (killswitch));
	}
}

void
bluetooth_killswitch_set_state (BluetoothKillswitch *killswitch,
				BluetoothKillswitchState state)
{
	BluetoothKillswitchPrivate *priv;
	struct rfkill_event event;
	ssize_t len;

	g_return_if_fail (BLUETOOTH_IS_KILLSWITCH (killswitch));
	g_return_if_fail (state != BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED);

	priv = killswitch->priv;

	memset (&event, 0, sizeof(event));
	event.op = RFKILL_OP_CHANGE_ALL;
	event.type = RFKILL_TYPE_BLUETOOTH;
	if (state == BLUETOOTH_KILLSWITCH_STATE_SOFT_BLOCKED)
		event.soft = 1;
	else if (state == BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED)
		event.soft = 0;
	else
		g_assert_not_reached ();

	len = rfkill_glib_send_event (priv->rfkill, &event);
	if (len < 0)
		g_warning ("Failed to change RFKILL state: %s",
			   g_strerror (errno));
}

BluetoothKillswitchState
bluetooth_killswitch_get_state (BluetoothKillswitch *killswitch)
{
	BluetoothKillswitchPrivate *priv;
	int state = BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED;
	GList *l;

	g_return_val_if_fail (BLUETOOTH_IS_KILLSWITCH (killswitch), state);

	priv = killswitch->priv;

	if (priv->killswitches == NULL)
		return BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER;

	for (l = priv->killswitches ; l ; l = l->next) {
		BluetoothIndKillswitch *ind = l->data;

		g_debug ("killswitch %d is %s",
			 ind->index, state_to_string (ind->state));

		if (ind->state == BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED) {
			state = BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED;
			break;
		}

		if (ind->state == BLUETOOTH_KILLSWITCH_STATE_SOFT_BLOCKED) {
			state = BLUETOOTH_KILLSWITCH_STATE_SOFT_BLOCKED;
			continue;
		}

		state = ind->state;
	}

	g_debug ("killswitches state %s", state_to_string (state));

	return state;
}

gboolean
bluetooth_killswitch_has_killswitches (BluetoothKillswitch *killswitch)
{
	BluetoothKillswitchPrivate *priv = killswitch->priv;

	g_return_val_if_fail (BLUETOOTH_IS_KILLSWITCH (killswitch), FALSE);

	return (priv->killswitches != NULL);
}

static void
remove_killswitch (BluetoothKillswitch *killswitch,
		   guint index)
{
	BluetoothKillswitchPrivate *priv;
	GList *l;

	priv = killswitch->priv;

	for (l = priv->killswitches; l != NULL; l = l->next) {
		BluetoothIndKillswitch *ind = l->data;
		if (ind->index == index) {
			priv->killswitches = g_list_remove (priv->killswitches, ind);
			g_debug ("removing killswitch idx %d", index);
			g_free (ind);
			g_signal_emit (G_OBJECT (killswitch),
				       signals[STATE_CHANGED],
				       0, bluetooth_killswitch_get_state (killswitch));
			return;
		}
	}
}

static void
add_killswitch (BluetoothKillswitch *killswitch,
		guint index,
		BluetoothKillswitchState state)

{
	BluetoothKillswitchPrivate *priv;
	BluetoothIndKillswitch *ind;

	priv = killswitch->priv;

	g_debug ("adding killswitch idx %d state %s", index, state_to_string (state));
	ind = g_new0 (BluetoothIndKillswitch, 1);
	ind->index = index;
	ind->state = state;
	priv->killswitches = g_list_append (priv->killswitches, ind);
}

static void
killswitch_changed (RfkillGlib          *rfkill,
		    GList               *events,
		    BluetoothKillswitch *killswitch)
{
	BluetoothKillswitchState state;
	gboolean changed;
	GList *l;

	if (killswitch->priv->in_init) {
		for (l = events; l != NULL; l = l->next) {
			struct rfkill_event *event = l->data;

			if (event->op != RFKILL_OP_ADD)
				continue;
			if (event->type != RFKILL_TYPE_BLUETOOTH)
				continue;

			state = event_to_state (event->soft, event->hard);

			g_debug ("Read killswitch (idx=%d): %s",
				 event->idx, bluetooth_killswitch_state_to_string (state));

			add_killswitch (killswitch, event->idx, state);
		}

		g_signal_emit (G_OBJECT (killswitch),
			       signals[STATE_CHANGED],
			       0, bluetooth_killswitch_get_state (killswitch));
		return;
	}

	changed = FALSE;

	/* Gather the previous killswitch so we can
	 * see what really changed */
	state = bluetooth_killswitch_get_state (killswitch);

	for (l = events; l != NULL; l = l->next) {
		struct rfkill_event *event = l->data;

		if (event->type != RFKILL_TYPE_BLUETOOTH &&
		    event->type != RFKILL_TYPE_ALL)
			continue;

		if (event->op == RFKILL_OP_CHANGE) {
			update_killswitch (killswitch, event->idx, event->soft, event->hard);
			/* No changed here because update_killswitch
			 * handles sending the state-changed signal */
		} else if (event->op == RFKILL_OP_DEL) {
			remove_killswitch (killswitch, event->idx);
			changed = TRUE;
		} else if (event->op == RFKILL_OP_ADD) {
			BluetoothKillswitchState state;
			state = event_to_state (event->soft, event->hard);
			add_killswitch (killswitch, event->idx, state);
			changed = TRUE;
		}
	}

	if (changed) {
		BluetoothKillswitchState new_state;

		new_state = bluetooth_killswitch_get_state (killswitch);
		if (new_state != state) {
			g_signal_emit (G_OBJECT (killswitch),
				       signals[STATE_CHANGED],
				       0, bluetooth_killswitch_get_state (killswitch));
		}
	}
}

static void
bluetooth_killswitch_init (BluetoothKillswitch *killswitch)
{
	BluetoothKillswitchPrivate *priv;

	priv = BLUETOOTH_KILLSWITCH_GET_PRIVATE (killswitch);
	killswitch->priv = priv;

	priv->rfkill = rfkill_glib_new ();
	g_signal_connect (priv->rfkill, "changed",
			  G_CALLBACK (killswitch_changed), killswitch);

	priv->in_init = TRUE;
	if (rfkill_glib_open (priv->rfkill) < 0)
		return;
	priv->in_init = FALSE;
}

static void
bluetooth_killswitch_finalize (GObject *object)
{
	BluetoothKillswitch *killswitch;

	killswitch = BLUETOOTH_KILLSWITCH (object);

	g_clear_object (&killswitch->priv->rfkill);

	g_list_free_full (killswitch->priv->killswitches, g_free);
	killswitch->priv->killswitches = NULL;

	G_OBJECT_CLASS(bluetooth_killswitch_parent_class)->finalize(object);
}

static void
bluetooth_killswitch_class_init(BluetoothKillswitchClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	g_type_class_add_private(klass, sizeof(BluetoothKillswitchPrivate));
	object_class->finalize = bluetooth_killswitch_finalize;

	signals[STATE_CHANGED] =
		g_signal_new ("state-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (BluetoothKillswitchClass, state_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

}

BluetoothKillswitch *
bluetooth_killswitch_new (void)
{
	return BLUETOOTH_KILLSWITCH (g_object_new (BLUETOOTH_TYPE_KILLSWITCH, NULL));
}
