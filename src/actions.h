/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
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
 */

#ifndef __INDICATOR_SESSION_ACTIONS_H__
#define __INDICATOR_SESSION_ACTIONS_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/* standard GObject macros */
#define INDICATOR_TYPE_SESSION_ACTIONS          (indicator_session_actions_get_type())
#define INDICATOR_SESSION_ACTIONS(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_SESSION_ACTIONS, IndicatorSessionActions))
#define INDICATOR_SESSION_ACTIONS_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_SESSION_ACTIONS, IndicatorSessionActionsClass))
#define INDICATOR_SESSION_ACTIONS_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST ((k), INDICATOR_TYPE_SESSION_ACTIONS, IndicatorSessionActionsClass))
#define INDICATOR_IS_SESSION_ACTIONS(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_SESSION_ACTIONS))

typedef struct _IndicatorSessionActions      IndicatorSessionActions;
typedef struct _IndicatorSessionActionsClass IndicatorSessionActionsClass;

/* property keys */
#define INDICATOR_SESSION_ACTIONS_PROP_CAN_LOCK "can-lock"
#define INDICATOR_SESSION_ACTIONS_PROP_CAN_LOGOUT "can-logout"
#define INDICATOR_SESSION_ACTIONS_PROP_CAN_REBOOT "can-reboot"
#define INDICATOR_SESSION_ACTIONS_PROP_CAN_SWITCH "can-switch"
#define INDICATOR_SESSION_ACTIONS_PROP_CAN_SUSPEND "can-suspend"
#define INDICATOR_SESSION_ACTIONS_PROP_CAN_HIBERNATE "can-hibernate"
#define INDICATOR_SESSION_ACTIONS_PROP_CAN_PROMPT "can-show-end-session-dialog"
#define INDICATOR_SESSION_ACTIONS_PROP_HAS_ONLINE_ACCOUNT_ERROR "has-online-account-error"

/**
 * A base class for invoking and getting state information on system actions.
 * Use backend.h's get_backend() to get an instance.
 */
struct _IndicatorSessionActions
{
  /*< private >*/
  GObject parent;
};

struct _IndicatorSessionActionsClass
{
  GObjectClass parent_class;

  /* pure virtual functions */

  gboolean (*can_lock)                 (IndicatorSessionActions * self);
  gboolean (*can_logout)               (IndicatorSessionActions * self);
  gboolean (*can_reboot)               (IndicatorSessionActions * self);
  gboolean (*can_switch)               (IndicatorSessionActions * self);
  gboolean (*can_suspend)              (IndicatorSessionActions * self);
  gboolean (*can_hibernate)            (IndicatorSessionActions * self);
  gboolean (*can_prompt)               (IndicatorSessionActions * self);
  gboolean (*has_online_account_error) (IndicatorSessionActions * self);

  void  (*suspend)                     (IndicatorSessionActions * self);
  void  (*hibernate)                   (IndicatorSessionActions * self);
  void  (*logout)                      (IndicatorSessionActions * self);
  void  (*reboot)                      (IndicatorSessionActions * self);
  void  (*power_off)                   (IndicatorSessionActions * self);
  void  (*help)                        (IndicatorSessionActions * self);
  void  (*about)                       (IndicatorSessionActions * self);
  void  (*settings)                    (IndicatorSessionActions * self);
  void  (*online_accounts)             (IndicatorSessionActions * self);

  void  (*switch_to_greeter)           (IndicatorSessionActions * self);
  void  (*switch_to_screensaver)       (IndicatorSessionActions * self);
  void  (*switch_to_guest)             (IndicatorSessionActions * self);
  void  (*switch_to_username)          (IndicatorSessionActions * self,
                                        const gchar             * username);
};

/***
****
***/

GType indicator_session_actions_get_type (void);

gboolean indicator_session_actions_can_lock                    (IndicatorSessionActions * self);
gboolean indicator_session_actions_can_logout                  (IndicatorSessionActions * self);
gboolean indicator_session_actions_can_reboot                  (IndicatorSessionActions * self);
gboolean indicator_session_actions_can_switch                  (IndicatorSessionActions * self);
gboolean indicator_session_actions_can_suspend                 (IndicatorSessionActions * self);
gboolean indicator_session_actions_can_hibernate               (IndicatorSessionActions * self);
gboolean indicator_session_actions_can_prompt                  (IndicatorSessionActions * self);
gboolean indicator_session_actions_has_online_account_error    (IndicatorSessionActions * self);


void indicator_session_actions_notify_can_lock                 (IndicatorSessionActions * self);
void indicator_session_actions_notify_can_logout               (IndicatorSessionActions * self);
void indicator_session_actions_notify_can_reboot               (IndicatorSessionActions * self);
void indicator_session_actions_notify_can_switch               (IndicatorSessionActions * self);
void indicator_session_actions_notify_can_suspend              (IndicatorSessionActions * self);
void indicator_session_actions_notify_can_hibernate            (IndicatorSessionActions * self);
void indicator_session_actions_notify_can_prompt               (IndicatorSessionActions * self);
void indicator_session_actions_notify_has_online_account_error (IndicatorSessionActions * self);

void indicator_session_actions_lock                            (IndicatorSessionActions * self);
void indicator_session_actions_suspend                         (IndicatorSessionActions * self);
void indicator_session_actions_hibernate                       (IndicatorSessionActions * self);
void indicator_session_actions_logout                          (IndicatorSessionActions * self);
void indicator_session_actions_reboot                          (IndicatorSessionActions * self);
void indicator_session_actions_power_off                       (IndicatorSessionActions * self);

void indicator_session_actions_help                            (IndicatorSessionActions * self);
void indicator_session_actions_about                           (IndicatorSessionActions * self);
void indicator_session_actions_settings                        (IndicatorSessionActions * self);
void indicator_session_actions_online_accounts                 (IndicatorSessionActions * self);

void indicator_session_actions_switch_to_screensaver           (IndicatorSessionActions * self);
void indicator_session_actions_switch_to_greeter               (IndicatorSessionActions * self);
void indicator_session_actions_switch_to_guest                 (IndicatorSessionActions * self);
void indicator_session_actions_switch_to_username              (IndicatorSessionActions * self,
                                                               const gchar              * username);

G_END_DECLS

#endif /* __INDICATOR_SESSION_ACTIONS_H__ */
