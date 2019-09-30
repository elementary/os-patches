/*
 * Copyright © 2004 Noah Levitt
 * Copyright © 2007, 2008 Christian Persch
 * Copyright © 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02110-1301  USA
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "gucharmap-print-operation.h"
#include "gucharmap-search-dialog.h"
#include "gucharmap-settings.h"
#include "gucharmap-window.h"

#define FONT_CHANGE_FACTOR (1.189207115f) /* 2^(0.25) */

/* #define ENABLE_PRINTING */

static void gucharmap_window_class_init (GucharmapWindowClass *klass);
static void gucharmap_window_init       (GucharmapWindow *window);

G_DEFINE_TYPE (GucharmapWindow, gucharmap_window, GTK_TYPE_APPLICATION_WINDOW)

static void
show_error_dialog (GtkWindow *parent,
                   GError *error)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                   "%s", error->message);
  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_window_present (GTK_WINDOW (dialog));
}

#ifdef ENABLE_PRINTING

static void
ensure_print_data (GucharmapWindow *guw)
{
  if (!guw->page_setup) {
    guw->page_setup = gtk_page_setup_new ();
  }

  if (!guw->print_settings) {
    guw->print_settings = gtk_print_settings_new ();
  }
}

static void
print_operation_done_cb (GtkPrintOperation *operation,
                         GtkPrintOperationResult result,
                         GucharmapWindow *guw)
{
  if (result == GTK_PRINT_OPERATION_RESULT_ERROR) {
    GError *error = NULL;

    gtk_print_operation_get_error (operation, &error);
    show_error_dialog (GTK_WINDOW (guw), error);
    g_error_free (error);
  } else if (result == GTK_PRINT_OPERATION_RESULT_APPLY) {
    if (guw->print_settings)
      g_object_unref (guw->print_settings);
    guw->print_settings = g_object_ref (gtk_print_operation_get_print_settings (operation));
  }
}

static void
gucharmap_window_print (GucharmapWindow *guw,
                        GtkPrintOperationAction action)
{
  GtkPrintOperation *op;
  PangoFontDescription *font_desc;
  GucharmapCodepointList *codepoint_list;
  GucharmapChartable *chartable;
  char *chapter, *filename;
  GtkPrintOperationResult rv;
  GError *error = NULL;

  g_object_get (guw->charmap,
                "active-codepoint-list", &codepoint_list,
                "font-desc", &font_desc,
                NULL);

  op = gucharmap_print_operation_new (codepoint_list, font_desc);
  if (codepoint_list)
    g_object_unref (codepoint_list);
  if (font_desc)
    pango_font_description_free (font_desc);

  ensure_print_data (guw);
  if (guw->page_setup)
    gtk_print_operation_set_default_page_setup (op, guw->page_setup);
  if (guw->print_settings)
    gtk_print_operation_set_print_settings (op, guw->print_settings);

  chapter = gucharmap_charmap_get_active_chapter (guw->charmap);
  if (chapter) {
    filename = g_strconcat (chapter, ".pdf", NULL);
    gtk_print_operation_set_export_filename (op, filename);
    g_free (filename);
    g_free (chapter);
  }

  gtk_print_operation_set_allow_async (op, TRUE);
  gtk_print_operation_set_show_progress (op, TRUE);

  g_signal_connect (op, "done",
                    G_CALLBACK (print_operation_done_cb), guw);

  rv = gtk_print_operation_run (op, action, GTK_WINDOW (guw), &error);
  if (rv == GTK_PRINT_OPERATION_RESULT_ERROR) {
    show_error_dialog (GTK_WINDOW (guw), error);
    g_error_free (error);
  }

  g_object_unref (op);
}

#endif /* ENABLE_PRINTING */

static void
status_message (GtkWidget       *widget, 
                const gchar     *message, 
                GucharmapWindow *guw)
{
  gtk_statusbar_pop (GTK_STATUSBAR (guw->status), 0);

  if (message)
    gtk_statusbar_push (GTK_STATUSBAR (guw->status), 0, message);
}

static void
search_start (GucharmapSearchDialog *search_dialog,
              GucharmapWindow       *guw)
{
  GdkCursor *cursor;
  GAction *action;

  cursor = gdk_cursor_new_for_display (gtk_widget_get_display (GTK_WIDGET (guw)), GDK_WATCH);
  gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (guw)), cursor);
  g_object_unref (cursor);

  action = g_action_map_lookup_action (G_ACTION_MAP (guw), "find");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
  action = g_action_map_lookup_action (G_ACTION_MAP (guw), "find-next");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
  action = g_action_map_lookup_action (G_ACTION_MAP (guw), "find-previous");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
}

static void
search_finish (GucharmapSearchDialog *search_dialog,
               gunichar               found_char,
               GucharmapWindow       *guw)
{
  GAction *action;

  if (found_char != (gunichar)(-1))
    gucharmap_charmap_set_active_character (guw->charmap, found_char);
  /* not-found dialog handled by GucharmapSearchDialog */

  gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (guw)), NULL);

  action = g_action_map_lookup_action (G_ACTION_MAP (guw), "find");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
  action = g_action_map_lookup_action (G_ACTION_MAP (guw), "find-next");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
  action = g_action_map_lookup_action (G_ACTION_MAP (guw), "find-previous");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
}

static void
search_find (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       data)
{
  GucharmapWindow *guw = data;

  g_assert (GUCHARMAP_IS_WINDOW (guw));

  if (guw->search_dialog == NULL)
    {
      guw->search_dialog = gucharmap_search_dialog_new (guw);
      g_signal_connect (guw->search_dialog, "search-start", G_CALLBACK (search_start), guw);
      g_signal_connect (guw->search_dialog, "search-finish", G_CALLBACK (search_finish), guw);
    }

  gucharmap_search_dialog_present (GUCHARMAP_SEARCH_DIALOG (guw->search_dialog));
}

static void
search_find_next (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       data)
{
  GucharmapWindow *guw = data;

  if (guw->search_dialog)
    gucharmap_search_dialog_start_search (GUCHARMAP_SEARCH_DIALOG (guw->search_dialog), GUCHARMAP_DIRECTION_FORWARD);
  else
    search_find (action, NULL, guw);
}

static void
search_find_prev (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       data)
{
  GucharmapWindow *guw = data;

  if (guw->search_dialog)
    gucharmap_search_dialog_start_search (GUCHARMAP_SEARCH_DIALOG (guw->search_dialog), GUCHARMAP_DIRECTION_BACKWARD);
  else
    search_find (action, NULL, guw);
}

#ifdef ENABLE_PRINTING

static void
page_setup_done_cb (GtkPageSetup *page_setup,
                    GucharmapWindow *guw)
{
  if (page_setup) {
    g_object_unref (guw->page_setup);
    guw->page_setup = page_setup;
  }
}

static void
file_page_setup (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       data)
{
  GucharmapWindow *guw = data;

  ensure_print_data (guw);

  gtk_print_run_page_setup_dialog_async (GTK_WINDOW (guw),
                                         guw->page_setup,
                                         guw->print_settings,
                                         (GtkPageSetupDoneFunc) page_setup_done_cb,
                                         guw);
}

#if 0
static void
file_print_preview (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       data)
{
  GucharmapWindow *guw = data;

  gucharmap_window_print (guw, GTK_PRINT_OPERATION_ACTION_PREVIEW);
}
#endif

static void
file_print (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       data)
{
  GucharmapWindow *guw = data;

  gucharmap_window_print (guw, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

#endif /* ENABLE_PRINTING */

static void
close_window (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       data)
{
  GtkWidget *widget = data;

  gtk_widget_destroy (widget);
}

static void
font_bigger (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       data)
{
  GucharmapWindow *guw = data;

  gucharmap_mini_font_selection_change_font_size (GUCHARMAP_MINI_FONT_SELECTION (guw->fontsel),
                                                  FONT_CHANGE_FACTOR);
}

static void
font_smaller (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       data)
{
  GucharmapWindow *guw = data;

  gucharmap_mini_font_selection_change_font_size (GUCHARMAP_MINI_FONT_SELECTION (guw->fontsel),
                                                  1.0f / FONT_CHANGE_FACTOR);
}

static void
font_default (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       data)
{
  GucharmapWindow *guw = data;

  gucharmap_mini_font_selection_reset_font_size (GUCHARMAP_MINI_FONT_SELECTION (guw->fontsel));
}

static void
snap_cols_pow2_changed (GSettings  *settings,
                        const char *key,
                        gpointer    data)
{
  GucharmapWindow  *guw = data;

  gucharmap_charmap_set_snap_pow2 (guw->charmap,
                                   g_settings_get_boolean (settings, key));
}

static void
toggle_action_activated (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       data)
{
  GVariant *state = g_action_get_state (G_ACTION (action));
  gboolean value = g_variant_get_boolean (state);

  g_action_change_state (G_ACTION (action), g_variant_new_boolean (!value));
  g_variant_unref (state);
}

static void
change_no_font_fallback (GSimpleAction *action,
                         GVariant      *state,
                         gpointer       data)
{
  GucharmapWindow *guw = data;
  gboolean is_active = g_variant_get_boolean (state);

  gucharmap_charmap_set_font_fallback (guw->charmap, !is_active);
  g_simple_action_set_state (action, state);
/*  gucharmap_settings_set_font_fallback (is_active); */
}

static void
open_url (GtkWindow *parent,
          const char *uri,
          guint32 user_time)
{
  GdkScreen *screen;
  GError *error = NULL;

  if (parent)
    screen = gtk_widget_get_screen (GTK_WIDGET (parent));
  else
    screen = gdk_screen_get_default ();

  if (!gtk_show_uri (screen, uri, user_time, &error)) {
    show_error_dialog (parent, error);
    g_error_free (error);
  }
}

static void
help_contents (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       data)
{
  GucharmapWindow *window = data;
  open_url (GTK_WINDOW (window),
            "help:gucharmap", /* DOC_MODULE */
            gtk_get_current_event_time ());
}

static void
help_about (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       data)
{
  GucharmapWindow *guw = data;
  const gchar *authors[] =
    { 
      "Noah Levitt <nlevitt@columbia.edu>", 
      "Daniel Elstner <daniel.elstner@gmx.net>", 
      "Padraig O'Briain <Padraig.Obriain@sun.com>",
      "Christian Persch <" "chpe" "\100" "gnome" "." "org" ">",
      NULL 
    };

  const gchar *documenters[] =
    {
      "Chee Bin HOH <cbhoh@gnome.org>",
      "Sun Microsystems",
      NULL
    };	  

  const gchar *license[] = {
    N_("Gucharmap is free software; you can redistribute it and/or modify "
       "it under the terms of the GNU General Public License as published by "
       "the Free Software Foundation; either version 3 of the License, or "
       "(at your option) any later version."),
    N_("Permission is hereby granted, free of charge, to any person obtaining "
       "a copy of the Unicode data files to deal in them without restriction, "
       "including without limitation the rights to use, copy, modify, merge, "
       "publish, distribute, and/or sell copies."),
    N_("Gucharmap and the Unicode data files are distributed in the hope that "
       "they will be useful, but WITHOUT ANY WARRANTY; without even the implied "
       "warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See "
       "the GNU General Public License and Unicode Copyright for more details."),
    N_("You should have received a copy of the GNU General Public License "
       "along with Gucharmap; if not, write to the Free Software Foundation, Inc., "
       "59 Temple Place, Suite 330, Boston, MA  02110-1301  USA"),
    N_("Also you should have received a copy of the Unicode Copyright along "
       "with Gucharmap; you can always find it at Unicode's website: "
       "http://www.unicode.org/copyright.html")
  };
  gchar *license_trans;
  license_trans = g_strconcat (_(license[0]), "\n\n", _(license[1]), "\n\n",
			       _(license[2]), "\n\n", _(license[3]), "\n\n",
			       _(license[4]), "\n\n", NULL);

  gtk_show_about_dialog (GTK_WINDOW (guw),
			 "program-name", _("GNOME Character Map"),
			 "version", VERSION,
			 "comments", _("Based on the Unicode Character Database 6.3.0"),
			 "copyright", "Copyright © 2004 Noah Levitt\n"
				      "Copyright © 1991–2013 Unicode, Inc.\n"
				      "Copyright © 2007–2012 Christian Persch",
			 "documenters", documenters,
			 "license", license_trans,
			 "wrap-license", TRUE,
			 "logo-icon-name", GUCHARMAP_ICON_NAME,
  			 "authors", authors,
			 "translator-credits", _("translator-credits"),
			 "website", "http://live.gnome.org/Gucharmap",
			 NULL);

  g_free (license_trans);
}

static void
next_or_prev_character (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer    data)
{
  GucharmapWindow *guw = data;
  GucharmapChartable *chartable;
  GucharmapChartableClass *klass;
  const char *name;
  guint keyval = 0;

  name = g_action_get_name (G_ACTION (action));
  if (strcmp (name, "next-character") == 0) {
    keyval = GDK_KEY_Right;
  } else if (strcmp (name, "previous-character") == 0) {
    keyval = GDK_KEY_Left;
  } else {
    g_assert_not_reached ();
  }

  chartable = gucharmap_charmap_get_chartable (guw->charmap);
  klass = GUCHARMAP_CHARTABLE_GET_CLASS (chartable);
  gtk_binding_set_activate (gtk_binding_set_by_class (klass),
                            keyval,
                            0,
                            G_OBJECT (chartable));
}

static void
next_chapter (GSimpleAction *action,
              GVariant      *parameter,
              gpointer    data)
{
  GucharmapWindow *guw = data;
  gucharmap_charmap_next_chapter (guw->charmap);
}

static void
prev_chapter (GSimpleAction *action,
              GVariant      *parameter,
              gpointer    data)
{
  GucharmapWindow *guw = data;
  gucharmap_charmap_previous_chapter (guw->charmap);
}

static void
chapters_set_labels (const gchar     *labelnext,
		     const gchar     *labelprev,
		     GucharmapWindow *guw)
{
  GtkApplication *app;
  GMenuModel *model;
  int n_items;

  app = GTK_APPLICATION (g_application_get_default ());
  g_return_if_fail (app != NULL);
  model = G_MENU_MODEL (g_object_get_data (G_OBJECT (app), "go-chapter-menu"));

  n_items = g_menu_model_get_n_items (model);
  while (n_items--)
    g_menu_remove (G_MENU (model), 0);

  g_menu_append (G_MENU (model), labelnext, "win.next-chapter");
  g_menu_append (G_MENU (model), labelprev, "win.previous-chapter");
}

enum {
  VIEW_BY_SCRIPT,
  VIEW_BY_BLOCK
};

static void
gucharmap_window_set_chapters_model (GucharmapWindow *guw,
                                     GucharmapChaptersMode mode)
{
  GucharmapChaptersModel *model = NULL;

  switch (mode)
    {
      case GUCHARMAP_CHAPTERS_SCRIPT:
      	model = gucharmap_script_chapters_model_new ();
	chapters_set_labels (_("Next Script"), _("Previous Script"), guw);
	break;
      
      case GUCHARMAP_CHAPTERS_BLOCK:
      	model = gucharmap_block_chapters_model_new ();
	chapters_set_labels (_("Next Block"), _("Previous Block"), guw);
	break;
      
      default:
        g_assert_not_reached ();
    }

  gucharmap_charmap_set_chapters_model (guw->charmap, model);
  g_object_unref (model);
}

static void
gucharmap_window_group_by_changed (GSettings   *settings,
                                   const gchar *key,
                                   gpointer     user_data)
{
  GucharmapWindow *guw = user_data;

  gucharmap_window_set_chapters_model (guw, g_settings_get_enum (settings, "group-by"));
}

#ifdef DEBUG_chpe
static void
move_to_next_screen_cb (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       data)
{
  GtkWidget *widget = data;
  GdkScreen *screen;
  GdkDisplay *display;
  int number_of_screens, screen_num;

  screen = gtk_widget_get_screen (widget);
  display = gdk_screen_get_display (screen);
  screen_num = gdk_screen_get_number (screen);
  number_of_screens =  gdk_display_get_n_screens (display);

  if ((screen_num + 1) < number_of_screens) {
    screen = gdk_display_get_screen (display, screen_num + 1);
  } else {
    screen = gdk_display_get_screen (display, 0);
  }

  gtk_window_set_screen (GTK_WINDOW (widget), screen);
}
#endif

static void
insert_character_in_text_to_copy (GucharmapChartable *chartable,
                                  GucharmapWindow *guw)
{
  gchar ubuf[7];
  gint pos = -1;
  gunichar wc;

  wc = gucharmap_chartable_get_active_character (chartable);
  /* Can't copy values that are not valid unicode characters */
  if (!gucharmap_unichar_validate (wc))
    return;

  ubuf[g_unichar_to_utf8 (wc, ubuf)] = '\0';
  gtk_editable_delete_selection (GTK_EDITABLE (guw->text_to_copy_entry));
  pos = gtk_editable_get_position (GTK_EDITABLE (guw->text_to_copy_entry));
  gtk_editable_insert_text (GTK_EDITABLE (guw->text_to_copy_entry), ubuf, -1, &pos);
  gtk_editable_set_position (GTK_EDITABLE (guw->text_to_copy_entry), pos);
}

static void
edit_copy (GtkWidget *widget, GucharmapWindow *guw)
{
  /* if nothing is selected, select the whole thing */
  if (! gtk_editable_get_selection_bounds (
              GTK_EDITABLE (guw->text_to_copy_entry), NULL, NULL))
    gtk_editable_select_region (GTK_EDITABLE (guw->text_to_copy_entry), 0, -1);

  gtk_editable_copy_clipboard (GTK_EDITABLE (guw->text_to_copy_entry));
}

static void
entry_changed_sensitize_button (GtkEditable *editable, GtkWidget *button)
{
  const gchar *entry_text = gtk_entry_get_text (GTK_ENTRY (editable));
  gtk_widget_set_sensitive (button, entry_text[0] != '\0');
}

static void
status_realize (GtkWidget       *status,
                GucharmapWindow *guw)
{
  GtkAllocation *allocation;
  GtkAllocation widget_allocation;

  gtk_widget_get_allocation (guw->status, &widget_allocation);
  allocation = &widget_allocation;

  /* FIXMEchpe ewww... */
  /* increase the height a bit so it doesn't resize itself */
  gtk_widget_set_size_request (guw->status, -1, allocation->height + 9);
}

static gboolean
save_last_char_idle_cb (GucharmapWindow *guw)
{
  guw->save_last_char_idle_id = 0;

  g_settings_set_uint (guw->settings, "last-char", 
                       gucharmap_charmap_get_active_character (guw->charmap));

  return FALSE;
}

static void
fontsel_sync_font_desc (GucharmapMiniFontSelection *fontsel,
                        GParamSpec *pspec,
                        GucharmapWindow *guw)
{
  PangoFontDescription *font_desc;
  char *font;

  if (guw->in_notification)
    return;

  font_desc = gucharmap_mini_font_selection_get_font_desc (fontsel);

  guw->in_notification = TRUE;
  gucharmap_charmap_set_font_desc (guw->charmap, font_desc);
  guw->in_notification = FALSE;

  font = pango_font_description_to_string (font_desc);
  g_settings_set (guw->settings, "font", "ms", font);
  g_free (font);
}

static void
charmap_sync_font_desc (GucharmapCharmap *charmap,
                        GParamSpec *pspec,
                        GucharmapWindow *guw)
{
  PangoFontDescription *font_desc;

  if (guw->in_notification)
    return;

  font_desc = gucharmap_charmap_get_font_desc (charmap);

  guw->in_notification = TRUE;
  gucharmap_mini_font_selection_set_font_desc (GUCHARMAP_MINI_FONT_SELECTION (guw->fontsel),
                                               font_desc);
  guw->in_notification = FALSE;
}

static void
charmap_sync_active_character (GtkWidget *widget,
                               GParamSpec *pspec,
                               GucharmapWindow *guw)
{
  if (guw->save_last_char_idle_id != 0)
    return;

  guw->save_last_char_idle_id = g_idle_add ((GSourceFunc) save_last_char_idle_cb, guw);
}

static void
gucharmap_window_init (GucharmapWindow *guw)
{
  GtkWidget *grid, *button, *label;
  GucharmapChartable *chartable;
  /* tooltips are NULL because they are never actually shown in the program */
  const GActionEntry menu_entries[] =
  {
#ifdef ENABLE_PRINTING
    { "page-setup", file_page_setup, NULL, NULL, NULL },
#if 0
    { "print-preview", file_print_preview, NULL, NULL, NULL },
#endif
    { "print", file_print, NULL, NULL, NULL },
#endif /* ENABLE_PRINTING */
    { "close", close_window, NULL, NULL, NULL },

    { "zoom-in", font_bigger, NULL, NULL, NULL },
    { "zoom-out", font_smaller, NULL, NULL, NULL },
    { "normal-size", font_default, NULL, NULL, NULL },

    { "find", search_find, NULL, NULL, NULL },
    { "find-next", search_find_next, NULL, NULL, NULL },
    { "find-previous", search_find_prev, NULL, NULL, NULL },

    { "next-character", next_or_prev_character, NULL, NULL, NULL },
    { "previous-character", next_or_prev_character, NULL, NULL, NULL },
    { "next-chapter", next_chapter, NULL, NULL, NULL },
    { "previous-chapter", prev_chapter, NULL, NULL, NULL },

    { "help", help_contents, NULL, NULL, NULL },
    { "about", help_about, NULL, NULL, NULL },

  #ifdef DEBUG_chpe
    { "move-next-screen", move_to_next_screen_cb, NULL, NULL, NULL },
  #endif

    { "show-only-glyphs-in-font", toggle_action_activated, NULL, "false",
      change_no_font_fallback },
  };
  GAction *action;
  gunichar active;
  gchar *font;

  guw->settings = g_settings_new ("org.gnome.Charmap");

  gtk_window_set_title (GTK_WINDOW (guw), _("Character Map"));
  gtk_window_set_icon_name (GTK_WINDOW (guw), GUCHARMAP_ICON_NAME);

  g_action_map_add_action_entries (G_ACTION_MAP (guw),
                                   menu_entries, G_N_ELEMENTS (menu_entries),
                                   guw);

  /* snap-to-power-of-two */
  action = g_settings_create_action (guw->settings, "snap-cols-pow2");
  g_action_map_add_action (G_ACTION_MAP (guw), action);
  g_signal_connect (guw->settings, "changed::snap-cols-pow2",
                    G_CALLBACK (snap_cols_pow2_changed), guw);

  /* Now the widgets */
  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (guw), grid);

  /* The font selector */
  guw->fontsel = gucharmap_mini_font_selection_new ();
  gtk_grid_attach (GTK_GRID (grid), guw->fontsel, 0, 1, 3, 1);
  gtk_widget_show (GTK_WIDGET (guw->fontsel));

  /* The charmap */
  guw->charmap = GUCHARMAP_CHARMAP (gucharmap_charmap_new ());
  g_signal_connect (guw->charmap, "notify::font-desc",
                    G_CALLBACK (charmap_sync_font_desc), guw);

  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (guw->charmap), 0, 2, 3, 1);
  gtk_widget_show (GTK_WIDGET (guw->charmap));

  /* Text to copy entry + button */
  label = gtk_label_new_with_mnemonic (_("_Text to copy:"));
  g_object_set (label, "margin", 6, NULL);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);
  gtk_widget_show (label);

  button = gtk_button_new_from_stock (GTK_STOCK_COPY);
  g_object_set (button, "margin", 6, NULL);
  gtk_widget_set_tooltip_text (button, _("Copy to the clipboard."));
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (edit_copy), guw);
  gtk_grid_attach (GTK_GRID (grid), button, 2, 3, 1, 1);
  gtk_widget_show (button);

  gtk_widget_set_sensitive (button, FALSE);
  guw->text_to_copy_entry = gtk_entry_new ();
  g_object_set (guw->text_to_copy_entry, "margin", 6, NULL);
  gtk_widget_set_hexpand (guw->text_to_copy_entry, TRUE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), guw->text_to_copy_entry);
  g_signal_connect (G_OBJECT (guw->text_to_copy_entry), "changed",
                    G_CALLBACK (entry_changed_sensitize_button), button);

  gtk_grid_attach (GTK_GRID (grid), guw->text_to_copy_entry, 1, 3, 1, 1);
  gtk_widget_show (guw->text_to_copy_entry);

  /* FIXMEchpe!! */
  chartable = gucharmap_charmap_get_chartable (guw->charmap);
  g_signal_connect (chartable, "activate", G_CALLBACK (insert_character_in_text_to_copy), guw);

  /* Finally the statusbar */
  guw->status = gtk_statusbar_new ();
  gtk_grid_attach (GTK_GRID (grid), guw->status, 0, 4, 3, 1);
  gtk_widget_show (guw->status);
  g_signal_connect (guw->status, "realize", G_CALLBACK (status_realize), guw);

  g_signal_connect (guw->charmap, "status-message", G_CALLBACK (status_message), guw);

  gtk_widget_show (grid);

  gtk_widget_grab_focus (GTK_WIDGET (gucharmap_charmap_get_chartable (guw->charmap)));

  gtk_window_set_has_resize_grip (GTK_WINDOW (guw), TRUE);

  /* read initial settings */
  /* font */
  g_settings_get (guw->settings, "font", "ms", &font);
  if (font != NULL)
    {
      gucharmap_window_set_font (guw, font);
      g_free (font);
    }

  /* group by */
  g_action_map_add_action (G_ACTION_MAP (guw), g_settings_create_action (guw->settings, "group-by"));
  g_signal_connect_object (guw->settings, "changed::group-by", G_CALLBACK (gucharmap_window_group_by_changed), guw, 0);
  gucharmap_window_set_chapters_model (guw, g_settings_get_enum (guw->settings, "group-by"));

  /* active character */
  active = g_settings_get_uint (guw->settings, "last-char");
  gucharmap_charmap_set_active_character (guw->charmap, active);

  /* window geometry */
  gucharmap_settings_add_window (GTK_WINDOW (guw));

  /* connect these only after applying the initial settings in order to
   * avoid unnecessary writes to GSettings.
   */
  g_signal_connect (guw->charmap, "notify::active-character",
                    G_CALLBACK (charmap_sync_active_character), guw);
  g_signal_connect (guw->fontsel, "notify::font-desc",
                    G_CALLBACK (fontsel_sync_font_desc), guw);
}

static void
gucharmap_window_finalize (GObject *object)
{
  GucharmapWindow *guw = GUCHARMAP_WINDOW (object);

  if (guw->save_last_char_idle_id != 0)
    g_source_remove (guw->save_last_char_idle_id);

  if (guw->page_setup)
    g_object_unref (guw->page_setup);

  if (guw->print_settings)
    g_object_unref (guw->print_settings);

  G_OBJECT_CLASS (gucharmap_window_parent_class)->finalize (object);
}

static GObject *
gucharmap_window_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_params)
{
  GObject *object;

  object = G_OBJECT_CLASS (gucharmap_window_parent_class)->constructor (type, n_construct_properties, construct_params);
  g_object_bind_property (gtk_settings_get_default (),
                          "gtk-shell-shows-app-menu",
                          object, "show-menubar",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  return object;
}

static void
gucharmap_window_class_init (GucharmapWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = gucharmap_window_constructor;
  object_class->finalize = gucharmap_window_finalize;
}

/* Public API */

GtkWidget *
gucharmap_window_new (GtkApplication *application)
{
  return GTK_WIDGET (g_object_new (gucharmap_window_get_type (),
                     "application", application, NULL));
}

void
gucharmap_window_set_font (GucharmapWindow *guw,
                           const char *font)
{
  PangoFontDescription *font_desc;

  g_return_if_fail (GUCHARMAP_IS_WINDOW (guw));

  g_assert (!gtk_widget_get_realized (GTK_WIDGET (guw)));

  if (!font)
    return;

  font_desc = pango_font_description_from_string (font);
  gucharmap_charmap_set_font_desc (guw->charmap, font_desc);
  pango_font_description_free (font_desc);
}
