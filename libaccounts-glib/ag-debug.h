/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2009-2010 Nokia Corporation.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@canonical.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef _AG_DEBUG_H_
#define _AG_DEBUG_H_

#include <glib.h>
#include <time.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL
void _ag_debug_init (void);

typedef enum {
    AG_DEBUG_TIME = 1 << 0,
    AG_DEBUG_REFS = 1 << 1,
    AG_DEBUG_LOCKS = 1 << 2,
    AG_DEBUG_QUERIES = 1 << 3,
    AG_DEBUG_INFO = 1 << 4,
    AG_DEBUG_ALL = 0xffffffff
} AgDebugLevel;

G_GNUC_INTERNAL
AgDebugLevel _ag_debug_get_level (void);

#ifdef ENABLE_DEBUG

#define DEBUG(level, format, ...) G_STMT_START {                   \
    if (_ag_debug_get_level() & level)                         \
        g_debug("%s: " format, G_STRFUNC, ##__VA_ARGS__);   \
} G_STMT_END

/* Macros for profiling */
#define TIME_START() \
    struct timespec tm0, tm1; \
    struct timespec tt0, tt1; \
    long ms_mdiff; \
    long ms_tdiff; \
    clock_gettime(CLOCK_MONOTONIC, &tm0); \
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tt0)

#define TIME_STOP() \
    clock_gettime(CLOCK_MONOTONIC, &tm1); \
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tt1); \
    ms_mdiff = (tm1.tv_sec - tm0.tv_sec) * 1000 + \
               (tm1.tv_nsec - tm0.tv_nsec) / 1000000; \
    ms_tdiff = (tt1.tv_sec - tt0.tv_sec) * 1000 + \
               (tt1.tv_nsec - tt0.tv_nsec) / 1000000; \
    DEBUG_TIME("%s, total %ld ms, thread %ld ms", G_STRLOC, ms_mdiff, ms_tdiff)

#else /* !ENABLE_DEBUG */

#define DEBUG(level, format, ...)
#define TIME_START()
#define TIME_STOP()

#endif

#define DEBUG_TIME(format, ...) DEBUG(AG_DEBUG_TIME, format, ##__VA_ARGS__)
#define DEBUG_REFS(format, ...) DEBUG(AG_DEBUG_REFS, format, ##__VA_ARGS__)
#define DEBUG_LOCKS(format, ...) DEBUG(AG_DEBUG_LOCKS, format, ##__VA_ARGS__)
#define DEBUG_QUERIES(format, ...) \
    DEBUG(AG_DEBUG_QUERIES, format, ##__VA_ARGS__)
#define DEBUG_INFO(format, ...) DEBUG(AG_DEBUG_INFO, format, ##__VA_ARGS__)

G_END_DECLS

#endif /* _AG_DEBUG_H_ */
