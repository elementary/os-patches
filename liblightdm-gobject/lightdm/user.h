/*
 * Copyright (C) 2010 Robert Ancell.
 * Author: Robert Ancell <robert.ancell@canonical.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 or version 3 of the License.
 * See http://www.gnu.org/copyleft/lgpl.html the full text of the license.
 */

#ifndef LIGHTDM_USER_H_
#define LIGHTDM_USER_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define LIGHTDM_TYPE_USER_LIST            (lightdm_user_list_get_type())
#define LIGHTDM_USER_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIGHTDM_TYPE_USER_LIST, LightDMUserList))
#define LIGHTDM_USER_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), LIGHTDM_TYPE_USER_LIST, LightDMUserListClass))
#define LIGHTDM_IS_USER_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIGHTDM_TYPE_USER_LIST))
#define LIGHTDM_IS_USER_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LIGHTDM_TYPE_USER_LIST))
#define LIGHTDM_USER_LIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), LIGHTDM_TYPE_USER_LIST, LightDMUserListClass))

typedef struct _LightDMUserList           LightDMUserList;
typedef struct _LightDMUserListClass      LightDMUserListClass;

#define LIGHTDM_TYPE_USER            (lightdm_user_get_type())
#define LIGHTDM_USER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIGHTDM_TYPE_USER, LightDMUser))
#define LIGHTDM_USER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), LIGHTDM_TYPE_USER, LightDMUserClass))
#define LIGHTDM_IS_USER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIGHTDM_TYPE_USER))
#define LIGHTDM_IS_USER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LIGHTDM_TYPE_USER))
#define LIGHTDM_USER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), LIGHTDM_TYPE_USER, LightDMUserClass))

typedef struct _LightDMUser          LightDMUser;
typedef struct _LightDMUserClass     LightDMUserClass;

#define LIGHTDM_USER_LIST_SIGNAL_USER_ADDED   "user-added"
#define LIGHTDM_USER_LIST_SIGNAL_USER_CHANGED "user-changed"
#define LIGHTDM_USER_LIST_SIGNAL_USER_REMOVED "user-removed"

#define LIGHTDM_SIGNAL_USER_CHANGED "changed"

struct _LightDMUser
{
    GObject parent_instance;
};

struct _LightDMUserClass
{
    /*< private >*/
    GObjectClass parent_class;

    void (*changed)(LightDMUser *user);

    /* Reserved */
    void (*reserved1) (void);
    void (*reserved2) (void);
    void (*reserved3) (void);
    void (*reserved4) (void);
    void (*reserved5) (void);
    void (*reserved6) (void);
};

struct _LightDMUserList
{
    GObject parent_instance;
};

struct _LightDMUserListClass
{
    /*< private >*/
    GObjectClass parent_class;

    void (*user_added)(LightDMUserList *user_list, LightDMUser *user);
    void (*user_changed)(LightDMUserList *user_list, LightDMUser *user);
    void (*user_removed)(LightDMUserList *user_list, LightDMUser *user);

    /* Reserved */
    void (*reserved1) (void);
    void (*reserved2) (void);
    void (*reserved3) (void);
    void (*reserved4) (void);
    void (*reserved5) (void);
    void (*reserved6) (void);
};

#ifdef GLIB_VERSION_2_44
typedef LightDMUser *LightDMUser_autoptr;
static inline void glib_autoptr_cleanup_LightDMUser (LightDMUser **_ptr)
{
    glib_autoptr_cleanup_GObject ((GObject **) _ptr);
}
typedef LightDMUserList *LightDMUserList_autoptr;
static inline void glib_autoptr_cleanup_LightDMUserList (LightDMUserList **_ptr)
{
    glib_autoptr_cleanup_GObject ((GObject **) _ptr);
}
#endif

GType lightdm_user_list_get_type (void);

GType lightdm_user_get_type (void);

LightDMUserList *lightdm_user_list_get_instance (void);

gint lightdm_user_list_get_length (LightDMUserList *user_list);

LightDMUser *lightdm_user_list_get_user_by_name (LightDMUserList *user_list, const gchar *username);

GList *lightdm_user_list_get_users (LightDMUserList *user_list);

const gchar *lightdm_user_get_name (LightDMUser *user);

const gchar *lightdm_user_get_real_name (LightDMUser *user);

const gchar *lightdm_user_get_display_name (LightDMUser *user);

const gchar *lightdm_user_get_home_directory (LightDMUser *user);

const gchar *lightdm_user_get_image (LightDMUser *user);

const gchar *lightdm_user_get_background (LightDMUser *user);

const gchar *lightdm_user_get_language (LightDMUser *user);

const gchar *lightdm_user_get_layout (LightDMUser *user);

const gchar * const *lightdm_user_get_layouts (LightDMUser *user);

const gchar *lightdm_user_get_session (LightDMUser *user);

gboolean lightdm_user_get_logged_in (LightDMUser *user);

gboolean lightdm_user_get_has_messages (LightDMUser *user);

uid_t lightdm_user_get_uid (LightDMUser *user);

gboolean lightdm_user_get_is_locked (LightDMUser *user);

G_END_DECLS

#endif /* LIGHTDM_USER_H_ */
