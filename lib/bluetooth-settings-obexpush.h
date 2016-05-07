/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */

/*
 *  Copyright (C) 2004-2008 Red Hat, Inc.
 *  Copyright (C) 2013 Intel Corporation.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Bastien Nocera <hadess@hadess.net>
 *  Authors: Emilio Pozuelo Monfort <emilio.pozuelo@collabora.co.uk>
 *
 */

#ifndef __OBEX_AGENT_H__
#define __OBEX_AGENT_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ObexAgent {
	GObject parent;
	guint owner_id;
	guint object_reg_id;
	guint obexd_watch_id;
	GDBusConnection *connection;
} ObexAgent;

typedef struct _ObexAgentClass {
	GObjectClass parent;
} ObexAgentClass;

GType obex_agent_get_type (void);

#define OBEX_AGENT_TYPE              (obex_agent_get_type ())
#define OBEX_AGENT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), OBEX_AGENT_TYPE, ObexAgent))
#define OBEX_AGENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), OBEX_AGENT_TYPE, ObexAgentClass))
#define IS_OBEX_AGENT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), OBEX_AGENT_TYPE))
#define IS_OBEX_AGENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), OBEX_AGENT_TYPE))
#define OBEX_AGENT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), OBEX_AGENT_TYPE, ObexAgentClass))

void     obex_agent_up (void);
void     obex_agent_down (void);
char    *lookup_download_dir (void);

G_END_DECLS

#endif
