#include "pin.c"
#include "bluetooth-enums.h"

int main (int argc, char **argv)
{
	guint max_digits;
	gboolean confirm;
	char *pin;

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

#if 0
	pin = get_pincode_for_device (BLUETOOTH_TYPE_KEYBOARD,
				      "00:13:6C:00:00:00",
				      "TomTom Remote",
				      &max_digits,
				      &confirm);
#endif
	pin = get_pincode_for_device (BLUETOOTH_TYPE_MOUSE,
				      "0C:77:1A:00:00:00",
				      "Some Apple mouse",
				      &max_digits,
				      &confirm);

	g_message ("pin: %s max digits: %d confirm: %d",
		   pin, max_digits, confirm);

	return 0;
}
