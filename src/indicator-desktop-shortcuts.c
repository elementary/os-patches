/*
A small file to parse through the actions that are available
in the desktop file and making those easily usable.

Copyright 2010 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3.0 as published by the Free Software Foundation.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License version 3.0 for more details.

You should have received a copy of the GNU General Public
License along with this library. If not, see
<http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gio/gdesktopappinfo.h>
#include "indicator-desktop-shortcuts.h"

#define ACTIONS_KEY               "Actions"
#define ACTION_GROUP_PREFIX       "Desktop Action"

#define OLD_GROUP_SUFFIX          "Shortcut Group"
#define OLD_SHORTCUTS_KEY         "X-Ayatana-Desktop-Shortcuts"
#define OLD_ENVIRON_KEY           "TargetEnvironment"

#define PROP_DESKTOP_FILE_S   "desktop-file"
#define PROP_IDENTITY_S       "identity"

typedef enum _actions_t actions_t;
enum _actions_t {
	ACTIONS_NONE,
	ACTIONS_XAYATANA,
	ACTIONS_DESKTOP_SPEC
};

typedef struct _IndicatorDesktopShortcutsPrivate IndicatorDesktopShortcutsPrivate;
struct _IndicatorDesktopShortcutsPrivate {
	actions_t actions;
	GKeyFile * keyfile;
	gchar * identity;
	GArray * nicks;
	gchar * domain;
};

enum {
	PROP_0,
	PROP_DESKTOP_FILE,
	PROP_IDENTITY
};

#define INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(o) \
		(G_TYPE_INSTANCE_GET_PRIVATE ((o), INDICATOR_TYPE_DESKTOP_SHORTCUTS, IndicatorDesktopShortcutsPrivate))

static void indicator_desktop_shortcuts_class_init (IndicatorDesktopShortcutsClass *klass);
static void indicator_desktop_shortcuts_init       (IndicatorDesktopShortcuts *self);
static void indicator_desktop_shortcuts_dispose    (GObject *object);
static void indicator_desktop_shortcuts_finalize   (GObject *object);
static void set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void parse_keyfile (IndicatorDesktopShortcuts * ids);
static gboolean should_show (GKeyFile * keyfile, const gchar * group, const gchar * identity, gboolean should_have_target);

G_DEFINE_TYPE (IndicatorDesktopShortcuts, indicator_desktop_shortcuts, G_TYPE_OBJECT);

/* Build up the class */
static void
indicator_desktop_shortcuts_class_init (IndicatorDesktopShortcutsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (IndicatorDesktopShortcutsPrivate));

	object_class->dispose = indicator_desktop_shortcuts_dispose;
	object_class->finalize = indicator_desktop_shortcuts_finalize;

	/* Property funcs */
	object_class->set_property = set_property;
	object_class->get_property = get_property;

	g_object_class_install_property(object_class, PROP_DESKTOP_FILE,
	                                g_param_spec_string(PROP_DESKTOP_FILE_S,
	                                                    "The path of the desktop file to read",
	                                                    "A path to a desktop file that we'll look for shortcuts in.",
	                                                    NULL,
	                                                    G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property(object_class, PROP_IDENTITY,
	                                g_param_spec_string(PROP_IDENTITY_S,
	                                                    "The string that represents the identity that we're acting as.",
	                                                    "Used to process ShowIn and NotShownIn fields of the desktop shortcust to get the proper list.",
	                                                    NULL,
	                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));

	return;
}

/* Initialize instance data */
static void
indicator_desktop_shortcuts_init (IndicatorDesktopShortcuts *self)
{
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(self);

	priv->keyfile = NULL;
	priv->identity = NULL;
	priv->domain = NULL;
	priv->nicks = g_array_new(TRUE, TRUE, sizeof(gchar *));
	priv->actions = ACTIONS_NONE;

	return;
}

/* Clear object references */
static void
indicator_desktop_shortcuts_dispose (GObject *object)
{
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(object);

	if (priv->keyfile) {
		g_key_file_free(priv->keyfile);
		priv->keyfile = NULL;
	}

	G_OBJECT_CLASS (indicator_desktop_shortcuts_parent_class)->dispose (object);
	return;
}

/* Free all memory */
static void
indicator_desktop_shortcuts_finalize (GObject *object)
{
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(object);

	if (priv->identity != NULL) {
		g_free(priv->identity);
		priv->identity = NULL;
	}

	if (priv->domain != NULL) {
		g_free(priv->domain);
		priv->domain = NULL;
	}

	if (priv->nicks != NULL) {
		gint i;
		for (i = 0; i < priv->nicks->len; i++) {
			gchar * nick = g_array_index(priv->nicks, gchar *, i);
			g_free(nick);
		}
		g_array_free(priv->nicks, TRUE);
		priv->nicks = NULL;
	}

	G_OBJECT_CLASS (indicator_desktop_shortcuts_parent_class)->finalize (object);
	return;
}

/* Sets one of the two properties we have, only at construction though */
static void
set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
	g_return_if_fail(INDICATOR_IS_DESKTOP_SHORTCUTS(object));
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(object);

	switch(prop_id) {
	case PROP_DESKTOP_FILE: {
		if (priv->keyfile != NULL) {
			g_key_file_free(priv->keyfile);
			priv->keyfile = NULL;
			priv->actions = ACTIONS_NONE;
		}

		GError * error = NULL;
		GKeyFile * keyfile = g_key_file_new();
		g_key_file_load_from_file(keyfile, g_value_get_string(value), G_KEY_FILE_NONE, &error);

		if (error != NULL) {
			g_warning("Unable to load keyfile from file '%s': %s", g_value_get_string(value), error->message);
			g_error_free(error);
			g_key_file_free(keyfile);
			break;
		}

		/* Always prefer the desktop spec if we can get it */
		if (priv->actions == ACTIONS_NONE && g_key_file_has_key(keyfile, G_KEY_FILE_DESKTOP_GROUP, ACTIONS_KEY, NULL)) {
			priv->actions = ACTIONS_DESKTOP_SPEC;
		}

		/* But fallback if we can't */
		if (priv->actions == ACTIONS_NONE && g_key_file_has_key(keyfile, G_KEY_FILE_DESKTOP_GROUP, OLD_SHORTCUTS_KEY, NULL)) {
			priv->actions = ACTIONS_XAYATANA;
			g_warning("Desktop file '%s' is using a deprecated format for its actions that will be dropped soon.", g_value_get_string(value));
		}

		if (priv->actions == ACTIONS_NONE) {
			g_key_file_free(keyfile);
			break;
		}

		priv->keyfile = keyfile;
		parse_keyfile(INDICATOR_DESKTOP_SHORTCUTS(object));
		break;
	}
	case PROP_IDENTITY:
		if (priv->identity != NULL) {
			g_warning("Identity already set to '%s' and trying to set it to '%s'.", priv->identity, g_value_get_string(value));
			return;
		}
		priv->identity = g_value_dup_string(value);
		parse_keyfile(INDICATOR_DESKTOP_SHORTCUTS(object));
		break;
	/* *********************** */
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	return;
}

/* Gets either the desktop file our the identity. */
static void
get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
	g_return_if_fail(INDICATOR_IS_DESKTOP_SHORTCUTS(object));
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(object);

	switch(prop_id) {
	case PROP_IDENTITY:
		g_value_set_string(value, priv->identity);
		break;
	/* *********************** */
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	return;
}

/* Checks to see if we can, and if we can it goes through
   and parses the keyfile entries. */
static void
parse_keyfile (IndicatorDesktopShortcuts * ids)
{
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(ids);

	if (priv->keyfile == NULL) {
		return;
	}

	if (priv->identity == NULL) {
		return;
	}

	/* Remove a previous translation domain if we had one
	   from a previously parsed file. */
	if (priv->domain != NULL) {
		g_free(priv->domain);
		priv->domain = NULL;
	}

	/* Check to see if there is a custom translation domain that
	   we should take into account. */
	if (priv->domain == NULL &&
			g_key_file_has_key(priv->keyfile, G_KEY_FILE_DESKTOP_GROUP, "X-GNOME-Gettext-Domain", NULL)) {
		priv->domain = g_key_file_get_string(priv->keyfile, G_KEY_FILE_DESKTOP_GROUP, "X-GNOME-Gettext-Domain", NULL);
	}

	if (priv->domain == NULL &&
			g_key_file_has_key(priv->keyfile, G_KEY_FILE_DESKTOP_GROUP, "X-Ubuntu-Gettext-Domain", NULL)) {
		priv->domain = g_key_file_get_string(priv->keyfile, G_KEY_FILE_DESKTOP_GROUP, "X-Ubuntu-Gettext-Domain", NULL);
	}

	/* We need to figure out what we're looking for and what we want to
	   look for in the rest of the file */
	const gchar * list_name = NULL;
	const gchar * group_format = NULL;
	gboolean should_have_target = FALSE;

	switch (priv->actions) {
	case ACTIONS_NONE:
		/* None, let's just get outta here */
		return;
	case ACTIONS_XAYATANA:
		list_name = OLD_SHORTCUTS_KEY;
		group_format = "%s " OLD_GROUP_SUFFIX;
		should_have_target = TRUE;
		break;
	case ACTIONS_DESKTOP_SPEC:
		list_name = ACTIONS_KEY;
		group_format = ACTION_GROUP_PREFIX " %s";
		should_have_target = FALSE;
		break;
	default:
		g_assert_not_reached();
		return;
	}

	/* Okay, we've got everything we need.  Let's get it on! */
	gint i;
	gsize num_nicks = 0;
	gchar ** nicks = g_key_file_get_string_list(priv->keyfile, G_KEY_FILE_DESKTOP_GROUP, list_name, &num_nicks, NULL);

	/* If there is an error from get_string_list num_nicks should still
	   be zero, so this loop will drop out. */
	for (i = 0; i < num_nicks; i++) {
		/* g_debug("Looking at group nick %s", nicks[i]); */
		gchar * groupname = g_strdup_printf(group_format, nicks[i]);
		if (!g_key_file_has_group(priv->keyfile, groupname)) {
			g_warning("Unable to find group '%s'", groupname);
			g_free(groupname);
			continue;
		}

		if (!should_show(priv->keyfile, G_KEY_FILE_DESKTOP_GROUP, priv->identity, FALSE)) {
			g_free(groupname);
			continue;
		}

		if (!should_show(priv->keyfile, groupname, priv->identity, should_have_target)) {
			g_free(groupname);
			continue;
		}

		gchar * nickalloc = g_strdup(nicks[i]);
		g_array_append_val(priv->nicks, nickalloc);
		g_free(groupname);
	}

	if (nicks != NULL) {
		g_strfreev(nicks);
	}

	return;
}

/* Checks the ONLY_SHOW_IN and NOT_SHOW_IN keys for a group to
   see if we should be showing ourselves. */
static gboolean
should_show (GKeyFile * keyfile, const gchar * group, const gchar * identity, gboolean should_have_target)
{
	if (should_have_target && g_key_file_has_key(keyfile, group, OLD_ENVIRON_KEY, NULL)) {
		/* If we've got this key, we're going to return here and not
		   process the deprecated keys. */
		gint j;
		gsize num_env = 0;
		gchar ** envs = g_key_file_get_string_list(keyfile, group, OLD_ENVIRON_KEY, &num_env, NULL);

		for (j = 0; j < num_env; j++) {
			if (g_strcmp0(envs[j], identity) == 0) {
				break;
			}
		}

		if (envs != NULL) {
			g_strfreev(envs);
		}

		if (j == num_env) {
			return FALSE;
		}
		return TRUE;	
	}

	/* If there is a list of OnlyShowIn entries we need to check
	   to see if we're in that list.  If not, we drop this nick */
	if (g_key_file_has_key(keyfile, group, G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN, NULL)) {
		gint j;
		gsize num_only = 0;
		gchar ** onlies = g_key_file_get_string_list(keyfile, group, G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN, &num_only, NULL);

		for (j = 0; j < num_only; j++) {
			if (g_strcmp0(onlies[j], identity) == 0) {
				break;
			}
		}

		if (onlies != NULL) {
			g_strfreev(onlies);
		}

		if (j == num_only) {
			return FALSE;
		}
	}

	/* If there is a NotShowIn entry we need to make sure that we're
	   not in that list.  If we are, we need to drop out. */
	if (g_key_file_has_key(keyfile, group, G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN, NULL)) {
		gint j;
		gsize num_not = 0;
		gchar ** nots = g_key_file_get_string_list(keyfile, group, G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN, &num_not, NULL);

		for (j = 0; j < num_not; j++) {
			if (g_strcmp0(nots[j], identity) == 0) {
				break;
			}
		}

		if (nots != NULL) {
			g_strfreev(nots);
		}

		if (j != num_not) {
			return FALSE;
		}
	}

	return TRUE;
}

/* Looks through the nicks to see if this one is in the list,
   and thus valid to use. */
static gboolean
is_valid_nick (gchar ** list, const gchar * nick)
{
	if (*list == NULL)
		return FALSE;
	/* g_debug("Checking Nick: %s", list[0]); */
	if (g_strcmp0(list[0], nick) == 0)
		return TRUE;
	return is_valid_nick(&list[1], nick);
}

/* API */

/**
	indicator_desktop_shortcuts_new:
	@file: The desktop file that would be opened to
		find the actions.
	@identity: This is a string that represents the identity
		that should be used in searching those actions.  It 
		relates to the ShowIn and NotShownIn properties.
	
	This function creates the basic object.  It involves opening
	the file and parsing it.  It could potentially block on IO.  At
	the end of the day you'll have a fully functional object.

	Return value: A new #IndicatorDesktopShortcuts object.
*/
IndicatorDesktopShortcuts *
indicator_desktop_shortcuts_new (const gchar * file, const gchar * identity)
{
	GObject * obj = g_object_new(INDICATOR_TYPE_DESKTOP_SHORTCUTS,
	                             PROP_DESKTOP_FILE_S, file,
	                             PROP_IDENTITY_S, identity,
	                             NULL);
	return INDICATOR_DESKTOP_SHORTCUTS(obj);
}

/**
	indicator_desktop_shortcuts_get_nicks:
	@ids: The #IndicatorDesktopShortcuts object to look in

	Give you the list of commands that are available for this desktop
	file given the identity that was passed in at creation.  This will
	filter out the various items in the desktop file.  These nicks can
	then be used as keys for working with the desktop file.

	Return value: A #NULL terminated list of strings.  This memory
		is managed by the @ids object.
*/
const gchar **
indicator_desktop_shortcuts_get_nicks (IndicatorDesktopShortcuts * ids)
{
	g_return_val_if_fail(INDICATOR_IS_DESKTOP_SHORTCUTS(ids), NULL);
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(ids);
	return (const gchar **)priv->nicks->data;
}

/**
	indicator_desktop_shortcuts_nick_get_name:
	@ids: The #IndicatorDesktopShortcuts object to look in
	@nick: Which command that we're referencing.

	This function looks in a desktop file for a nick to find the
	user visible name for that shortcut.  The @nick parameter
	should be gotten from #indicator_desktop_shortcuts_get_nicks
	though it's not required that the exact memory location
	be the same.

	Return value: A user visible string for the shortcut or
		#NULL on error.
*/
gchar *
indicator_desktop_shortcuts_nick_get_name (IndicatorDesktopShortcuts * ids, const gchar * nick)
{
	g_return_val_if_fail(INDICATOR_IS_DESKTOP_SHORTCUTS(ids), NULL);
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(ids);

	g_return_val_if_fail(priv->actions != ACTIONS_NONE, NULL);
	g_return_val_if_fail(priv->keyfile != NULL, NULL);
	g_return_val_if_fail(is_valid_nick((gchar **)priv->nicks->data, nick), NULL);

	const gchar * group_format = NULL;

	switch (priv->actions) {
	case ACTIONS_XAYATANA:
		group_format = "%s " OLD_GROUP_SUFFIX;
		break;
	case ACTIONS_DESKTOP_SPEC:
		group_format = ACTION_GROUP_PREFIX " %s";
		break;
	default:
		g_assert_not_reached();
		return NULL;
	}

	gchar * groupheader = g_strdup_printf(group_format, nick);
	if (!g_key_file_has_group(priv->keyfile, groupheader)) {
		g_warning("The group for nick '%s' doesn't exist anymore.", nick);
		g_free(groupheader);
		return NULL;
	}

	if (!g_key_file_has_key(priv->keyfile, groupheader, G_KEY_FILE_DESKTOP_KEY_NAME, NULL)) {
		g_warning("No name available for nick '%s'", nick);
		g_free(groupheader);
		return NULL;
	}

	gchar * name = NULL;
	gchar * keyvalue = g_key_file_get_string(priv->keyfile,
	                                         groupheader,
	                                         G_KEY_FILE_DESKTOP_KEY_NAME,
	                                         NULL);
	gchar * localeval = g_key_file_get_locale_string(priv->keyfile,
		                                    groupheader,
		                                    G_KEY_FILE_DESKTOP_KEY_NAME,
		                                    NULL,
		                                    NULL);
	g_free(groupheader);

	if (priv->domain != NULL && g_strcmp0(keyvalue, localeval) == 0) {
		name = g_strdup(g_dgettext(priv->domain, keyvalue));
		g_free(localeval);
	} else {
		name = localeval;
	}

	g_free(keyvalue);

	return name;
}

/**
	indicator_desktop_shortcuts_nick_exec_with_context:
	@ids: The #IndicatorDesktopShortcuts object to look in
	@nick: Which command that we're referencing.
	@launch_context: The #GAppLaunchContext to use for launching the shortcut

	Here we take a @nick and try and execute the action that is
	associated with it.  The @nick parameter should be gotten
	from #indicator_desktop_shortcuts_get_nicks though it's not
	required that the exact memory location be the same.

	Return value: #TRUE on success or #FALSE on error.
*/
gboolean
indicator_desktop_shortcuts_nick_exec_with_context (IndicatorDesktopShortcuts * ids, const gchar * nick, GAppLaunchContext * launch_context)
{
	GError * error = NULL;

	g_return_val_if_fail(INDICATOR_IS_DESKTOP_SHORTCUTS(ids), FALSE);
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(ids);

	g_return_val_if_fail(priv->actions != ACTIONS_NONE, FALSE);
	g_return_val_if_fail(priv->keyfile != NULL, FALSE);
	g_return_val_if_fail(is_valid_nick((gchar **)priv->nicks->data, nick), FALSE);

	const gchar * group_format = NULL;

	switch (priv->actions) {
	case ACTIONS_XAYATANA:
		group_format = "%s " OLD_GROUP_SUFFIX;
		break;
	case ACTIONS_DESKTOP_SPEC:
		group_format = ACTION_GROUP_PREFIX " %s";
		break;
	default:
		g_assert_not_reached();
		return FALSE;
	}

	gchar * groupheader = g_strdup_printf(group_format, nick);
	if (!g_key_file_has_group(priv->keyfile, groupheader)) {
		g_warning("The group for nick '%s' doesn't exist anymore.", nick);
		g_free(groupheader);
		return FALSE;
	}

	if (!g_key_file_has_key(priv->keyfile, groupheader, G_KEY_FILE_DESKTOP_KEY_NAME, NULL)) {
		g_warning("No name available for nick '%s'", nick);
		g_free(groupheader);
		return FALSE;
	}

	if (!g_key_file_has_key(priv->keyfile, groupheader, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL)) {
		g_warning("No exec available for nick '%s'", nick);
		g_free(groupheader);
		return FALSE;
	}

	/* Grab the name and the exec entries out of our current group */
	gchar * name = g_key_file_get_locale_string(priv->keyfile,
	                                            groupheader,
	                                            G_KEY_FILE_DESKTOP_KEY_NAME,
	                                            NULL,
	                                            NULL);

	gchar * exec = g_key_file_get_locale_string(priv->keyfile,
	                                            groupheader,
	                                            G_KEY_FILE_DESKTOP_KEY_EXEC,
	                                            NULL,
	                                            NULL);

	g_free(groupheader);

	GAppInfoCreateFlags flags = G_APP_INFO_CREATE_NONE;

	if (launch_context) {
		flags |= G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION;
	}

	GAppInfo * appinfo = g_app_info_create_from_commandline(exec, name, flags, &error);
	g_free(name); g_free(exec);

	if (error != NULL) {
		g_warning("Unable to build Command line App info: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	if (appinfo == NULL) {
		g_warning("Unable to build Command line App info (unknown)");
		return FALSE;
	}

	gboolean launched = g_app_info_launch(appinfo, NULL, launch_context, &error);

	if (error != NULL) {
		g_warning("Unable to launch file from nick '%s': %s", nick, error->message);
		g_clear_error(&error);
	}

	g_object_unref(appinfo);

	return launched;
}

/**
	indicator_desktop_shortcuts_nick_exec:
	@ids: The #IndicatorDesktopShortcuts object to look in
	@nick: Which command that we're referencing.

	Here we take a @nick and try and execute the action that is
	associated with it.  The @nick parameter should be gotten
	from #indicator_desktop_shortcuts_get_nicks though it's not
	required that the exact memory location be the same.
	This function is deprecated and shouldn't be used in newly
	written code.

	Return value: #TRUE on success or #FALSE on error.
*/
gboolean
indicator_desktop_shortcuts_nick_exec (IndicatorDesktopShortcuts * ids, const gchar * nick)
{
	return indicator_desktop_shortcuts_nick_exec_with_context (ids, nick, NULL);
}
