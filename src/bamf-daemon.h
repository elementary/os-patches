/*
 * Copyright (C) 2011 Canonical Ltd
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
 * Authored by: Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */

#ifndef __BAMFDAEMON_H__
#define __BAMFDAEMON_H__

#include <glib.h>
#include <glib-object.h>

#define BAMF_TYPE_DAEMON                        (bamf_daemon_get_type ())
#define BAMF_DAEMON(obj)                        (G_TYPE_CHECK_INSTANCE_CAST ((obj), BAMF_TYPE_DAEMON, BamfDaemon))
#define BAMF_IS_DAEMON(obj)                     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BAMF_TYPE_DAEMON))
#define BAMF_DAEMON_CLASS(klass)                (G_TYPE_CHECK_CLASS_CAST ((klass), BAMF_TYPE_DAEMON, BamfDaemonClass))
#define BAMF_IS_DAEMON_CLASS(klass)             (G_TYPE_CHECK_CLASS_TYPE ((klass), BAMF_TYPE_DAEMON))
#define BAMF_DAEMON_GET_CLASS(obj)              (G_TYPE_INSTANCE_GET_CLASS ((obj), BAMF_TYPE_DAEMON, BamfDaemonClass))

typedef struct _BamfDaemon BamfDaemon;
typedef struct _BamfDaemonClass BamfDaemonClass;
typedef struct _BamfDaemonPrivate BamfDaemonPrivate;

struct _BamfDaemonClass
{
  GObjectClass parent;
};

struct _BamfDaemon
{
  GObject parent;

  /* private */
  BamfDaemonPrivate *priv;
};

GType        bamf_daemon_get_type    (void) G_GNUC_CONST;

void         bamf_daemon_start       (BamfDaemon *self);

void         bamf_daemon_stop        (BamfDaemon *self);

gboolean     bamf_daemon_is_running  (BamfDaemon *self);

BamfDaemon * bamf_daemon_get_default (void);

#endif //__BAMFDAEMON_H__
