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

#ifndef __messaging_menu_app_h__
#define __messaging_menu_app_h__

#include <gio/gio.h>
#include "messaging-menu-message.h"

G_BEGIN_DECLS

#define MESSAGING_MENU_TYPE_APP         messaging_menu_app_get_type()
#define MESSAGING_MENU_APP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MESSAGING_MENU_TYPE_APP, MessagingMenuApp))
#define MESSAGING_MENU_APP_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), MESSAGING_MENU_TYPE_APP, MessagingMenuAppClass))
#define MESSAGING_MENU_IS_APP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MESSAGING_MENU_TYPE_APP))

/**
 * MessagingMenuStatus:
 * @MESSAGING_MENU_STATUS_AVAILABLE: available
 * @MESSAGING_MENU_STATUS_AWAY: away
 * @MESSAGING_MENU_STATUS_BUSY: busy
 * @MESSAGING_MENU_STATUS_INVISIBLE: invisible
 * @MESSAGING_MENU_STATUS_OFFLINE: offline
 *
 * An enumeration for the possible chat statuses the messaging menu can be in.
 */
typedef enum {
  MESSAGING_MENU_STATUS_AVAILABLE,
  MESSAGING_MENU_STATUS_AWAY,
  MESSAGING_MENU_STATUS_BUSY,
  MESSAGING_MENU_STATUS_INVISIBLE,
  MESSAGING_MENU_STATUS_OFFLINE
} MessagingMenuStatus;


typedef GObjectClass             MessagingMenuAppClass;
typedef struct _MessagingMenuApp MessagingMenuApp;

GType               messaging_menu_app_get_type                  (void) G_GNUC_CONST;

MessagingMenuApp *  messaging_menu_app_new                       (const gchar *desktop_id);

void                messaging_menu_app_register                  (MessagingMenuApp *app);
void                messaging_menu_app_unregister                (MessagingMenuApp *app);

void                messaging_menu_app_set_status                (MessagingMenuApp   *app,
                                                                  MessagingMenuStatus status);

void                messaging_menu_app_insert_source             (MessagingMenuApp *app,
                                                                  gint              position,
                                                                  const gchar      *id,
                                                                  GIcon            *icon,
                                                                  const gchar      *label);

void                messaging_menu_app_append_source             (MessagingMenuApp *app,
                                                                  const gchar      *id,
                                                                  GIcon            *icon,
                                                                  const gchar      *label);

void                messaging_menu_app_insert_source_with_count  (MessagingMenuApp *app,
                                                                  gint              position,
                                                                  const gchar      *id,
                                                                  GIcon            *icon,
                                                                  const gchar      *label,
                                                                  guint             count);

void                messaging_menu_app_append_source_with_count  (MessagingMenuApp *app,
                                                                  const gchar      *id,
                                                                  GIcon            *icon,
                                                                  const gchar      *label,
                                                                  guint             count);

void                messaging_menu_app_insert_source_with_time   (MessagingMenuApp *app,
                                                                  gint              position,
                                                                  const gchar      *id,
                                                                  GIcon            *icon,
                                                                  const gchar      *label,
                                                                  gint64            time);

void                messaging_menu_app_append_source_with_time   (MessagingMenuApp *app,
                                                                  const gchar      *id,
                                                                  GIcon            *icon,
                                                                  const gchar      *label,
                                                                  gint64            time);

void                messaging_menu_app_append_source_with_string (MessagingMenuApp *app,
                                                                  const gchar      *id,
                                                                  GIcon            *icon,
                                                                  const gchar      *label,
                                                                  const gchar      *str);

void                messaging_menu_app_insert_source_with_string (MessagingMenuApp *app,
                                                                  gint              position,
                                                                  const gchar      *id,
                                                                  GIcon            *icon,
                                                                  const gchar      *label,
                                                                  const gchar      *str);

void                messaging_menu_app_remove_source             (MessagingMenuApp *app,
                                                                  const gchar      *source_id);

gboolean            messaging_menu_app_has_source                (MessagingMenuApp *app,
                                                                  const gchar      *source_id);

void                messaging_menu_app_set_source_label          (MessagingMenuApp *app,
                                                                  const gchar      *source_id,
                                                                  const gchar      *label);

void                messaging_menu_app_set_source_icon           (MessagingMenuApp *app,
                                                                  const gchar      *source_id,
                                                                  GIcon            *icon);

void                messaging_menu_app_set_source_count          (MessagingMenuApp *app,
                                                                  const gchar      *source_id,
                                                                  guint             count);

void                messaging_menu_app_set_source_time           (MessagingMenuApp *app,
                                                                  const gchar      *source_id,
                                                                  gint64            time);

void                messaging_menu_app_set_source_string         (MessagingMenuApp *app,
                                                                  const gchar      *source_id,
                                                                  const gchar      *str);

void                messaging_menu_app_draw_attention            (MessagingMenuApp *app,
                                                                  const gchar      *source_id);

void                messaging_menu_app_remove_attention          (MessagingMenuApp *app,
                                                                  const gchar      *source_id);

void                messaging_menu_app_append_message            (MessagingMenuApp     *app,
                                                                  MessagingMenuMessage *msg,
                                                                  const gchar          *source_id,
                                                                  gboolean              notify);

MessagingMenuMessage * messaging_menu_app_get_message            (MessagingMenuApp *app,
                                                                  const gchar      *id);

void                messaging_menu_app_remove_message            (MessagingMenuApp     *app,
                                                                  MessagingMenuMessage *msg);

void                messaging_menu_app_remove_message_by_id      (MessagingMenuApp     *app,
                                                                  const gchar          *id);

G_END_DECLS

#endif
