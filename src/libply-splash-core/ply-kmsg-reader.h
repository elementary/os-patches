/* ply-kmsg-reader.h - kernel log message reader
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 */
#ifndef PLY_KMSG_READER_H
#define PLY_KMSG_READER_H

#include "ply-list.h"
#include "ply-boot-splash.h"
#include <sys/syslog.h>

typedef struct _ply_kmsg_reader ply_kmsg_reader_t;

struct dmesg_name
{
        const char *name;
};

typedef struct
{
        int                priority;
        int                facility;
        unsigned long      sequence;
        unsigned long long timestamp;
        char              *message;
} kmsg_message_t;

struct _ply_kmsg_reader
{
        int             kmsg_fd;
        ply_fd_watch_t *fd_watch;
        ply_trigger_t  *kmsg_trigger;
        ply_list_t     *kmsg_messages;
};

typedef void (* ply_kmsg_reader_message_handler_t) (void *,
                                                    kmsg_message_t *);

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_kmsg_reader_t *ply_kmsg_reader_new (void);
void ply_kmsg_reader_free (ply_kmsg_reader_t *kmsg_reader);
void ply_kmsg_reader_start (ply_kmsg_reader_t *kmsg_reader);
void ply_kmsg_reader_stop (ply_kmsg_reader_t *kmsg_reader);
void ply_kmsg_reader_watch_for_messages (ply_kmsg_reader_t                *kmsg_reader,
                                         ply_kmsg_reader_message_handler_t message_handler,
                                         void                             *user_data);

#endif //PLY_HIDE_FUNCTION_DECLARATIONS

#endif //PLY_KMSG_READER_H
