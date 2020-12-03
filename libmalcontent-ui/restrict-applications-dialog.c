/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2020 Endless Mobile, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  - Philip Withnall <withnall@endlessm.com>
 */

#include "config.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "restrict-applications-dialog.h"
#include "restrict-applications-selector.h"


static void update_description (MctRestrictApplicationsDialog *self);

/**
 * MctRestrictApplicationsDialog:
 *
 * The ‘Restrict Applications’ dialog is a dialog which shows the available
 * applications on the system alongside a column of toggle switches, which
 * allows the given user to be prevented from running each application.
 *
 * The dialog contains a single #MctRestrictApplicationsSelector. It takes a
 * #MctRestrictApplicationsDialog:user and
 * #MctRestrictApplicationsDialog:app-filter as input to set up the UI, and
 * returns its output as set of modifications to a given #MctAppFilterBuilder
 * using mct_restrict_applications_dialog_build_app_filter().
 *
 * Since: 0.5.0
 */
struct _MctRestrictApplicationsDialog
{
  GtkDialog parent_instance;

  MctRestrictApplicationsSelector *selector;
  GtkLabel *description;

  MctAppFilter *app_filter;  /* (owned) (not nullable) */
  gchar *user_display_name;  /* (owned) (nullable) */
};

G_DEFINE_TYPE (MctRestrictApplicationsDialog, mct_restrict_applications_dialog, GTK_TYPE_DIALOG)

typedef enum
{
  PROP_APP_FILTER = 1,
  PROP_USER_DISPLAY_NAME,
} MctRestrictApplicationsDialogProperty;

static GParamSpec *properties[PROP_USER_DISPLAY_NAME + 1];

static void
mct_restrict_applications_dialog_constructed (GObject *obj)
{
  MctRestrictApplicationsDialog *self = MCT_RESTRICT_APPLICATIONS_DIALOG (obj);

  g_assert (self->app_filter != NULL);
  g_assert (self->user_display_name == NULL ||
            (*self->user_display_name != '\0' &&
             g_utf8_validate (self->user_display_name, -1, NULL)));

  G_OBJECT_CLASS (mct_restrict_applications_dialog_parent_class)->constructed (obj);
}

static void
mct_restrict_applications_dialog_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
  MctRestrictApplicationsDialog *self = MCT_RESTRICT_APPLICATIONS_DIALOG (object);

  switch ((MctRestrictApplicationsDialogProperty) prop_id)
    {
    case PROP_APP_FILTER:
      g_value_set_boxed (value, self->app_filter);
      break;

    case PROP_USER_DISPLAY_NAME:
      g_value_set_string (value, self->user_display_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mct_restrict_applications_dialog_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  MctRestrictApplicationsDialog *self = MCT_RESTRICT_APPLICATIONS_DIALOG (object);

  switch ((MctRestrictApplicationsDialogProperty) prop_id)
    {
    case PROP_APP_FILTER:
      mct_restrict_applications_dialog_set_app_filter (self, g_value_get_boxed (value));
      break;

    case PROP_USER_DISPLAY_NAME:
      mct_restrict_applications_dialog_set_user_display_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mct_restrict_applications_dialog_dispose (GObject *object)
{
  MctRestrictApplicationsDialog *self = (MctRestrictApplicationsDialog *)object;

  g_clear_pointer (&self->app_filter, mct_app_filter_unref);
  g_clear_pointer (&self->user_display_name, g_free);

  G_OBJECT_CLASS (mct_restrict_applications_dialog_parent_class)->dispose (object);
}

static void
mct_restrict_applications_dialog_class_init (MctRestrictApplicationsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = mct_restrict_applications_dialog_constructed;
  object_class->get_property = mct_restrict_applications_dialog_get_property;
  object_class->set_property = mct_restrict_applications_dialog_set_property;
  object_class->dispose = mct_restrict_applications_dialog_dispose;

  /**
   * MctRestrictApplicationsDialog:app-filter: (not nullable)
   *
   * The user’s current app filter, used to set up the dialog. As app filters
   * are immutable, it is not updated as the dialog is changed. Use
   * mct_restrict_applications_dialog_build_app_filter() to build the new app
   * filter.
   *
   * Since: 0.5.0
   */
  properties[PROP_APP_FILTER] =
      g_param_spec_boxed ("app-filter",
                          "App Filter",
                          "The user’s current app filter, used to set up the dialog.",
                          MCT_TYPE_APP_FILTER,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY);

  /**
   * MctRestrictApplicationsDialog:user-display-name: (nullable)
   *
   * The display name for the currently selected user account, or %NULL if no
   * user is selected. This will typically be the user’s full name (if known)
   * or their username.
   *
   * If set, it must be valid UTF-8 and non-empty.
   *
   * Since: 0.5.0
   */
  properties[PROP_USER_DISPLAY_NAME] =
      g_param_spec_string ("user-display-name",
                           "User Display Name",
                           "The display name for the currently selected user account, or %NULL if no user is selected.",
                           NULL,
                           G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS |
                           G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/freedesktop/MalcontentUi/ui/restrict-applications-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, MctRestrictApplicationsDialog, selector);
  gtk_widget_class_bind_template_child (widget_class, MctRestrictApplicationsDialog, description);
}

static void
mct_restrict_applications_dialog_init (MctRestrictApplicationsDialog *self)
{
  /* Ensure the types used in the UI are registered. */
  g_type_ensure (MCT_TYPE_RESTRICT_APPLICATIONS_SELECTOR);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
update_description (MctRestrictApplicationsDialog *self)
{
  g_autofree gchar *description = NULL;

  if (self->user_display_name == NULL)
    {
      gtk_widget_hide (GTK_WIDGET (self->description));
      return;
    }

  /* Translators: the placeholder is a user’s full name */
  description = g_strdup_printf (_("Restrict %s from using the following installed applications."),
                                 self->user_display_name);
  gtk_label_set_text (self->description, description);
  gtk_widget_show (GTK_WIDGET (self->description));
}

/**
 * mct_restrict_applications_dialog_new:
 * @app_filter: (transfer none): the initial app filter configuration to show
 * @user_display_name: (transfer none) (nullable): the display name of the user
 *    to show the app filter for, or %NULL if no user is selected
 *
 * Create a new #MctRestrictApplicationsDialog widget.
 *
 * Returns: (transfer full): a new restricted applications editing dialog
 * Since: 0.5.0
 */
MctRestrictApplicationsDialog *
mct_restrict_applications_dialog_new (MctAppFilter *app_filter,
                                      const gchar  *user_display_name)
{
  g_return_val_if_fail (app_filter != NULL, NULL);
  g_return_val_if_fail (user_display_name == NULL ||
                        (*user_display_name != '\0' &&
                         g_utf8_validate (user_display_name, -1, NULL)), NULL);

  return g_object_new (MCT_TYPE_RESTRICT_APPLICATIONS_DIALOG,
                       "app-filter", app_filter,
                       "user-display-name", user_display_name,
                       NULL);
}

/**
 * mct_restrict_applications_dialog_get_app_filter:
 * @self: an #MctRestrictApplicationsDialog
 *
 * Get the value of #MctRestrictApplicationsDialog:app-filter. If the property
 * was originally set to %NULL, this will be the empty app filter.
 *
 * Returns: (transfer none) (not nullable): the initial app filter used to
 *    populate the dialog
 * Since: 0.5.0
 */
MctAppFilter *
mct_restrict_applications_dialog_get_app_filter (MctRestrictApplicationsDialog *self)
{
  g_return_val_if_fail (MCT_IS_RESTRICT_APPLICATIONS_DIALOG (self), NULL);

  return self->app_filter;
}

/**
 * mct_restrict_applications_dialog_set_app_filter:
 * @self: an #MctRestrictApplicationsDialog
 * @app_filter: (nullable) (transfer none): the app filter to configure the dialog
 *    from, or %NULL to use an empty app filter
 *
 * Set the value of #MctRestrictApplicationsDialog:app-filter.
 *
 * Since: 0.5.0
 */
void
mct_restrict_applications_dialog_set_app_filter (MctRestrictApplicationsDialog *self,
                                                 MctAppFilter                  *app_filter)
{
  g_autoptr(MctAppFilter) owned_app_filter = NULL;

  g_return_if_fail (MCT_IS_RESTRICT_APPLICATIONS_DIALOG (self));

  /* Default app filter, typically for when we’re instantiated by #GtkBuilder. */
  if (app_filter == NULL)
    {
      g_auto(MctAppFilterBuilder) builder = MCT_APP_FILTER_BUILDER_INIT ();
      owned_app_filter = mct_app_filter_builder_end (&builder);
      app_filter = owned_app_filter;
    }

  if (app_filter == self->app_filter)
    return;

  g_clear_pointer (&self->app_filter, mct_app_filter_unref);
  self->app_filter = mct_app_filter_ref (app_filter);

  mct_restrict_applications_selector_set_app_filter (self->selector, self->app_filter);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_APP_FILTER]);
}

/**
 * mct_restrict_applications_dialog_get_user_display_name:
 * @self: an #MctRestrictApplicationsDialog
 *
 * Get the value of #MctRestrictApplicationsDialog:user-display-name.
 *
 * Returns: (transfer none) (nullable): the display name of the user the dialog
 *    is configured for, or %NULL if unknown
 * Since: 0.5.0
 */
const gchar *
mct_restrict_applications_dialog_get_user_display_name (MctRestrictApplicationsDialog *self)
{
  g_return_val_if_fail (MCT_IS_RESTRICT_APPLICATIONS_DIALOG (self), NULL);

  return self->user_display_name;
}

/**
 * mct_restrict_applications_dialog_set_user_display_name:
 * @self: an #MctRestrictApplicationsDialog
 * @user_display_name: (nullable) (transfer none): the display name of the user
 *    to configure the dialog for, or %NULL if unknown
 *
 * Set the value of #MctRestrictApplicationsDialog:user-display-name.
 *
 * Since: 0.5.0
 */
void
mct_restrict_applications_dialog_set_user_display_name (MctRestrictApplicationsDialog *self,
                                                        const gchar                   *user_display_name)
{
  g_return_if_fail (MCT_IS_RESTRICT_APPLICATIONS_DIALOG (self));
  g_return_if_fail (user_display_name == NULL ||
                    (*user_display_name != '\0' &&
                     g_utf8_validate (user_display_name, -1, NULL)));

  if (g_strcmp0 (self->user_display_name, user_display_name) == 0)
    return;

  g_clear_pointer (&self->user_display_name, g_free);
  self->user_display_name = g_strdup (user_display_name);

  update_description (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USER_DISPLAY_NAME]);
}

/**
 * mct_restrict_applications_dialog_build_app_filter:
 * @self: an #MctRestrictApplicationsDialog
 * @builder: an existing #MctAppFilterBuilder to modify
 *
 * Get the app filter settings currently configured in the dialog, by modifying
 * the given @builder.
 *
 * Typically this will be called in the handler for #GtkDialog::response.
 *
 * Since: 0.5.0
 */
void
mct_restrict_applications_dialog_build_app_filter (MctRestrictApplicationsDialog *self,
                                                   MctAppFilterBuilder           *builder)
{
  g_return_if_fail (MCT_IS_RESTRICT_APPLICATIONS_DIALOG (self));
  g_return_if_fail (builder != NULL);

  mct_restrict_applications_selector_build_app_filter (self->selector, builder);
}
