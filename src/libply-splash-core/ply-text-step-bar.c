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
#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ply-text-display.h"
#include "ply-text-step-bar.h"

struct _ply_text_step_bar
{
        ply_text_display_t *display;

        int                 column;
        int                 row;
        int                 number_of_rows;
        int                 number_of_columns;

        double              percent_done;
        uint32_t            is_hidden : 1;
};

ply_text_step_bar_t *
ply_text_step_bar_new (void)
{
        ply_text_step_bar_t *step_bar;

        step_bar = calloc (1, sizeof(ply_text_step_bar_t));

        step_bar->row = 0;
        step_bar->column = 0;
        step_bar->number_of_columns = 0;
        step_bar->number_of_rows = 0;

        return step_bar;
}

void
ply_text_step_bar_free (ply_text_step_bar_t *step_bar)
{
        if (step_bar == NULL)
                return;

        free (step_bar);
}

void
ply_text_step_bar_draw (ply_text_step_bar_t *step_bar)
{
        int i;
        int cur;

        if (step_bar->is_hidden)
                return;

        ply_text_display_set_background_color (step_bar->display,
                                               PLY_TERMINAL_COLOR_BLACK);

        ply_text_display_set_cursor_position (step_bar->display,
                                              step_bar->column,
                                              step_bar->row);

        cur = step_bar->percent_done * step_bar->number_of_columns;
        for (i = 0; i < step_bar->number_of_columns; i++) {
                if (i == cur)
                        ply_text_display_set_foreground_color (step_bar->display,
                                                               PLY_TERMINAL_COLOR_WHITE);
                else
                        ply_text_display_set_foreground_color (step_bar->display,
                                                               PLY_TERMINAL_COLOR_BROWN);

                /* U+25A0 BLACK SQUARE */
                ply_text_display_write (step_bar->display, "%s", "\xe2\x96\xa0");
                ply_text_display_write (step_bar->display, "%c", ' ');
        }

        ply_text_display_set_foreground_color (step_bar->display,
                                               PLY_TERMINAL_COLOR_DEFAULT);
}

void
ply_text_step_bar_show (ply_text_step_bar_t *step_bar,
                        ply_text_display_t  *display)
{
        int screen_rows;
        int screen_cols;

        assert (step_bar != NULL);

        step_bar->display = display;


        screen_rows = ply_text_display_get_number_of_rows (display);
        screen_cols = ply_text_display_get_number_of_columns (display);

        step_bar->number_of_rows = 1;
        step_bar->row = screen_rows * .66;
        step_bar->number_of_columns = 3;
        step_bar->column = screen_cols / 2.0 - step_bar->number_of_columns / 2.0;

        step_bar->is_hidden = false;

        ply_text_step_bar_draw (step_bar);
}

void
ply_text_step_bar_hide (ply_text_step_bar_t *step_bar)
{
        step_bar->display = NULL;
        step_bar->is_hidden = true;
}

void
ply_text_step_bar_set_percent_done (ply_text_step_bar_t *step_bar,
                                    double               percent_done)
{
        step_bar->percent_done = percent_done;
}

double
ply_text_step_bar_get_percent_done (ply_text_step_bar_t *step_bar)
{
        return step_bar->percent_done;
}

int
ply_text_step_bar_get_number_of_columns (ply_text_step_bar_t *step_bar)
{
        return step_bar->number_of_columns;
}

int
ply_text_step_bar_get_number_of_rows (ply_text_step_bar_t *step_bar)
{
        return step_bar->number_of_rows;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
