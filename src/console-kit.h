/*
 * Copyright (C) 2010-2011 Robert Ancell.
 * Author: Robert Ancell <robert.ancell@canonical.com>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See http://www.gnu.org/copyleft/gpl.html the full text of the
 * license.
 */

#ifndef CONSOLE_KIT_H_
#define CONSOLE_KIT_H_

#include <glib-object.h>

G_BEGIN_DECLS

gchar *ck_open_session (GVariantBuilder *parameters);

void ck_lock_session (const gchar *cookie);

void ck_unlock_session (const gchar *cookie);

void ck_activate_session (const gchar *cookie);

void ck_close_session (const gchar *cookie);

gchar *ck_get_xdg_runtime_dir (const gchar *cookie);

G_END_DECLS

#endif /* CONSOLE_KIT_H_ */
