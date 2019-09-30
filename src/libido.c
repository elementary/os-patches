/*
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Lars Uebernickel <lars.uebernickel@canonical.com>
 */

#include <gio/gio.h>

/**
 * ido_init:
 *
 * Initializes ido. It has to be called after gtk_init(), but before any
 * other calls into ido are made.
 */
void
ido_init (void)
{
  GType ido_menu_item_factory_get_type (void);

  /* make sure this extension point is registered so that gtk calls it
   * when finding custom menu items */
  g_type_ensure (ido_menu_item_factory_get_type ());
}
