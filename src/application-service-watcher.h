/*
An object implementing the NotificationWatcher interface and passes
the information into the app-store.

Copyright 2009 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __APPLICATION_SERVICE_WATCHER_H__
#define __APPLICATION_SERVICE_WATCHER_H__

#include <glib.h>
#include <glib-object.h>

#include "application-service-appstore.h"

G_BEGIN_DECLS

#define APPLICATION_SERVICE_WATCHER_TYPE            (application_service_watcher_get_type ())
#define APPLICATION_SERVICE_WATCHER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), APPLICATION_SERVICE_WATCHER_TYPE, ApplicationServiceWatcher))
#define APPLICATION_SERVICE_WATCHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), APPLICATION_SERVICE_WATCHER_TYPE, ApplicationServiceWatcherClass))
#define IS_APPLICATION_SERVICE_WATCHER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), APPLICATION_SERVICE_WATCHER_TYPE))
#define IS_APPLICATION_SERVICE_WATCHER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), APPLICATION_SERVICE_WATCHER_TYPE))
#define APPLICATION_SERVICE_WATCHER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), APPLICATION_SERVICE_WATCHER_TYPE, ApplicationServiceWatcherClass))

typedef struct _ApplicationServiceWatcher      ApplicationServiceWatcher;
typedef struct _ApplicationServiceWatcherClass ApplicationServiceWatcherClass;

struct _ApplicationServiceWatcherClass {
	GObjectClass parent_class;

	/* Signals */
	void (*status_notifier_item_registered) (ApplicationServiceWatcher * watcher, gchar * object, gpointer data);
	void (*status_notifier_item_unregistered) (ApplicationServiceWatcher * watcher, gchar * object, gpointer data);
	void (*status_notifier_host_registered) (ApplicationServiceWatcher * watcher, gpointer data);
};

struct _ApplicationServiceWatcher {
	GObject parent;
};

GType application_service_watcher_get_type (void);
ApplicationServiceWatcher * application_service_watcher_new (ApplicationServiceAppstore * appstore);

G_END_DECLS

#endif
