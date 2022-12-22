/*
 * Copyright (C) 2008-2012 Red Hat, Inc.
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
#ifndef PLY_TEXT_STEP_BAR_H
#define PLY_TEXT_STEP_BAR_H

#include <unistd.h>

#include "ply-event-loop.h"
#include "ply-text-display.h"

typedef struct _ply_text_step_bar ply_text_step_bar_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_text_step_bar_t *ply_text_step_bar_new (void);
void ply_text_step_bar_free (ply_text_step_bar_t *step_bar);

void ply_text_step_bar_draw (ply_text_step_bar_t *step_bar);
void ply_text_step_bar_show (ply_text_step_bar_t *step_bar,
                             ply_text_display_t  *display);
void ply_text_step_bar_hide (ply_text_step_bar_t *step_bar);

void ply_text_step_bar_set_fraction_done (ply_text_step_bar_t *step_bar,
                                          double               fraction_done);

double ply_text_step_bar_get_fraction_done (ply_text_step_bar_t *step_bar);

int ply_text_step_bar_get_number_of_rows (ply_text_step_bar_t *step_bar);
int ply_text_step_bar_get_number_of_columns (ply_text_step_bar_t *step_bar);
#endif

#endif /* PLY_TEXT_PULSER_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
