/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
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
 * Copyright 2009 - 2014444 Red Hat, Inc.
 */

#ifndef __CE_POLKIT_BUTTON_H__
#define __CE_POLKIT_BUTTON_H__

#include <gtk/gtk.h>

#include <NetworkManager.h>

#define CE_TYPE_POLKIT_BUTTON            (ce_polkit_button_get_type ())
#define CE_POLKIT_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CE_TYPE_POLKIT_BUTTON, CEPolkitButton))
#define CE_POKLIT_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CE_TYPE_POLKIT_BUTTON, CEPolkitButtonClass))
#define CE_IS_POLKIT_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CE_TYPE_POLKIT_BUTTON))
#define CE_IS_POLKIT_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CE_TYPE_POLKIT_BUTTON))
#define CE_POLKIT_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CE_TYPE_POLKIT_BUTTON, CEPolkitButtonClass))

typedef struct {
	GtkButton parent;
} CEPolkitButton;

typedef struct {
	GtkButtonClass parent;

	/* Signals */
	void (*actionable) (CEPolkitButton *self, gboolean actionable);
	
	void (*authorized) (CEPolkitButton *self, gboolean authorized);
} CEPolkitButtonClass;

GType ce_polkit_button_get_type (void);

GtkWidget *ce_polkit_button_new (const char *label,
                                 const char *tooltip,
                                 const char *auth_tooltip,
                                 const char *stock_icon,
                                 NMClient *client,
                                 NMClientPermission permission);

void ce_polkit_button_set_validation_error (CEPolkitButton *self, const char *validation_error);

gboolean ce_polkit_button_get_actionable (CEPolkitButton *button);

gboolean ce_polkit_button_get_authorized (CEPolkitButton *button);

#endif  /* __CE_POLKIT_BUTTON_H__ */

