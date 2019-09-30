/*
 * Copyright (C) 2010-2011 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Jason Smith <jason.smith@canonical.com>
 *              Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */

#ifndef __BAMFAPPLICATION_H__
#define __BAMFAPPLICATION_H__

#include "bamf-view.h"
#include "bamf-window.h"
#include <glib.h>
#include <glib-object.h>

#define BAMF_TYPE_APPLICATION                   (bamf_application_get_type ())
#define BAMF_APPLICATION(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), BAMF_TYPE_APPLICATION, BamfApplication))
#define BAMF_IS_APPLICATION(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BAMF_TYPE_APPLICATION))
#define BAMF_APPLICATION_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), BAMF_TYPE_APPLICATION, BamfApplicationClass))
#define BAMF_IS_APPLICATION_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), BAMF_TYPE_APPLICATION))
#define BAMF_APPLICATION_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), BAMF_TYPE_APPLICATION, BamfApplicationClass))

typedef struct _BamfApplication BamfApplication;
typedef struct _BamfApplicationClass BamfApplicationClass;
typedef struct _BamfApplicationPrivate BamfApplicationPrivate;

struct _BamfApplicationClass
{
  BamfViewClass parent;

  void (*get_application_menu) (BamfApplication *application, gchar **name, gchar **path);
  BamfView* (*get_focusable_child) (BamfApplication *application);
  char ** (*get_supported_mime_types) (BamfApplication *application);
  gboolean (*get_close_when_empty) (BamfApplication *application);
  void (*supported_mimes_changed) (BamfApplication *application, const gchar **mimes);
};

struct _BamfApplication
{
  BamfView parent;

  /* private */
  BamfApplicationPrivate *priv;
};

typedef enum
{
  BAMF_APPLICATION_SYSTEM,  /* BamfWindow container */
  BAMF_APPLICATION_WEB,     /* BamfTab container */
  BAMF_APPLICATION_UNKNOWN,
} BamfApplicationType;

GType             bamf_application_get_type                   (void) G_GNUC_CONST;

void              bamf_application_emit_supported_mime_types_changed     (BamfApplication *application);

const char      * bamf_application_get_desktop_file           (BamfApplication *application);
void              bamf_application_set_desktop_file           (BamfApplication *application,
                                                               const char * desktop_file);

char           ** bamf_application_get_supported_mime_types   (BamfApplication *application);

GVariant        * bamf_application_get_xids                   (BamfApplication *application);

gboolean          bamf_application_manages_xid                (BamfApplication *application,
                                                               guint32 xid);

BamfWindow      * bamf_application_get_window                 (BamfApplication *application,
                                                               guint32 xid);

gboolean          bamf_application_contains_similar_to_window (BamfApplication *app,
                                                               BamfWindow *window);

gboolean          bamf_application_create_local_desktop_file  (BamfApplication *app);

const char      * bamf_application_get_wmclass                (BamfApplication *application);
void              bamf_application_set_wmclass                (BamfApplication *application,
                                                               const char *wmclass);

BamfApplication * bamf_application_new                        (void);

BamfApplication * bamf_application_new_from_desktop_file      (const char * desktop_file);
gboolean          bamf_application_get_show_stubs             (BamfApplication *application);

BamfApplication * bamf_application_new_from_desktop_files     (GList * desktop_files);

BamfApplication * bamf_application_new_with_wmclass           (const char *wmclass);

void              bamf_application_set_application_type       (BamfApplication *application,
                                                               BamfApplicationType type);
BamfApplicationType bamf_application_get_application_type     (BamfApplication *application);

void              bamf_application_get_application_menu       (BamfApplication *application,
                                                               gchar **name, gchar **object_path);

BamfView        * bamf_application_get_focusable_child        (BamfApplication *application);

BamfView        * bamf_application_get_main_child             (BamfApplication *application);

gboolean bamf_application_get_close_when_empty (BamfApplication *application);
gboolean bamf_application_set_desktop_file_from_id (BamfApplication *application, const char *id);



#endif
