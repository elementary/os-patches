/*
 * Copyright 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as
 * published by the Free Software Foundation.
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

#ifndef __messaging_menu_message_h__
#define __messaging_menu_message_h__

#include <gio/gio.h>

G_BEGIN_DECLS

#define MESSAGING_MENU_TYPE_MESSAGE            (messaging_menu_message_get_type ())
#define MESSAGING_MENU_MESSAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MESSAGING_MENU_TYPE_MESSAGE, MessagingMenuMessage))
#define MESSAGING_MENU_MESSAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MESSAGING_MENU_TYPE_MESSAGE, MessagingMenuMessageClass))
#define MESSAGING_MENU_IS_MESSAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MESSAGING_MENU_TYPE_MESSAGE))
#define MESSAGING_MENU_IS_MESSAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MESSAGING_MENU_TYPE_MESSAGE))
#define MESSAGING_MENU_MESSAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MESSAGING_MENU_TYPE_MESSAGE, MessagingMenuMessageClass))

typedef struct _MessagingMenuMessage MessagingMenuMessage;

GType                   messaging_menu_message_get_type             (void) G_GNUC_CONST;

MessagingMenuMessage *  messaging_menu_message_new                  (const gchar *id,
                                                                     GIcon       *icon,
                                                                     const gchar *title,
                                                                     const gchar *subtitle,
                                                                     const gchar *body,
                                                                     gint64       time);

const gchar *           messaging_menu_message_get_id               (MessagingMenuMessage *msg);

GIcon *                 messaging_menu_message_get_icon             (MessagingMenuMessage *msg);

const gchar *           messaging_menu_message_get_title            (MessagingMenuMessage *msg);

const gchar *           messaging_menu_message_get_subtitle         (MessagingMenuMessage *msg);

const gchar *           messaging_menu_message_get_body             (MessagingMenuMessage *msg);

gint64                  messaging_menu_message_get_time             (MessagingMenuMessage *msg);

gboolean                messaging_menu_message_get_draws_attention  (MessagingMenuMessage *msg);

void                    messaging_menu_message_set_draws_attention  (MessagingMenuMessage *msg,
                                                                     gboolean              draws_attention);

void                    messaging_menu_message_add_action           (MessagingMenuMessage *msg,
                                                                     const gchar          *id,
                                                                     const gchar          *label,
                                                                     const GVariantType   *parameter_type,
                                                                     GVariant             *parameter_hint);

G_END_DECLS

#endif
