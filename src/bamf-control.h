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


#ifndef __BAMFCONTROL_H__
#define __BAMFCONTROL_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#define BAMF_TYPE_CONTROL                       (bamf_control_get_type ())
#define BAMF_CONTROL(obj)                       (G_TYPE_CHECK_INSTANCE_CAST ((obj), BAMF_TYPE_CONTROL, BamfControl))
#define BAMF_IS_CONTROL(obj)                    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BAMF_TYPE_CONTROL))
#define BAMF_CONTROL_CLASS(klass)               (G_TYPE_CHECK_CLASS_CAST ((klass), BAMF_TYPE_CONTROL, BamfControlClass))
#define BAMF_IS_CONTROL_CLASS(klass)            (G_TYPE_CHECK_CLASS_TYPE ((klass), BAMF_TYPE_CONTROL))
#define BAMF_CONTROL_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS ((obj), BAMF_TYPE_CONTROL, BamfControlClass))

typedef struct _BamfControl BamfControl;
typedef struct _BamfControlClass BamfControlClass;
typedef struct _BamfControlPrivate BamfControlPrivate;

struct _BamfControlClass
{
  BamfDBusControlSkeletonClass parent;
};

struct _BamfControl
{
  BamfDBusControlSkeleton parent;

  /* private */
  BamfControlPrivate *priv;
};

GType         bamf_control_get_type                     (void) G_GNUC_CONST;

void          bamf_control_register_application_for_pid (BamfControl *control,
                                                         const char *application,
                                                         gint32 pid);

void          bamf_control_insert_desktop_file          (BamfControl *control,
                                                         const char *path);

void          bamf_control_create_local_desktop_file 	(BamfControl *control,
                                                       const char *desktop_file);

void          bamf_control_quit                         (BamfControl *control);

BamfControl * bamf_control_get_default                  (void);

#endif
