/* vim: set et ts=8 sw=8: */
/*
 * Copyright 2014 Red Hat, Inc.
 *
 * Geoclue is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Geoclue is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Geoclue; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 */

#ifndef GCLUE_MODEM_MANAGER_H
#define GCLUE_MODEM_MANAGER_H

#include <gio/gio.h>
#include "gclue-modem.h"

G_BEGIN_DECLS

GType gclue_modem_manager_get_type (void) G_GNUC_CONST;

#define GCLUE_TYPE_MODEM_MANAGER            (gclue_modem_manager_get_type ())
#define GCLUE_MODEM_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_MODEM_MANAGER, GClueModemManager))
#define GCLUE_IS_MODEM_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_MODEM_MANAGER))
#define GCLUE_MODEM_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GCLUE_TYPE_MODEM_MANAGER, GClueModemManagerClass))
#define GCLUE_IS_MODEM_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GCLUE_TYPE_MODEM_MANAGER))
#define GCLUE_MODEM_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GCLUE_TYPE_MODEM_MANAGER, GClueModemManagerClass))

/**
 * GClueModemManager:
 *
 * All the fields in the #GClueModemManager structure are private and should never be accessed directly.
**/
typedef struct _GClueModemManager        GClueModemManager;
typedef struct _GClueModemManagerClass   GClueModemManagerClass;
typedef struct _GClueModemManagerPrivate GClueModemManagerPrivate;

struct _GClueModemManager {
        /* <private> */
        GObject parent_instance;
        GClueModemManagerPrivate *priv;
};

/**
 * GClueModemManagerClass:
 *
 * All the fields in the #GClueModemManagerClass structure are private and should never be accessed directly.
**/
struct _GClueModemManagerClass {
        /* <private> */
        GObjectClass parent_class;
};

GClueModem* gclue_modem_manager_get_singleton (void);

G_END_DECLS

#endif /* GCLUE_MODEM_MANAGER_H */
