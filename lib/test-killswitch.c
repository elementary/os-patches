#include <glib-object.h>
#include "bluetooth-killswitch.h"

static void
state_changed_cb (BluetoothKillswitch *killswitch,
		  BluetoothKillswitchState state,
		  gpointer user_data)
{
	g_message ("killswitch changed to state '%s'",
		   bluetooth_killswitch_state_to_string (state));
}

int main (int argc, char **argv)
{
	GMainLoop *mainloop;
	BluetoothKillswitch *ks;

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_type_init ();

	mainloop = g_main_loop_new (NULL, FALSE);

	ks = bluetooth_killswitch_new ();
	if (bluetooth_killswitch_has_killswitches (ks) == FALSE) {
		g_message ("No killswitches");
		return 0;
	}
	g_signal_connect (G_OBJECT (ks), "state-changed",
			  G_CALLBACK (state_changed_cb), NULL);

	g_main_loop_run (mainloop);

	return 0;
}
