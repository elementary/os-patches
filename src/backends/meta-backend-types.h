/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013-2018 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_BACKEND_TYPE_H
#define META_BACKEND_TYPE_H

typedef struct _MetaBackend MetaBackend;

typedef struct _MetaMonitorManager MetaMonitorManager;

typedef struct _MetaMonitorConfigManager MetaMonitorConfigManager;
typedef struct _MetaMonitorConfigStore MetaMonitorConfigStore;
typedef struct _MetaMonitorsConfig MetaMonitorsConfig;

typedef enum _MetaMonitorsConfigFlag MetaMonitorsConfigFlag;

typedef struct _MetaMonitor MetaMonitor;
typedef struct _MetaMonitorNormal MetaMonitorNormal;
typedef struct _MetaMonitorTiled MetaMonitorTiled;
typedef struct _MetaMonitorSpec MetaMonitorSpec;
typedef struct _MetaLogicalMonitor MetaLogicalMonitor;

typedef enum _MetaMonitorTransform MetaMonitorTransform;

typedef struct _MetaMonitorMode MetaMonitorMode;

typedef struct _MetaGpu MetaGpu;

typedef struct _MetaCrtc MetaCrtc;
typedef struct _MetaOutput MetaOutput;
typedef struct _MetaCrtcMode MetaCrtcMode;
typedef struct _MetaCrtcAssignment MetaCrtcAssignment;
typedef struct _MetaOutputAssignment MetaOutputAssignment;

typedef struct _MetaTileInfo MetaTileInfo;

typedef struct _MetaRenderer MetaRenderer;
typedef struct _MetaRendererView MetaRendererView;

typedef struct _MetaRemoteDesktop MetaRemoteDesktop;
typedef struct _MetaScreenCast MetaScreenCast;
typedef struct _MetaScreenCastSession MetaScreenCastSession;
typedef struct _MetaScreenCastStream MetaScreenCastStream;

typedef struct _MetaVirtualMonitor MetaVirtualMonitor;
typedef struct _MetaVirtualMonitorInfo MetaVirtualMonitorInfo;
typedef struct _MetaVirtualModeInfo MetaVirtualModeInfo;

typedef struct _MetaIdleManager MetaIdleManager;

#ifdef HAVE_REMOTE_DESKTOP
typedef struct _MetaRemoteDesktop MetaRemoteDesktop;
#endif

#endif /* META_BACKEND_TYPE_H */
