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

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "backend.h"
#include "recoverable-problem.h"
#include "service.h"

#define BUS_NAME "com.canonical.indicator.session"
#define BUS_PATH "/com/canonical/indicator/session"

#define ICON_DEFAULT "system-devices-panel"
#define ICON_INFO    "system-devices-panel-information"
#define ICON_ALERT   "system-devices-panel-alert"

G_DEFINE_TYPE (IndicatorSessionService,
               indicator_session_service,
               G_TYPE_OBJECT)

/* signals enum */
enum
{
  NAME_LOST,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum
{
  PROP_0,
  PROP_MAX_USERS,
  PROP_LAST
};

static GParamSpec * properties[PROP_LAST];

enum
{
  SECTION_HEADER    = (1<<0),
  SECTION_ADMIN     = (1<<1),
  SECTION_SETTINGS  = (1<<2),
  SECTION_SWITCH    = (1<<3),
  SECTION_LOGOUT    = (1<<4),
  SECTION_SESSION   = (1<<5)
};

enum
{
  PROFILE_DESKTOP,
  PROFILE_GREETER,
  PROFILE_LOCKSCREEN,
  N_PROFILES
};

static const char * const menu_names[N_PROFILES] =
{
  "desktop",
  "desktop_greeter",
  "desktop_lockscreen"
};

struct ProfileMenuInfo
{
  /* the root level -- the header is the only child of this */
  GMenu * menu;

  /* parent of the sections. This is the header's submenu */
  GMenu * submenu;

  guint export_id;
};

struct _IndicatorSessionServicePrivate
{
  guint own_id;
  guint max_users;
  IndicatorSessionUsers * backend_users;
  IndicatorSessionGuest * backend_guest;
  IndicatorSessionActions * backend_actions;
  GSettings * indicator_settings;
  GSettings * keybinding_settings;
  GSimpleActionGroup * actions;
  guint actions_export_id;
  struct ProfileMenuInfo menus[N_PROFILES];
  GSimpleAction * header_action;
  GSimpleAction * user_switcher_action;
  GSimpleAction * guest_switcher_action;
  GHashTable * users;
  GHashTable * reported_users;
  guint rebuild_id;
  int rebuild_flags;
  GDBusConnection * conn;
  GCancellable * cancellable;
  GVariant * default_icon_serialized;
};

typedef IndicatorSessionServicePrivate priv_t;

static const char * get_current_real_name (IndicatorSessionService * self);

/***
****
***/

static void rebuild_now (IndicatorSessionService * self, int section);
static void rebuild_soon (IndicatorSessionService * self, int section);

static inline void
rebuild_header_soon (IndicatorSessionService * self)
{
  rebuild_soon (self, SECTION_HEADER);
}
static inline void
rebuild_switch_section_soon (IndicatorSessionService * self)
{
  rebuild_soon (self, SECTION_SWITCH);
}
static inline void
rebuild_logout_section_soon (IndicatorSessionService * self)
{
  rebuild_soon (self, SECTION_LOGOUT);
}
static inline void
rebuild_session_section_soon (IndicatorSessionService * self)
{
  rebuild_soon (self, SECTION_SESSION);
}
static inline void
rebuild_settings_section_soon (IndicatorSessionService * self)
{
  rebuild_soon (self, SECTION_SETTINGS);
}

/***
****
***/

static gboolean
show_user_list (IndicatorSessionService * self)
{
  return g_settings_get_boolean (self->priv->indicator_settings,
                                 "user-show-menu");
}


static GVariant *
action_state_for_header (IndicatorSessionService * self)
{
  const priv_t * const p = self->priv;
  gboolean show_name;
  const gchar * real_name;
  const gchar * label;
  gchar * a11y;
  GVariantBuilder b;
  GVariant * state;

  show_name = g_settings_get_boolean (p->indicator_settings,
                                      "show-real-name-on-panel");

  real_name = get_current_real_name (self);
  label = show_name && real_name ? real_name : "";

  if (*label)
    {
      /* Translators: the name of the menu ("System"), then the user's name */
      a11y = g_strdup_printf (_("System, %s"), label);
    }
  else
    {
      a11y = g_strdup (_("System"));
    }

  /* build the state */
  g_variant_builder_init (&b, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add (&b, "{sv}", "accessible-desc", g_variant_new_string (a11y));
  g_variant_builder_add (&b, "{sv}", "icon", p->default_icon_serialized);
  if (label && *label)
    g_variant_builder_add (&b, "{sv}", "label", g_variant_new_string (label));
  g_variant_builder_add (&b, "{sv}", "visible", g_variant_new_boolean (TRUE));
  state = g_variant_builder_end (&b);

  /* cleanup */
  g_free (a11y);

  return state;
}

static void
update_header_action (IndicatorSessionService * self)
{
  g_simple_action_set_state (self->priv->header_action, action_state_for_header (self));
}

/***
****  USERS
***/

static GMenuModel * create_switch_section (IndicatorSessionService * self, int profile);

static void
add_user (IndicatorSessionService * self, guint uid)
{
  IndicatorSessionUser * u;

  if ((u = indicator_session_users_get_user (self->priv->backend_users, uid)))
    {
      /* update our user table */
      g_hash_table_insert (self->priv->users, GUINT_TO_POINTER(uid), u);

      /* queue rebuilds for the affected sections */
      rebuild_switch_section_soon (self);
      if (u->is_current_user)
        rebuild_header_soon (self);
    }
}

static void
on_user_added (IndicatorSessionUsers * backend_users G_GNUC_UNUSED,
               guint                   uid,
               gpointer                gself)
{
  add_user (INDICATOR_SESSION_SERVICE(gself), uid);
}

static void
on_user_changed (IndicatorSessionUsers * backend_users G_GNUC_UNUSED,
                 guint                   uid,
                 gpointer                gself)
{
  add_user (INDICATOR_SESSION_SERVICE(gself), uid);
}

static void
maybe_add_users (IndicatorSessionService * self)
{
  if (show_user_list (self))
    {
      GList * uids, * l;

      uids = indicator_session_users_get_uids (self->priv->backend_users);
      for (l=uids; l!=NULL; l=l->next)
        add_user (self, GPOINTER_TO_UINT(l->data));
      g_list_free (uids);
    }
}


static void
user_show_menu_changed (IndicatorSessionService * self)
{
  if (show_user_list (self))
      maybe_add_users (self);
  else
      g_hash_table_remove_all (self->priv->users);

  rebuild_switch_section_soon (self);
}

static void
on_user_removed (IndicatorSessionUsers * backend_users G_GNUC_UNUSED,
                 guint                   uid,
                 gpointer                gself)
{
  IndicatorSessionService * self = INDICATOR_SESSION_SERVICE (gself);
  g_return_if_fail (self != NULL);

  /* update our user table */
  g_hash_table_remove (self->priv->users, GUINT_TO_POINTER(uid));

  /* enqueue rebuilds for the affected sections */
  rebuild_switch_section_soon (self);
}

static const char *
get_user_label (const IndicatorSessionUser * user)
{
  const char * c;

  /* if real_name exists and is printable, use it */
  c = user->real_name;
  if ((c != NULL) && g_utf8_validate(c, -1, NULL))
    {
      while (*c != '\0')
        {
          if (g_unichar_isgraph(g_utf8_get_char(c)))
            return user->real_name;

          c = g_utf8_next_char(c);
        }
    }

  /* otherwise, use this as a fallback */
  return user->user_name;
}

static const char *
get_current_real_name (IndicatorSessionService * self)
{
  GHashTableIter iter;
  gpointer key, value;

  /* is it the guest? */
  if (indicator_session_guest_is_active (self->priv->backend_guest))
    return _("Guest");

  /* is it a user? */
  g_hash_table_iter_init (&iter, self->priv->users);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      IndicatorSessionUser * user = value;
      if (user->is_current_user)
        return get_user_label (user);
    }

  return "";
}

/***
****
***/

static GMenuModel *
create_admin_section (void)
{
  GMenu * menu;

  menu = g_menu_new ();
  g_menu_append (menu, _("About This Computer"), "indicator.about");
  g_menu_append (menu, _("Ubuntu Help"), "indicator.help");
  return G_MENU_MODEL (menu);
}

static GMenuModel *
create_settings_section (IndicatorSessionService * self)
{
  GMenu * menu;
  priv_t * p = self->priv;

  menu = g_menu_new ();
  g_menu_append (menu, _("System Settings…"), "indicator.settings");
  if (indicator_session_actions_has_online_account_error (p->backend_actions))
      g_menu_append (menu, _("Online Accounts…"), "indicator.online-accounts");

  return G_MENU_MODEL (menu);
}

/**
 * The switch-to-guest action's state is a dictionary with these entries:
 *   - "is-active" (boolean)
 *   - "is-logged-in" (boolean)
 */
static GVariant *
create_guest_switcher_state (IndicatorSessionService * self)
{
  GVariant * val;
  GVariantBuilder b;
  IndicatorSessionGuest * const g = self->priv->backend_guest;

  g_variant_builder_init (&b, G_VARIANT_TYPE ("a{sv}"));
  val = g_variant_new_boolean (indicator_session_guest_is_active (g));
  g_variant_builder_add (&b, "{sv}", "is-active", val);
  val = g_variant_new_boolean (indicator_session_guest_is_logged_in (g));
  g_variant_builder_add (&b, "{sv}", "is-logged-in", val);
  return g_variant_builder_end (&b);
}

/**
 * The switch-to-user action's state is a dictionary with these entries: 
 *  - "active-user" (username string)
 *  - "logged-in-users" (array of username strings)
 */
static GVariant *
create_user_switcher_state (IndicatorSessionService * self)
{
  GVariantBuilder a;
  GVariantBuilder b;
  GVariant * val;
  GHashTableIter ht_iter;
  gpointer ht_value;
  const char * current_user;

  current_user = "";
  g_variant_builder_init (&a, G_VARIANT_TYPE("as"));
  g_hash_table_iter_init (&ht_iter, self->priv->users);
  while (g_hash_table_iter_next (&ht_iter, NULL, &ht_value))
    {
      const IndicatorSessionUser * u = ht_value;

      if (u->is_current_user)
        current_user = u->user_name;

      if (u->is_logged_in)
        g_variant_builder_add (&a, "s", u->user_name);
    }

  g_variant_builder_init (&b, G_VARIANT_TYPE("a{sv}"));
  val = g_variant_new_string (current_user);
  g_variant_builder_add (&b, "{sv}", "active-user", val);
  val = g_variant_builder_end (&a);
  g_variant_builder_add (&b, "{sv}", "logged-in-users", val);
  return g_variant_builder_end (&b);
}

static void
update_switch_actions (IndicatorSessionService * self)
{
  g_simple_action_set_state (self->priv->guest_switcher_action,
                             create_guest_switcher_state (self));

  g_simple_action_set_state (self->priv->user_switcher_action,
                             create_user_switcher_state (self));
}

static gboolean
use_ellipsis (IndicatorSessionService * self)
{
  /* does the backend support confirmation prompts? */
  if (!indicator_session_actions_can_prompt (self->priv->backend_actions))
    return FALSE;

  /* has the user disabled prompts? */
  if (g_settings_get_boolean (self->priv->indicator_settings,
                              "suppress-logout-restart-shutdown"))
    return FALSE;

  return TRUE;
}

/* lower index == more useful.
   When there are too many users for the menu,
   we use this to decide which to cull. */
static int
compare_users_by_usefulness (gconstpointer ga, gconstpointer gb)
{
  const IndicatorSessionUser * a = *(const IndicatorSessionUser**)ga;
  const IndicatorSessionUser * b = *(const IndicatorSessionUser**)gb;

  if (a->is_current_user != b->is_current_user)
    return a->is_current_user ? -1 : 1;

  if (a->is_logged_in != b->is_logged_in)
    return a->is_logged_in ? -1 : 1;

  if (a->login_frequency != b->login_frequency)
    return a->login_frequency > b->login_frequency ? -1 : 1;

  return 0;
}

/* sorting them for display in the menu */
static int
compare_users_by_label (gconstpointer ga, gconstpointer gb)
{
  int i;
  const IndicatorSessionUser * a = *(const IndicatorSessionUser**)ga;
  const IndicatorSessionUser * b = *(const IndicatorSessionUser**)gb;

  if ((i = g_strcmp0 (get_user_label (a), get_user_label (b))))
    return i;

  return g_strcmp0 (a->user_name, b->user_name);
}

static GVariant *
serialize_icon_file (const gchar * filename)
{
  GVariant * serialized_icon = NULL;

  if (filename != NULL)
    {
      GFile * file = g_file_new_for_path (filename);
      GIcon * icon = g_file_icon_new (file);

      serialized_icon = g_icon_serialize (icon);

      g_object_unref (icon);
      g_object_unref (file);
    }

  return serialized_icon;
}

static void
report_unusable_user (IndicatorSessionService * self, const IndicatorSessionUser * u)
{
  const priv_t * const p = self->priv;
  gpointer key;

  g_return_if_fail(u != NULL);

  key = GUINT_TO_POINTER(u->uid);

  if (!g_hash_table_contains (p->reported_users, key))
  {
    gchar * uid_str;
    GPtrArray * additional;
    const gchar * const error_name = "indicator-session-unknown-user-error";

    /* don't spam apport with duplicates */
    g_hash_table_add (p->reported_users, key);

    uid_str = g_strdup_printf("%u", u->uid);

    additional = g_ptr_array_new (); /* null-terminated key/value pair strs */
    g_ptr_array_add (additional, "uid");
    g_ptr_array_add (additional, uid_str);
    g_ptr_array_add (additional, "icon_file");
    g_ptr_array_add (additional, u->icon_file ? u->icon_file : "(null)");
    g_ptr_array_add (additional, "is_current_user");
    g_ptr_array_add (additional, u->is_current_user ? "true" : "false");
    g_ptr_array_add (additional, "is_logged_in");
    g_ptr_array_add (additional, u->is_logged_in ? "true" : "false");
    g_ptr_array_add (additional, "real_name");
    g_ptr_array_add (additional, u->real_name ? u->real_name : "(null)");
    g_ptr_array_add (additional, "user_name");
    g_ptr_array_add (additional, u->user_name ? u->user_name : "(null)");
    g_ptr_array_add (additional, NULL); /* null termination */
    report_recoverable_problem(error_name, (GPid)0, FALSE, (gchar**)additional->pdata);

    /* cleanup */
    g_free (uid_str);
    g_ptr_array_free (additional, TRUE);
  }
}

static GMenuModel *
create_switch_section (IndicatorSessionService * self, int profile)
{
  GMenu * menu;
  GMenuItem * item;
  gboolean want_accel;
  guint i;
  gpointer guser;
  GHashTableIter iter;
  GPtrArray * users;
  const priv_t * const p = self->priv;
  const gboolean ellipsis = use_ellipsis (self);

  menu = g_menu_new ();

  /* lockswitch */
  if (indicator_session_users_is_live_session (p->backend_users))
    {
      const char * action = "indicator.switch-to-screensaver";
      item = g_menu_item_new (_("Start Screen Saver"), action);
      want_accel = TRUE;
    }
  else if (profile == PROFILE_LOCKSCREEN ||
           indicator_session_guest_is_active (p->backend_guest))
    {
      const char * action = "indicator.switch-to-greeter";
      item = g_menu_item_new (ellipsis ? _("Switch Account…")
                                       : _("Switch Account"), action);
      want_accel = FALSE;
    }
  else
    {
      const char * action = "indicator.switch-to-screensaver";

      if (g_hash_table_size (p->users) == 1)
        item = g_menu_item_new (_("Lock"), action);
      else
        item = g_menu_item_new (ellipsis ? _("Lock/Switch Account…")
                                         : _("Lock/Switch Account"), action);

      want_accel = TRUE;
    }

  if (want_accel)
    {
      gchar * str = g_settings_get_string (p->keybinding_settings, "screensaver");
      g_menu_item_set_attribute (item, "accel", "s", str);
      g_free (str);
    }

  g_menu_append_item (menu, item);
  g_object_unref (item);

  if (indicator_session_guest_is_allowed (p->backend_guest))
    {
      GMenuItem *item;

      item = g_menu_item_new (_("Guest Session"), "indicator.switch-to-guest");
      g_menu_item_set_attribute (item, "x-canonical-type", "s", "indicator.guest-menu-item");
      g_menu_append_item (menu, item);

      g_object_unref (item);
    }

  /* if we need to show the user list, build an array of all the users we know
   * of, otherwise get out now */
  if (!show_user_list (self))
      return G_MENU_MODEL (menu);

  users = g_ptr_array_new ();
  g_hash_table_iter_init (&iter, p->users);
  while (g_hash_table_iter_next (&iter, NULL, &guser))
    g_ptr_array_add (users, guser);

  /* if there are too many users, cull out the less interesting ones */
  if (users->len > p->max_users)
    {
      g_ptr_array_sort (users, compare_users_by_usefulness);
      g_ptr_array_set_size (users, p->max_users);
    }

  /* sort the users by name */
  g_ptr_array_sort (users, compare_users_by_label);

  /* add the users */
  for (i=0; i<users->len; ++i)
    {
      const IndicatorSessionUser * u = g_ptr_array_index (users, i);
      const char * label;
      GVariant * serialized_icon;

      if (profile == PROFILE_LOCKSCREEN && u->is_current_user)
        continue;

      /* Sometimes we get a user without a username? bus hiccup.
         I can't reproduce it, but let's not confuse users with
         a meaningless menuitem. (see bug #1263228) */
      label = get_user_label (u);
      if (!label || !*label)
      {
        report_unusable_user (self, u);
        continue;
      }

      item = g_menu_item_new (label, NULL);
      g_menu_item_set_action_and_target (item, "indicator.switch-to-user", "s", u->user_name);
      g_menu_item_set_attribute (item, "x-canonical-type", "s", "indicator.user-menu-item");

      if ((serialized_icon = serialize_icon_file (u->icon_file)))
        {
          g_menu_item_set_attribute_value (item, G_MENU_ATTRIBUTE_ICON, serialized_icon);
          g_variant_unref (serialized_icon);
        }

      g_menu_append_item (menu, item);
      g_object_unref (item);
    }

  /* cleanup */
  g_ptr_array_free (users, TRUE);
  return G_MENU_MODEL (menu);
}

static GMenuModel *
create_logout_section (IndicatorSessionService * self)
{
  GMenu * menu;
  const priv_t * const p = self->priv;
  const gboolean ellipsis = use_ellipsis (self);

  menu = g_menu_new ();

  if (indicator_session_actions_can_logout (p->backend_actions))
    {
      const char * label = ellipsis ? _("Log Out…") : _("Log Out");
      g_menu_append (menu, label, "indicator.logout");
    }

  return G_MENU_MODEL (menu);
}

static GMenuModel *
create_session_section (IndicatorSessionService * self, int profile)
{
  GMenu * menu;
  const priv_t * const p = self->priv;
  GSettings * const s = p->indicator_settings;
  const gboolean ellipsis = use_ellipsis (self);

  menu = g_menu_new ();

  if (indicator_session_actions_can_suspend (p->backend_actions))
    g_menu_append (menu, _("Suspend"), "indicator.suspend");

  if (indicator_session_actions_can_hibernate (p->backend_actions))
    g_menu_append (menu, _("Hibernate"), "indicator.hibernate");

  if (profile != PROFILE_LOCKSCREEN && 
    indicator_session_actions_can_reboot (p->backend_actions))
    {
      const char * label = ellipsis ? _("Restart…") : _("Restart");
      g_menu_append (menu, label, "indicator.reboot");
    }

  if (profile != PROFILE_LOCKSCREEN && 
    !g_settings_get_boolean (s, "suppress-shutdown-menuitem"))
    {
      const char * label = ellipsis ? _("Shut Down…") : _("Shut Down");
      g_menu_append (menu, label, "indicator.power-off");
    }

  return G_MENU_MODEL (menu);
}

static void
create_menu (IndicatorSessionService * self, int profile)
{
  GMenu * menu;
  GMenu * submenu;
  GMenuItem * header;
  GMenuModel * sections[16];
  int i;
  int n = 0;

  g_assert (0<=profile && profile<N_PROFILES);
  g_assert (self->priv->menus[profile].menu == NULL);

  if (profile == PROFILE_DESKTOP)
    {
      sections[n++] = create_admin_section ();
      sections[n++] = create_settings_section (self);
      sections[n++] = create_switch_section (self, profile);
      sections[n++] = create_logout_section (self);
      sections[n++] = create_session_section (self, profile);
    }
  else if (profile == PROFILE_GREETER)
    {
      sections[n++] = create_session_section (self, profile);
    }
  else if (profile == PROFILE_LOCKSCREEN)
    {
      sections[n++] = create_switch_section (self, profile);
      sections[n++] = create_session_section (self, profile);
    }

  /* add sections to the submenu */
  submenu = g_menu_new ();
  for (i=0; i<n; ++i)
    {
      g_menu_append_section (submenu, NULL, sections[i]);
      g_object_unref (sections[i]);
    }

  /* add submenu to the header */
  header = g_menu_item_new (NULL, "indicator._header");
  g_menu_item_set_attribute (header, "x-canonical-type", "s", "com.canonical.indicator.root");
  g_menu_item_set_submenu (header, G_MENU_MODEL (submenu));
  g_object_unref (submenu);

  /* add header to the menu */
  menu = g_menu_new ();
  g_menu_append_item (menu, header);
  g_object_unref (header);

  self->priv->menus[profile].menu = menu;
  self->priv->menus[profile].submenu = submenu;
}

/***
****  GActions
***/

static IndicatorSessionActions *
get_backend_actions (gpointer gself)
{
  return INDICATOR_SESSION_SERVICE(gself)->priv->backend_actions;
}

static void
on_about_activated (GSimpleAction * a      G_GNUC_UNUSED,
                    GVariant      * param  G_GNUC_UNUSED,
                    gpointer        gself)
{
  indicator_session_actions_about (get_backend_actions(gself));
}

static void
on_online_accounts_activated (GSimpleAction * a      G_GNUC_UNUSED,
                              GVariant      * param  G_GNUC_UNUSED,
                              gpointer        gself)
{
  indicator_session_actions_online_accounts (get_backend_actions(gself));
}

static void
on_help_activated (GSimpleAction  * a      G_GNUC_UNUSED,
                   GVariant       * param  G_GNUC_UNUSED,
                   gpointer         gself)
{
  indicator_session_actions_help (get_backend_actions(gself));
}

static void
on_settings_activated (GSimpleAction * a      G_GNUC_UNUSED,
                       GVariant      * param  G_GNUC_UNUSED,
                       gpointer        gself)
{
  indicator_session_actions_settings (get_backend_actions(gself));
}

static void
on_logout_activated (GSimpleAction * a      G_GNUC_UNUSED,
                     GVariant      * param  G_GNUC_UNUSED,
                     gpointer        gself)
{
  indicator_session_actions_logout (get_backend_actions(gself));
}

static void
on_suspend_activated (GSimpleAction * a      G_GNUC_UNUSED,
                      GVariant      * param  G_GNUC_UNUSED,
                      gpointer        gself)
{
  indicator_session_actions_suspend (get_backend_actions(gself));
}

static void
on_hibernate_activated (GSimpleAction * a      G_GNUC_UNUSED,
                        GVariant      * param  G_GNUC_UNUSED,
                        gpointer        gself)
{
  indicator_session_actions_hibernate (get_backend_actions(gself));
}

static void
on_reboot_activated (GSimpleAction * action G_GNUC_UNUSED,
                     GVariant      * param  G_GNUC_UNUSED,
                     gpointer        gself)
{
  indicator_session_actions_reboot (get_backend_actions(gself));
}

static void
on_power_off_activated (GSimpleAction * a     G_GNUC_UNUSED,
                        GVariant      * param G_GNUC_UNUSED,
                        gpointer        gself)
{
  indicator_session_actions_power_off (get_backend_actions(gself));
}

static void
on_guest_activated (GSimpleAction * a     G_GNUC_UNUSED,
                    GVariant      * param G_GNUC_UNUSED,
                    gpointer        gself)
{
  indicator_session_actions_switch_to_guest (get_backend_actions(gself));
}

static void
on_screensaver_activated (GSimpleAction * a      G_GNUC_UNUSED,
                          GVariant      * param  G_GNUC_UNUSED,
                          gpointer        gself)
{
  indicator_session_actions_switch_to_screensaver (get_backend_actions(gself));
}

static void
on_greeter_activated (GSimpleAction * a      G_GNUC_UNUSED,
                      GVariant      * param  G_GNUC_UNUSED,
                      gpointer        gself)
{
  indicator_session_actions_switch_to_greeter (get_backend_actions(gself));
}

static void
on_user_activated (GSimpleAction * a         G_GNUC_UNUSED,
                   GVariant      * param,
                   gpointer        gself)
{
  const char * username = g_variant_get_string (param, NULL);
  indicator_session_actions_switch_to_username (get_backend_actions(gself),
                                                username);
}

static void
init_gactions (IndicatorSessionService * self)
{
  GVariant * v;
  GSimpleAction * a;
  priv_t * p = self->priv;

  GActionEntry entries[] = {
    { "about",                  on_about_activated           },
    { "help",                   on_help_activated            },
    { "hibernate",              on_hibernate_activated       },
    { "logout",                 on_logout_activated          },
    { "online-accounts",        on_online_accounts_activated },
    { "reboot",                 on_reboot_activated          },
    { "settings",               on_settings_activated        },
    { "switch-to-screensaver",  on_screensaver_activated     },
    { "switch-to-greeter",      on_greeter_activated         },
    { "suspend",                on_suspend_activated         },
    { "power-off",              on_power_off_activated       }
  };

  p->actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP(p->actions),
                                   entries,
                                   G_N_ELEMENTS(entries),
                                   self);

  /* add switch-to-guest action */
  v = create_guest_switcher_state (self);
  a = g_simple_action_new_stateful ("switch-to-guest", NULL, v);
  g_signal_connect (a, "activate", G_CALLBACK(on_guest_activated), self);
  g_action_map_add_action (G_ACTION_MAP (p->actions), G_ACTION(a));
  p->guest_switcher_action = a;

  /* add switch-to-user action... parameter is the uesrname */
  v = create_user_switcher_state (self);
  a = g_simple_action_new_stateful ("switch-to-user", G_VARIANT_TYPE_STRING, v);
  g_signal_connect (a, "activate", G_CALLBACK(on_user_activated), self);
  g_action_map_add_action (G_ACTION_MAP (p->actions), G_ACTION(a));
  p->user_switcher_action = a;

  /* add the header action */
  a = g_simple_action_new_stateful ("_header", NULL,
                                    action_state_for_header (self));
  g_action_map_add_action (G_ACTION_MAP (p->actions), G_ACTION(a));
  p->header_action = a;

  rebuild_now (self, SECTION_HEADER);
}

/***
****
***/

/**
 * A small helper function for rebuild_now().
 * - removes the previous section
 * - adds and unrefs the new section
 */
static void
rebuild_section (GMenu * parent, int pos, GMenuModel * new_section)
{
  g_menu_remove (parent, pos);
  g_menu_insert_section (parent, pos, NULL, new_section);
  g_object_unref (new_section);
}

static void
rebuild_now (IndicatorSessionService * self, int sections)
{
  priv_t * p = self->priv;
  struct ProfileMenuInfo * desktop = &p->menus[PROFILE_DESKTOP];
  struct ProfileMenuInfo * greeter = &p->menus[PROFILE_GREETER];
  struct ProfileMenuInfo * lockscreen = &p->menus[PROFILE_LOCKSCREEN];

  if (sections & SECTION_HEADER)
    {
      update_header_action (self);
    }

  if (sections & SECTION_ADMIN)
    {
      rebuild_section (desktop->submenu, 0, create_admin_section());
    }

  if (sections & SECTION_SETTINGS)
    {
      rebuild_section (desktop->submenu, 1, create_settings_section(self));
    }

  if (sections & SECTION_SWITCH)
    {
      rebuild_section (desktop->submenu, 2, create_switch_section(self, PROFILE_DESKTOP));
      rebuild_section (lockscreen->submenu, 0, create_switch_section(self, PROFILE_LOCKSCREEN));
      update_switch_actions (self);
    }

  if (sections & SECTION_LOGOUT)
    {
      rebuild_section (desktop->submenu, 3, create_logout_section(self));
    }

  if (sections & SECTION_SESSION)
    {
      rebuild_section (desktop->submenu, 4, create_session_section(self, PROFILE_DESKTOP));
      rebuild_section (greeter->submenu, 0, create_session_section(self, PROFILE_GREETER));
      rebuild_section (lockscreen->submenu, 1, create_session_section(self, PROFILE_LOCKSCREEN));
    }
}

static int
rebuild_timeout_func (IndicatorSessionService * self)
{
  priv_t * p = self->priv;
  rebuild_now (self, p->rebuild_flags);
  p->rebuild_flags = 0;
  p->rebuild_id = 0;
  return G_SOURCE_REMOVE;
}

static void
rebuild_soon (IndicatorSessionService * self, int section)
{
  priv_t * p = self->priv;

  p->rebuild_flags |= section;

  if (p->rebuild_id == 0)
    {
      /* Change events seem to come over the bus in small bursts. This msec
         value is an arbitrary number that tries to be large enough to fold
         multiple events into a single rebuild, but small enough that the
         user won't notice any lag. */
      static const int REBUILD_INTERVAL_MSEC = 500;

      p->rebuild_id = g_timeout_add (REBUILD_INTERVAL_MSEC,
                                     (GSourceFunc)rebuild_timeout_func,
                                     self);
    }
}

/***
**** GDBus
***/

static void
on_bus_acquired (GDBusConnection * connection,
                 const gchar     * name,
                 gpointer          gself)
{
  int i;
  guint id;
  GError * err = NULL;
  IndicatorSessionService * self = INDICATOR_SESSION_SERVICE(gself);
  priv_t * p = self->priv;

  g_debug ("bus acquired: %s", name);

  p->conn = g_object_ref (G_OBJECT (connection));

  /* export the actions */
  if ((id = g_dbus_connection_export_action_group (connection,
                                                   BUS_PATH,
                                                   G_ACTION_GROUP (p->actions),
                                                   &err)))
    {
      p->actions_export_id = id;
    }
  else
    {
      g_warning ("cannot export action group: %s", err->message);
      g_clear_error (&err);
    }

  /* export the menus */
  for (i=0; i<N_PROFILES; ++i)
    {
      char * path = g_strdup_printf ("%s/%s", BUS_PATH, menu_names[i]);
      struct ProfileMenuInfo * menu = &p->menus[i];

      if (menu->menu == NULL)
        create_menu (self, i);

      if ((id = g_dbus_connection_export_menu_model (connection,
                                                     path,
                                                     G_MENU_MODEL (menu->menu),
                                                     &err)))
        {
          menu->export_id = id;
        }
      else
        {
          g_warning ("cannot export %s menu: %s", menu_names[i], err->message);
          g_clear_error (&err);
        }

      g_free (path);
    }
}

static void
unexport (IndicatorSessionService * self)
{
  int i;
  priv_t * p = self->priv;

  /* unexport the menus */
  for (i=0; i<N_PROFILES; ++i)
    {
      guint * id = &self->priv->menus[i].export_id;

      if (*id)
        {
          g_dbus_connection_unexport_menu_model (p->conn, *id);
          *id = 0;
        }
    }

  /* unexport the actions */
  if (p->actions_export_id)
    {
      g_dbus_connection_unexport_action_group (p->conn, p->actions_export_id);
      p->actions_export_id = 0;
    }
}

static void
on_name_lost (GDBusConnection * connection G_GNUC_UNUSED,
              const gchar     * name,
              gpointer          gself)
{
  IndicatorSessionService * self = INDICATOR_SESSION_SERVICE (gself);

  g_debug ("%s %s name lost %s", G_STRLOC, G_STRFUNC, name);

  unexport (self);

  g_signal_emit (self, signals[NAME_LOST], 0, NULL);
}

/***
****
***/

static void
/* cppcheck-suppress unusedFunction */
indicator_session_service_init (IndicatorSessionService * self)
{
  priv_t * p;
  gpointer gp;
  GIcon * icon;

  /* init our priv pointer */
  p = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   INDICATOR_TYPE_SESSION_SERVICE,
                                   IndicatorSessionServicePrivate);
  p->indicator_settings = g_settings_new ("com.canonical.indicator.session");
  p->keybinding_settings = g_settings_new ("org.gnome.settings-daemon.plugins.media-keys");
  self->priv = p;

  /* init the backend objects */
  p->cancellable = g_cancellable_new ();
  backend_get (p->cancellable, &p->backend_actions,
                               &p->backend_users,
                               &p->backend_guest);

  icon = g_themed_icon_new_with_default_fallbacks (ICON_DEFAULT);
  p->default_icon_serialized = g_icon_serialize (icon);
  g_object_unref (icon);

  /* init our key-to-User table */
  p->users = g_hash_table_new_full (g_direct_hash,
                                    g_direct_equal,
                                    NULL,
                                    (GDestroyNotify)indicator_session_user_free);

  p->reported_users = g_hash_table_new (g_direct_hash, g_direct_equal);

  maybe_add_users (self);

  init_gactions (self);

  /* watch for changes in backend_users */
  gp = p->backend_users;
  g_signal_connect (gp, INDICATOR_SESSION_USERS_SIGNAL_USER_ADDED,
                    G_CALLBACK(on_user_added), self);
  g_signal_connect (gp, INDICATOR_SESSION_USERS_SIGNAL_USER_CHANGED,
                    G_CALLBACK(on_user_changed), self);
  g_signal_connect (gp, INDICATOR_SESSION_USERS_SIGNAL_USER_REMOVED,
                    G_CALLBACK(on_user_removed), self);
  g_signal_connect_swapped (gp, "notify::is-live-session",
                            G_CALLBACK(rebuild_switch_section_soon), self);

  /* watch for changes in backend_guest */
  gp = p->backend_guest;
  g_signal_connect_swapped (gp, "notify::guest-is-active-session",
                            G_CALLBACK(rebuild_header_soon), self);
  g_signal_connect_swapped (gp, "notify",
                            G_CALLBACK(rebuild_switch_section_soon), self);

  /* watch for updates in backend_actions */
  gp = p->backend_actions;
  g_signal_connect_swapped (gp, "notify",
                            G_CALLBACK(rebuild_switch_section_soon), self);
  g_signal_connect_swapped (gp, "notify",
                            G_CALLBACK(rebuild_logout_section_soon), self);
  g_signal_connect_swapped (gp, "notify",
                            G_CALLBACK(rebuild_session_section_soon), self);
  g_signal_connect_swapped (gp, "notify::has-online-account-error",
                            G_CALLBACK(rebuild_header_soon), self);
  g_signal_connect_swapped (gp, "notify::has-online-account-error",
                            G_CALLBACK(rebuild_settings_section_soon), self);

  /* watch for changes in the indicator's settings */
  gp = p->indicator_settings;
  g_signal_connect_swapped (gp, "changed::suppress-logout-restart-shutdown",
                            G_CALLBACK(rebuild_switch_section_soon), self);
  g_signal_connect_swapped (gp, "changed::suppress-logout-restart-shutdown",
                            G_CALLBACK(rebuild_logout_section_soon), self);
  g_signal_connect_swapped (gp, "changed::suppress-logout-restart-shutdown",
                            G_CALLBACK(rebuild_session_section_soon), self);
  g_signal_connect_swapped (gp, "changed::suppress-shutdown-menuitem",
                            G_CALLBACK(rebuild_session_section_soon), self);
  g_signal_connect_swapped (gp, "changed::show-real-name-on-panel",
                            G_CALLBACK(rebuild_header_soon), self);
  g_signal_connect_swapped (gp, "changed::user-show-menu",
                            G_CALLBACK(user_show_menu_changed), self);

  /* watch for changes to the lock keybinding */
  gp = p->keybinding_settings;
  g_signal_connect_swapped (gp, "changed::screensaver",
                            G_CALLBACK(rebuild_switch_section_soon), self);

  self->priv->own_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                       BUS_NAME,
                                       G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
                                       on_bus_acquired,
                                       NULL,
                                       on_name_lost,
                                       self,
                                       NULL);
}

/***
****  GObject plumbing: properties
***/

static void
my_get_property (GObject     * o,
                  guint         property_id,
                  GValue      * value,
                  GParamSpec  * pspec)
{
  IndicatorSessionService * self = INDICATOR_SESSION_SERVICE (o);
 
  switch (property_id)
    {
      case PROP_MAX_USERS:
        g_value_set_uint (value, self->priv->max_users);
        break;
 
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
    }
}

static void
my_set_property (GObject       * o,
                 guint           property_id,
                 const GValue  * value,
                 GParamSpec    * pspec)
{
  IndicatorSessionService * self = INDICATOR_SESSION_SERVICE (o);

  switch (property_id)
    {
      case PROP_MAX_USERS:
        self->priv->max_users = g_value_get_uint (value);
        rebuild_switch_section_soon (self);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
    }
}

/***
****  GObject plumbing: life cycle
***/

static void
my_dispose (GObject * o)
{
  int i;
  IndicatorSessionService * self = INDICATOR_SESSION_SERVICE(o);
  priv_t * p = self->priv;

  if (p->own_id)
    {
      g_bus_unown_name (p->own_id);
      p->own_id = 0;
    }

  unexport (self);

  if (p->cancellable != NULL)
    {
      g_cancellable_cancel (p->cancellable);
      g_clear_object (&p->cancellable);
    }

  if (p->rebuild_id)
    {
      g_source_remove (p->rebuild_id);
      p->rebuild_id = 0;
    }

  g_clear_pointer (&p->users, g_hash_table_destroy);
  g_clear_pointer (&p->reported_users, g_hash_table_destroy);
  g_clear_object (&p->backend_users);
  g_clear_object (&p->backend_guest);
  g_clear_object (&p->backend_actions);
  g_clear_object (&p->indicator_settings);
  g_clear_object (&p->keybinding_settings);
  g_clear_object (&p->actions);

  for (i=0; i<N_PROFILES; ++i)
    g_clear_object (&p->menus[i].menu);

  g_clear_object (&p->header_action);
  g_clear_object (&p->user_switcher_action);
  g_clear_object (&p->guest_switcher_action);
  g_clear_object (&p->conn);

  g_clear_pointer (&p->default_icon_serialized, g_variant_unref);

  G_OBJECT_CLASS (indicator_session_service_parent_class)->dispose (o);
}

static void
/* cppcheck-suppress unusedFunction */
indicator_session_service_class_init (IndicatorSessionServiceClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = my_dispose;
  object_class->get_property = my_get_property;
  object_class->set_property = my_set_property;

  g_type_class_add_private (klass, sizeof (IndicatorSessionServicePrivate));

  signals[NAME_LOST] = g_signal_new (INDICATOR_SESSION_SERVICE_SIGNAL_NAME_LOST,
                                     G_TYPE_FROM_CLASS(klass),
                                     G_SIGNAL_RUN_LAST,
                                     G_STRUCT_OFFSET (IndicatorSessionServiceClass, name_lost),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);

  properties[PROP_0] = NULL;

  properties[PROP_MAX_USERS] = g_param_spec_uint ("max-users",
                                                  "Max Users",
                                                  "Max visible users",
                                                  0, INT_MAX, 12,
                                                  G_PARAM_READWRITE |
                                                  G_PARAM_CONSTRUCT |
                                                  G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, properties);
}

IndicatorSessionService *
indicator_session_service_new (void)
{
  GObject * o = g_object_new (INDICATOR_TYPE_SESSION_SERVICE, NULL);

  return INDICATOR_SESSION_SERVICE (o);
}
