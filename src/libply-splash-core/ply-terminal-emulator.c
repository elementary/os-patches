/* ply-terminal-emulator.c - Minimal Terminal Emulator
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

#include "ply-array.h"
#include "ply-terminal-emulator.h"
#include "ply-logger.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <stdio.h>

#define PLY_TERMINAL_EMULATOR_SPACES_PER_TAB 8
#define PLY_TERMINAL_CONTROL_CODE_LETTER_OFFSET 64

/* Characters between 64 to 157 end the escape sequence strings (in testing)
 *  for i in $(seq 1 255)
 *  do
 *          if [[ $i == 72 || $i == 99 || $i == 100 || $i == 101 || $i == 102 || $i == 114 ]]
 *          then
 *                 continue
 *          fi
 *          printf -v CHARHEX "%x" $i
 *          printf -v CHAR '%b' "\U$CHARHEX"
 *          echo -e "$i $CHAR \033[${CHAR}aabc"
 *  done
 * (meaning that $CHAR ends the sequence instead of the first a)
 */
#define PLY_TERMINAL_ESCAPE_CODE_COMMAND_MINIMUM 64
#define PLY_TERMINAL_ESCAPE_CODE_COMMAND_MAXIMUM 157

typedef enum
{
        PLY_TERMINAL_EMULATOR_TERMINAL_STATE_UNESCAPED,
        PLY_TERMINAL_EMULATOR_TERMINAL_STATE_ESCAPED,
        PLY_TERMINAL_EMULATOR_TERMINAL_STATE_CONTROL_SEQUENCE_PARAMETER
} ply_terminal_emulator_terminal_state_t;

typedef enum
{
        PLY_TERMINAL_EMULATOR_UTF8_CHARACTER_PARSE_STATE_SINGLE_BYTE,
        PLY_TERMINAL_EMULATOR_UTF8_CHARACTER_PARSE_STATE_MULTI_BYTE,
} ply_terminal_emulator_utf8_character_parse_state_t;

typedef enum
{
        PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_CHARACTER,
        PLY_TERMINAL_EMULATOR_COMMAND_TYPE_ESCAPE,
        PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE
} ply_terminal_emulator_command_type_t;

typedef enum
{
        PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE,
        PLY_TERMINAL_EMULATOR_BREAK_STRING,
} ply_terminal_emulator_break_string_t;

typedef enum
{
        PLY_TERMINAL_EMULATOR_ERASE_LINE_TYPE_CURSOR_TO_RIGHT,
        PLY_TERMINAL_EMULATOR_ERASE_LINE_TYPE_CURSOR_TO_LEFT,
        PLY_TERMINAL_EMULATOR_ERASE_LINE_TYPE_WHOLE_LINE,
} ply_terminal_emulator_erase_line_type_t;

typedef enum
{
        PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_RESET_CURSOR_COLUMN,
        PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN,
} ply_terminal_emulator_break_string_action_t;

typedef struct
{
        char                                 code;
        ply_terminal_emulator_command_type_t type;
        ply_array_t                         *parameters;
        uint32_t                             parameters_valid : 1;
} ply_terminal_emulator_command_t;

struct _ply_terminal_emulator
{
        ply_terminal_emulator_terminal_state_t             state;

        size_t                                             number_of_rows;
        size_t                                             number_of_columns;

        size_t                                             line_count;
        ply_array_t                                       *lines;

        ply_trigger_t                                     *output_trigger;

        ssize_t                                            cursor_row_offset; /* Relative to the bottom-most allocated line */
        size_t                                             cursor_column;
        ply_terminal_emulator_break_string_action_t        break_action;

        uint32_t                                           last_parameter_was_integer : 1;
        uint32_t                                           pending_parameter_value;
        ply_terminal_emulator_command_t                   *staged_command;
        ply_list_t                                        *pending_commands;

        ply_terminal_emulator_utf8_character_parse_state_t pending_character_state;
        ply_buffer_t                                      *pending_character;
        int                                                pending_character_size;

        ply_rich_text_t                                   *current_line;
        ply_rich_text_character_style_t                    current_style;

        uint32_t                                           show_escape_sequences : 1;
};

typedef ply_terminal_emulator_break_string_t (*ply_terminal_emulator_dispatch_handler_t)();
typedef ply_terminal_emulator_break_string_t (*ply_terminal_emulator_control_sequence_handler_t) (ply_terminal_emulator_t *terminal_emulator,
                                                                                                  const char               code,
                                                                                                  uint32_t                 parameters[],
                                                                                                  size_t                   number_of_parameters,
                                                                                                  bool                     parameters_valid);
typedef ply_terminal_emulator_break_string_t (*ply_terminal_emulator_control_character_handler_t) (ply_terminal_emulator_t *terminal_emulator,
                                                                                                   const char               code);
typedef ply_terminal_emulator_break_string_t (*ply_terminal_emulator_escape_sequence_handler_t) (ply_terminal_emulator_t *terminal_emulator,
                                                                                                 const char               code);

static ply_terminal_emulator_command_t *ply_terminal_emulator_command_new (void);
static void ply_terminal_emulator_command_free (ply_terminal_emulator_command_t *command);

ply_terminal_emulator_t *
ply_terminal_emulator_new (size_t number_of_rows,
                           size_t number_of_columns)
{
        ply_terminal_emulator_t *terminal_emulator;
        ply_rich_text_t *terminal_emulator_line;
        ply_rich_text_span_t span;

        terminal_emulator = calloc (1, sizeof(struct _ply_terminal_emulator));

        terminal_emulator->line_count = 1;
        terminal_emulator->number_of_rows = number_of_rows;
        terminal_emulator->number_of_columns = number_of_columns;
        terminal_emulator->lines = ply_array_new (PLY_ARRAY_ELEMENT_TYPE_POINTER);

        terminal_emulator->pending_character = ply_buffer_new ();
        terminal_emulator->pending_character_state = PLY_TERMINAL_EMULATOR_UTF8_CHARACTER_PARSE_STATE_SINGLE_BYTE;
        terminal_emulator->pending_character_size = 0;

        span.offset = 0;
        span.range = terminal_emulator->number_of_columns;

        for (int i = 0; i < terminal_emulator->number_of_rows; i++) {
                terminal_emulator_line = ply_rich_text_new ();
                ply_rich_text_set_mutable_span (terminal_emulator_line, &span);
                ply_array_add_pointer_element (terminal_emulator->lines, terminal_emulator_line);
        }

        terminal_emulator->cursor_row_offset = 0;

        terminal_emulator->state = PLY_TERMINAL_EMULATOR_TERMINAL_STATE_UNESCAPED;

        terminal_emulator->last_parameter_was_integer = false;
        terminal_emulator->pending_parameter_value = 0;

        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;
        terminal_emulator->output_trigger = ply_trigger_new (NULL);

        terminal_emulator->pending_commands = ply_list_new ();

        ply_rich_text_character_style_initialize (&terminal_emulator->current_style);

        if (ply_kernel_command_line_has_argument ("plymouth.debug-escape-sequences"))
                terminal_emulator->show_escape_sequences = true;

        return terminal_emulator;
}

void
ply_terminal_emulator_free (ply_terminal_emulator_t *terminal_emulator)
{
        ply_rich_text_t **lines;
        ply_list_node_t *node;

        ply_list_foreach (terminal_emulator->pending_commands, node) {
                ply_terminal_emulator_command_t *command;
                command = ply_list_node_get_data (node);
                ply_terminal_emulator_command_free (command);
        }
        ply_list_free (terminal_emulator->pending_commands);

        lines = (ply_rich_text_t **) ply_array_get_pointer_elements (terminal_emulator->lines);
        for (size_t i = 0; lines[i] != NULL; i++) {
                ply_rich_text_drop_reference (lines[i]);
        }

        ply_array_free (terminal_emulator->lines);

        ply_trigger_free (terminal_emulator->output_trigger);

        free (terminal_emulator);
}

static ply_terminal_emulator_command_t *
ply_terminal_emulator_command_new (void)
{
        ply_terminal_emulator_command_t *command_object = calloc (1, sizeof(ply_terminal_emulator_command_t));
        return command_object;
}

static void
ply_terminal_emulator_command_free (ply_terminal_emulator_command_t *command)
{
        free (command);
}

void
fill_offsets_with_padding (ply_terminal_emulator_t *terminal_emulator,
                           size_t                   pad_start,
                           size_t                   pad_stop)
{
        ssize_t bytes_to_pad = pad_stop - pad_start;
        ply_rich_text_character_style_t default_style;

        ply_rich_text_character_style_initialize (&default_style);

        if (pad_start < 0 || bytes_to_pad <= 0)
                return;

        if (pad_stop > pad_start) {
                for (size_t i = pad_start; i <= pad_stop; i++) {
                        ply_rich_text_set_character (terminal_emulator->current_line, default_style, i, " ", 1);
                }
        }
}

/* escape sequence '<ESC>D' */
ply_terminal_emulator_break_string_t
on_escape_sequence_linefeed (ply_terminal_emulator_t *terminal_emulator,
                             const char               code)
{
        ply_trace ("terminal escape equence: line feed");

        assert (code == 'D');

        terminal_emulator->cursor_row_offset++;
        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

        return PLY_TERMINAL_EMULATOR_BREAK_STRING;
}

/* escape sequence '<ESC>E' */
ply_terminal_emulator_break_string_t
on_escape_sequence_newline (ply_terminal_emulator_t *terminal_emulator,
                            const char               code)
{
        ply_trace ("terminal escape equence: new line");

        assert (code == 'E');

        terminal_emulator->cursor_row_offset++;
        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_RESET_CURSOR_COLUMN;

        return PLY_TERMINAL_EMULATOR_BREAK_STRING;
}

/* escape sequence '<ESC>M' */
ply_terminal_emulator_break_string_t
on_escape_sequence_reverse_linefeed (ply_terminal_emulator_t *terminal_emulator,
                                     const char               code)
{
        ply_trace ("terminal escape equence: reverse line feed");

        assert (code == 'M');

        terminal_emulator->cursor_row_offset--;
        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

        return PLY_TERMINAL_EMULATOR_BREAK_STRING;
}

/* Control sequence '@' for '[@' */
ply_terminal_emulator_break_string_t
on_control_sequence_insert_blank_characters (ply_terminal_emulator_t *terminal_emulator,
                                             const char               code,
                                             uint32_t                 parameters[],
                                             size_t                   number_of_parameters,
                                             bool                     paramaters_valid)
{
        int parameter;
        size_t string_length = ply_rich_text_get_length (terminal_emulator->current_line);
        size_t new_string_length;
        size_t append_count;
        size_t string_move_end_offset;
        ply_rich_text_span_t span;
        size_t maximum_characters;
        ply_rich_text_character_style_t default_style;

        ply_trace ("terminal control sequence: insert blank characters");

        assert (code == '@');

        if (paramaters_valid != true)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;

        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

        if (number_of_parameters > 0) {
                parameter = parameters[0];

                /* 0 acts as 1, and this should not be negative */
                if (parameter <= 0)
                        parameter = 1;
        } else {
                parameter = 1;
        }

        ply_rich_text_get_mutable_span (terminal_emulator->current_line, &span);
        maximum_characters = span.offset + span.range;

        new_string_length = string_length + parameter;
        if (new_string_length >= maximum_characters) {
                append_count = maximum_characters - string_length - 1;
                new_string_length = maximum_characters - 1;
        } else {
                append_count = parameter;
        }

        string_move_end_offset = string_length - 1;
        if (string_move_end_offset >= maximum_characters)
                string_move_end_offset = maximum_characters - 1;

        if (new_string_length <= 0)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;

        fill_offsets_with_padding (terminal_emulator, string_length, new_string_length);

        ply_rich_text_character_style_initialize (&default_style);

        for (int i = string_move_end_offset; i >= terminal_emulator->cursor_column; i--) {
                ply_rich_text_move_character (terminal_emulator->current_line,
                                              i,
                                              i + append_count);
                ply_rich_text_set_character (terminal_emulator->current_line, default_style, i, " ", 1);

                if (i <= 0)
                        break;
        }

        return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
}

/* Control sequence 'A' for '[A' */
ply_terminal_emulator_break_string_t
on_control_sequence_move_cursor_up_rows (ply_terminal_emulator_t *terminal_emulator,
                                         const char               code,
                                         uint32_t                 parameters[],
                                         size_t                   number_of_parameters,
                                         bool                     paramaters_valid)
{
        int parameter;

        ply_trace ("terminal control sequence: move cursor up rows");

        assert (code == 'A');

        if (paramaters_valid != true)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;

        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

        if (number_of_parameters > 0) {
                parameter = parameters[0];

                /* 0 acts as 1, and this should not be negative */
                if (parameter <= 0)
                        parameter = 1;
        } else {
                parameter = 1;
        }

        terminal_emulator->cursor_row_offset -= parameter;
        return PLY_TERMINAL_EMULATOR_BREAK_STRING;
}

/* Control sequence 'B' for '[B' */
ply_terminal_emulator_break_string_t
on_control_sequence_move_cursor_down_rows (ply_terminal_emulator_t *terminal_emulator,
                                           const char               code,
                                           uint32_t                 parameters[],
                                           size_t                   number_of_parameters,
                                           bool                     paramaters_valid)
{
        int parameter;

        ply_trace ("terminal control sequence: move cursor down rows");

        assert (code == 'B');

        if (paramaters_valid != true)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;

        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

        if (number_of_parameters > 0) {
                parameter = parameters[0];

                /* 0 acts as 1, and this should not be negative */
                if (parameter <= 0)
                        parameter = 1;
        } else {
                parameter = 1;
        }

        terminal_emulator->cursor_row_offset += parameter;

        return PLY_TERMINAL_EMULATOR_BREAK_STRING;
}

/* Control sequence 'C' for '[C' */
ply_terminal_emulator_break_string_t
on_control_sequence_move_cursor_right (ply_terminal_emulator_t *terminal_emulator,
                                       const char               code,
                                       uint32_t                 parameters[],
                                       size_t                   number_of_parameters,
                                       bool                     paramaters_valid)
{
        int parameter;
        size_t string_length = ply_rich_text_get_length (terminal_emulator->current_line);
        ply_rich_text_span_t span;
        size_t maximum_characters;

        ply_trace ("terminal control sequence: move cursor right");

        assert (code == 'C');

        if (paramaters_valid != true)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;

        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

        if (number_of_parameters > 0) {
                parameter = parameters[0];

                /* 0 acts as 1, and this should not be negative */
                if (parameter <= 0)
                        parameter = 1;
        } else {
                parameter = 1;
        }

        terminal_emulator->cursor_column += parameter;

        ply_rich_text_get_mutable_span (terminal_emulator->current_line, &span);
        maximum_characters = span.offset + span.range;

        if (terminal_emulator->cursor_column >= maximum_characters)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING;

        fill_offsets_with_padding (terminal_emulator, string_length, terminal_emulator->cursor_column);

        return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
}

/* Control sequence 'D' for '[D' */
ply_terminal_emulator_break_string_t
on_control_sequence_move_cursor_left (ply_terminal_emulator_t *terminal_emulator,
                                      const char               code,
                                      uint32_t                 parameters[],
                                      size_t                   number_of_parameters,
                                      bool                     paramaters_valid)
{
        int parameter;
        size_t string_length = ply_rich_text_get_length (terminal_emulator->current_line);

        ply_trace ("terminal control sequence: move cursor left");

        assert (code == 'D');

        if (paramaters_valid != true)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;

        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

        if (number_of_parameters > 0) {
                parameter = parameters[0];

                /* 0 acts as 1, and this should not be negative */
                if (parameter <= 0)
                        parameter = 1;
        } else {
                parameter = 1;
        }

        if (parameter > string_length) {
                terminal_emulator->cursor_column = 0;
        } else {
                terminal_emulator->cursor_column -= parameter;
        }

        fill_offsets_with_padding (terminal_emulator, string_length, terminal_emulator->cursor_column);

        return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
}

/* Control sequence 'E' for '[E' */
ply_terminal_emulator_break_string_t
on_control_sequence_move_cursor_down_rows_to_first_column (ply_terminal_emulator_t *terminal_emulator,
                                                           const char               code,
                                                           uint32_t                 parameters[],
                                                           size_t                   number_of_parameters,
                                                           bool                     paramaters_valid)
{
        int parameter;

        ply_trace ("terminal control sequence: move cursor down rows to first column");

        assert (code == 'E');

        if (paramaters_valid != true)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;

        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_RESET_CURSOR_COLUMN;

        if (number_of_parameters > 0) {
                parameter = parameters[0];

                /* 0 acts as 1, and this should not be negative */
                if (parameter <= 0)
                        parameter = 1;
        } else {
                parameter = 1;
        }

        terminal_emulator->cursor_row_offset += parameter;

        return PLY_TERMINAL_EMULATOR_BREAK_STRING;
}

/* Control sequence 'F' for '[F' */
ply_terminal_emulator_break_string_t
on_control_sequence_move_cursor_up_rows_to_first_column (ply_terminal_emulator_t *terminal_emulator,
                                                         const char               code,
                                                         uint32_t                 parameters[],
                                                         size_t                   number_of_parameters,
                                                         bool                     paramaters_valid)
{
        size_t parameter;

        ply_trace ("terminal control sequence: move cursor up rows to column");

        assert (code == 'F');

        if (paramaters_valid != true)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;

        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_RESET_CURSOR_COLUMN;

        if (number_of_parameters > 0) {
                parameter = parameters[0];

                /* 0 acts as 1, and this should not be negative */
                if (parameter <= 0)
                        parameter = 1;
        } else {
                parameter = 1;
        }

        terminal_emulator->cursor_row_offset -= parameter;
        return PLY_TERMINAL_EMULATOR_BREAK_STRING;
}

/* Control sequence 'G' for '[G' */
ply_terminal_emulator_break_string_t
on_control_sequence_move_cursor_to_column (ply_terminal_emulator_t *terminal_emulator,
                                           const char               code,
                                           uint32_t                 parameters[],
                                           size_t                   number_of_parameters,
                                           bool                     paramaters_valid)
{
        int parameter;
        ply_rich_text_span_t span;
        size_t maximum_characters;

        size_t string_length = ply_rich_text_get_length (terminal_emulator->current_line);

        ply_trace ("terminal control sequence: move cursor to column");

        assert (code == 'G');

        if (paramaters_valid != true)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;

        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

        if (number_of_parameters > 0) {
                parameter = parameters[0];

                /* 0 acts as 1, and this should not be negative */
                if (parameter <= 0)
                        parameter = 1;
        } else {
                parameter = 1;
        }

        ply_rich_text_get_mutable_span (terminal_emulator->current_line, &span);
        maximum_characters = span.offset + span.range;

        if (parameter > maximum_characters) {
                terminal_emulator->cursor_column = 1;
        } else {
                /* parameter is never 0. the column '1' represents the 0 index on the string */
                terminal_emulator->cursor_column = parameter - 1;
        }

        if (terminal_emulator->cursor_column < 0)
                terminal_emulator->cursor_column = 0;

        fill_offsets_with_padding (terminal_emulator, string_length, terminal_emulator->cursor_column);

        return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
}

/* Control sequence 'K' for '[K' */
ply_terminal_emulator_break_string_t
on_control_sequence_erase_line (ply_terminal_emulator_t *terminal_emulator,
                                const char               code,
                                uint32_t                 parameters[],
                                size_t                   number_of_parameters,
                                bool                     paramaters_valid)
{
        ply_terminal_emulator_erase_line_type_t erase_line_type;
        size_t starting_offset = terminal_emulator->cursor_column;
        size_t string_length = ply_rich_text_get_length (terminal_emulator->current_line);
        size_t i;
        ply_rich_text_span_t span;
        size_t maximum_characters;

        ply_trace ("terminal control sequence: erase line");

        assert (code == 'K');

        if (paramaters_valid != true)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;


        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

        if (number_of_parameters > 0) {
                erase_line_type = (ply_terminal_emulator_erase_line_type_t) parameters[0];

                if (erase_line_type < PLY_TERMINAL_EMULATOR_ERASE_LINE_TYPE_CURSOR_TO_RIGHT ||
                    erase_line_type > PLY_TERMINAL_EMULATOR_ERASE_LINE_TYPE_WHOLE_LINE)
                        return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
        } else {
                erase_line_type = PLY_TERMINAL_EMULATOR_ERASE_LINE_TYPE_CURSOR_TO_RIGHT;
        }

        ply_rich_text_get_mutable_span (terminal_emulator->current_line, &span);
        maximum_characters = span.offset + span.range;

        if (starting_offset >= maximum_characters)
                starting_offset = maximum_characters - 1;

        if (erase_line_type == PLY_TERMINAL_EMULATOR_ERASE_LINE_TYPE_CURSOR_TO_LEFT || erase_line_type == PLY_TERMINAL_EMULATOR_ERASE_LINE_TYPE_WHOLE_LINE) {
                /* Ensure that all characters from the start of the string to the cursor are spaces */
                for (i = starting_offset; i >= 0; i--) {
                        /* Clear all characters at and before the current column */
                        ply_rich_text_set_character (terminal_emulator->current_line, terminal_emulator->current_style, i, " ", 1);

                        if (i <= 0)
                                break;
                }
        }
        if (erase_line_type == PLY_TERMINAL_EMULATOR_ERASE_LINE_TYPE_CURSOR_TO_RIGHT || erase_line_type == PLY_TERMINAL_EMULATOR_ERASE_LINE_TYPE_WHOLE_LINE) {
                /* Clear all characters at and after the current column (until the end of the string */
                for (i = starting_offset; i < string_length; i++) {
                        ply_rich_text_remove_character (terminal_emulator->current_line, i);
                }
        }

        return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
}

/* Control sequence 'P' for '[P' */
ply_terminal_emulator_break_string_t
on_control_sequence_delete_characters (ply_terminal_emulator_t *terminal_emulator,
                                       const char               code,
                                       uint32_t                 parameters[],
                                       size_t                   number_of_parameters,
                                       bool                     paramaters_valid)
{
        int parameter;
        size_t i;
        size_t string_length = ply_rich_text_get_length (terminal_emulator->current_line);

        ply_trace ("terminal control sequence: delete characters");

        assert (code == 'P');

        if (paramaters_valid != true)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;

        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

        if (number_of_parameters > 0) {
                parameter = parameters[0];

                /* 0 acts as 1, and this should not be negative */
                if (parameter <= 0)
                        parameter = 1;
        } else {
                parameter = 1;
        }

        if (terminal_emulator->cursor_column + parameter >= string_length)
                parameter = string_length - 1;

        for (i = terminal_emulator->cursor_column; i < string_length; i++) {
                ply_rich_text_move_character (terminal_emulator->current_line,
                                              i + parameter,
                                              i);
        }

        return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
}

/* Control sequence 'X' for '[X' */
ply_terminal_emulator_break_string_t
on_control_sequence_erase_characters (ply_terminal_emulator_t *terminal_emulator,
                                      const char               code,
                                      uint32_t                 parameters[],
                                      size_t                   number_of_parameters,
                                      bool                     paramaters_valid)
{
        int parameter;
        size_t i;
        size_t string_length = ply_rich_text_get_length (terminal_emulator->current_line);
        size_t delete_offset;

        ply_trace ("terminal control sequence: erase characters");

        assert (code == 'X');

        if (paramaters_valid != true)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;

        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

        if (number_of_parameters > 0) {
                parameter = parameters[0];

                /* 0 acts as 1, and this should not be negative */
                if (parameter <= 0)
                        parameter = 1;
        } else {
                parameter = 1;
        }
        for (i = 0; i < parameter; i++) {
                delete_offset = terminal_emulator->cursor_column + i;

                if (delete_offset >= string_length)
                        break;

                ply_rich_text_set_character (terminal_emulator->current_line, terminal_emulator->current_style, delete_offset, " ", 1);
        }

        return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
}

/* Control sequence 'm' for '[m' */
ply_terminal_emulator_break_string_t
on_control_sequence_set_attributes (ply_terminal_emulator_t *terminal_emulator,
                                    const char               code,
                                    uint32_t                 parameters[],
                                    size_t                   number_of_parameters,
                                    bool                     paramaters_valid)
{
        bool ignore_value = false;
        ply_terminal_color_t default_foreground_color = PLY_TERMINAL_COLOR_DEFAULT;
        ply_terminal_color_t default_background_color = PLY_TERMINAL_COLOR_DEFAULT;

        assert (code == 'm');

        if (paramaters_valid != true)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;

        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

        for (int i = 0; i < number_of_parameters; i++) {
                int parameter = parameters[i];

                if (ignore_value == true) {
                        ignore_value = false;
                        continue;
                }

                /* Paramaters cannot be negative. When no paramter is specified it acts as 0 */
                if (parameter < 0)
                        parameter = 0;

                switch (parameter) {
                case PLY_TERMINAL_ATTRIBUTE_RESET:
                        terminal_emulator->current_style.foreground_color = default_foreground_color;
                        terminal_emulator->current_style.background_color = default_background_color;
                        terminal_emulator->current_style.bold_enabled = false;
                        terminal_emulator->current_style.dim_enabled = false;
                        terminal_emulator->current_style.italic_enabled = false;
                        terminal_emulator->current_style.underline_enabled = false;
                        terminal_emulator->current_style.reverse_enabled = false;
                        break;
                case PLY_TERMINAL_ATTRIBUTE_BOLD:
                        terminal_emulator->current_style.bold_enabled = true;
                        break;
                case PLY_TERMINAL_ATTRIBUTE_NO_BOLD:
                        terminal_emulator->current_style.bold_enabled = false;
                        break;
                case PLY_TERMINAL_ATTRIBUTE_DIM:
                        terminal_emulator->current_style.dim_enabled = true;
                        break;
                case PLY_TERMINAL_ATTRIBUTE_NO_DIM:
                        terminal_emulator->current_style.dim_enabled = false;
                        break;
                case PLY_TERMINAL_ATTRIBUTE_ITALIC:
                        terminal_emulator->current_style.italic_enabled = true;
                        break;
                case PLY_TERMINAL_ATTRIBUTE_NO_ITALIC:
                        terminal_emulator->current_style.italic_enabled = false;
                        break;
                case PLY_TERMINAL_ATTRIBUTE_UNDERLINE:
                        terminal_emulator->current_style.underline_enabled = true;
                        break;
                case PLY_TERMINAL_ATTRIBUTE_NO_UNDERLINE:
                        terminal_emulator->current_style.underline_enabled = false;
                        break;
                case PLY_TERMINAL_ATTRIBUTE_REVERSE:
                        terminal_emulator->current_style.reverse_enabled = true;
                        break;
                case PLY_TERMINAL_ATTRIBUTE_NO_REVERSE:
                        terminal_emulator->current_style.reverse_enabled = false;
                        break;

                /* foreground color handling */
                case PLY_TERMINAL_ATTRIBUTE_FOREGROUND_COLOR_OFFSET + PLY_TERMINAL_COLOR_BLACK
                        ...
                        PLY_TERMINAL_ATTRIBUTE_FOREGROUND_COLOR_OFFSET + PLY_TERMINAL_COLOR_WHITE:
                        terminal_emulator->current_style.foreground_color = (ply_terminal_color_t) (parameters[i] - PLY_TERMINAL_ATTRIBUTE_FOREGROUND_COLOR_OFFSET);
                        break;
                case PLY_TERMINAL_ATTRIBUTE_FOREGROUND_COLOR_OFFSET + PLY_TERMINAL_COLOR_DEFAULT:
                        terminal_emulator->current_style.foreground_color = default_foreground_color;
                        break;

                /* background color handling */
                case PLY_TERMINAL_ATTRIBUTE_BACKGROUND_COLOR_OFFSET + PLY_TERMINAL_COLOR_BLACK
                        ...
                        PLY_TERMINAL_ATTRIBUTE_BACKGROUND_COLOR_OFFSET + PLY_TERMINAL_COLOR_WHITE:
                        terminal_emulator->current_style.background_color = (ply_terminal_color_t) (parameters[i] - PLY_TERMINAL_ATTRIBUTE_BACKGROUND_COLOR_OFFSET);
                        break;
                case PLY_TERMINAL_ATTRIBUTE_BACKGROUND_COLOR_OFFSET + PLY_TERMINAL_COLOR_DEFAULT:
                        terminal_emulator->current_style.background_color = default_background_color;
                        break;

                /* bright color handling, fallback to standard colors */
                case PLY_TERMINAL_ATTRIBUTE_FOREGROUND_BRIGHT_OFFSET + PLY_TERMINAL_COLOR_BLACK
                        ...
                        PLY_TERMINAL_ATTRIBUTE_FOREGROUND_BRIGHT_OFFSET + PLY_TERMINAL_COLOR_WHITE:
                        terminal_emulator->current_style.foreground_color = (ply_terminal_color_t) (parameters[i] - PLY_TERMINAL_ATTRIBUTE_FOREGROUND_BRIGHT_OFFSET);
                        terminal_emulator->current_style.dim_enabled = false;
                        break;
                case PLY_TERMINAL_ATTRIBUTE_BACKGROUND_BRIGHT_OFFSET + PLY_TERMINAL_COLOR_BLACK
                        ...
                        PLY_TERMINAL_ATTRIBUTE_BACKGROUND_BRIGHT_OFFSET + PLY_TERMINAL_COLOR_WHITE:
                        terminal_emulator->current_style.background_color = (ply_terminal_color_t) (parameters[i] - PLY_TERMINAL_ATTRIBUTE_BACKGROUND_BRIGHT_OFFSET);
                        break;

                /* If this is 38 or 48, it means that the next parameter is the attribute for 256 color. Skip it */
                case PLY_TERMINAL_ATTRIBUTE_FOREGROUND_COLOR_OFFSET + PLY_TERMINAL_COLOR_DEFAULT - 1:
                case PLY_TERMINAL_ATTRIBUTE_BACKGROUND_COLOR_OFFSET + PLY_TERMINAL_COLOR_DEFAULT - 1:
                        ignore_value = true;
                        break;
                }
        }

        return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
}

/* backspace character ('\b') */
ply_terminal_emulator_break_string_t
on_escape_character_backspace (ply_terminal_emulator_t *terminal_emulator,
                               const char               code)
{
        ply_trace ("terminal escape character: backspace");

        assert (code == '\b');

        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

        if (terminal_emulator->cursor_column != 0)
                terminal_emulator->cursor_column--;

        return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
}

/* tab character ('\t') */
ply_terminal_emulator_break_string_t
on_escape_character_tab (ply_terminal_emulator_t *terminal_emulator,
                         const char               code)
{
        int pad_character_count = 0;
        size_t string_length = ply_rich_text_get_length (terminal_emulator->current_line);
        size_t new_cursor_position;
        size_t new_string_length;
        ply_rich_text_span_t span;
        size_t maximum_characters;
        ply_rich_text_character_style_t default_style;

        ply_trace ("terminal escape character: tab");

        assert (code == '\t');

        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

        if (terminal_emulator->cursor_column <= 0) {
                pad_character_count = PLY_TERMINAL_EMULATOR_SPACES_PER_TAB;
        } else {
                pad_character_count = PLY_TERMINAL_EMULATOR_SPACES_PER_TAB - (terminal_emulator->cursor_column % PLY_TERMINAL_EMULATOR_SPACES_PER_TAB);
        }

        ply_rich_text_get_mutable_span (terminal_emulator->current_line, &span);
        maximum_characters = span.offset + span.range;

        new_cursor_position = terminal_emulator->cursor_column + pad_character_count;
        if (new_cursor_position >= maximum_characters - 1)
                new_cursor_position = maximum_characters - 1;

        terminal_emulator->cursor_column = new_cursor_position;

        /* If the cursor row offset is not on the same line, don't pad the string
         * This is for when a tab character is inside an escape code, after a new line
         */
        if (terminal_emulator->cursor_row_offset != 0)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;

        if (new_cursor_position < string_length)
                return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;


        new_string_length = string_length + pad_character_count;
        if (new_string_length >= maximum_characters - 1)
                new_string_length = maximum_characters - 1;

        ply_rich_text_character_style_initialize (&default_style);

        for (size_t i = string_length; i < new_string_length; i++) {
                ply_rich_text_set_character (terminal_emulator->current_line, default_style, i, " ", 1);
        }

        return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
}

/* linefeed characters ('\n', '\v', '\f') */
ply_terminal_emulator_break_string_t
on_escape_character_linefeed (ply_terminal_emulator_t *terminal_emulator,
                              const char               code)
{
        assert (code == '\n' || code == '\v' || code == '\f');

        terminal_emulator->cursor_row_offset++;
        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_RESET_CURSOR_COLUMN;

        return PLY_TERMINAL_EMULATOR_BREAK_STRING;
}

/* carriage return ('\r') */
ply_terminal_emulator_break_string_t
on_escape_character_carriage_return (ply_terminal_emulator_t *terminal_emulator,
                                     const char               code)
{
        ply_trace ("terminal escape character: carriage return");

        assert (code == '\r');

        terminal_emulator->cursor_column = 0;
        terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_RESET_CURSOR_COLUMN;

        return PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
}

struct
{
        union
        {
                void                                             *handler;
                ply_terminal_emulator_control_character_handler_t control_character_handler;
                ply_terminal_emulator_escape_sequence_handler_t   escape_sequence_handler;
                ply_terminal_emulator_control_sequence_handler_t  control_sequence_handler;
        };
        const char                           code;
        ply_terminal_emulator_command_type_t type;
}  control_code_dispatch_table[] = {
        { { on_escape_sequence_linefeed                               }, 'D',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_ESCAPE            },
        { { on_escape_sequence_newline                                }, 'E',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_ESCAPE            },
        { { on_escape_sequence_reverse_linefeed                       }, 'M',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_ESCAPE            },
        { { on_control_sequence_insert_blank_characters               }, '@',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE  },
        { { on_control_sequence_move_cursor_up_rows                   }, 'A',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE  },
        { { on_control_sequence_move_cursor_down_rows                 }, 'B',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE  },
        { { on_control_sequence_move_cursor_right                     }, 'C',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE  },
        { { on_control_sequence_move_cursor_left                      }, 'D',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE  },
        { { on_control_sequence_move_cursor_down_rows_to_first_column }, 'E',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE  },
        { { on_control_sequence_move_cursor_up_rows_to_first_column   }, 'F',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE  },
        { { on_control_sequence_move_cursor_to_column                 }, 'G',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE  },
        { { on_control_sequence_erase_line                            }, 'K',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE  },
        { { on_control_sequence_delete_characters                     }, 'P',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE  },
        { { on_control_sequence_erase_characters                      }, 'X',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE  },
        { { on_control_sequence_set_attributes                        }, 'm',  PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE  },
        { { on_escape_character_tab                                   }, '\t', PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_CHARACTER },
        { { on_escape_character_backspace                             }, '\b', PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_CHARACTER },
        { { on_escape_character_linefeed                              }, '\n', PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_CHARACTER },
        { { on_escape_character_linefeed                              }, '\v', PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_CHARACTER },
        { { on_escape_character_linefeed                              }, '\f', PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_CHARACTER },
        { { on_escape_character_carriage_return                       }, '\r', PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_CHARACTER },
        { { NULL                                                      } }
};

bool
ply_terminal_emulator_dispatch_control_sequence_command (ply_terminal_emulator_t         *terminal_emulator,
                                                         ply_terminal_emulator_command_t *command)
{
        bool break_string = false;

        for (int i = 0; control_code_dispatch_table[i].handler != NULL; i++) {
                if (control_code_dispatch_table[i].code == command->code && control_code_dispatch_table[i].type == command->type) {
                        switch (command->type) {
                        case PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE:
                                break_string = control_code_dispatch_table[i].control_sequence_handler (terminal_emulator, command->code,
                                                                                                        (uint32_t *) ply_array_get_uint32_elements (command->parameters),
                                                                                                        ply_array_get_size (command->parameters),
                                                                                                        command->parameters_valid);
                                ply_array_free (command->parameters);
                                break;
                        case PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_CHARACTER:
                                break_string = control_code_dispatch_table[i].control_character_handler (terminal_emulator, command->code);
                                break;
                        case PLY_TERMINAL_EMULATOR_COMMAND_TYPE_ESCAPE:
                                break_string = control_code_dispatch_table[i].escape_sequence_handler (terminal_emulator, command->code);
                                break;
                        }
                        break;
                }
        }

        return break_string;
}

ply_rich_text_t *
ply_terminal_emulator_get_nth_line (ply_terminal_emulator_t *terminal_emulator,
                                    int                      line_number)
{
        ply_rich_text_t *const *console_lines = (ply_rich_text_t *const *) ply_array_get_pointer_elements (terminal_emulator->lines);
        return console_lines[line_number % terminal_emulator->number_of_rows];
}

int
ply_terminal_emulator_get_line_count (ply_terminal_emulator_t *terminal_emulator)
{
        return terminal_emulator->line_count;
}

static ply_terminal_emulator_break_string_t
ply_terminal_emulator_flush_pending_character_to_line (ply_terminal_emulator_t *terminal_emulator)
{
        ply_terminal_emulator_break_string_t break_string = PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
        ply_rich_text_span_t span;
        const char *character_bytes;
        size_t character_size;
        size_t maximum_characters;

        character_bytes = ply_buffer_get_bytes (terminal_emulator->pending_character);
        character_size = ply_buffer_get_size (terminal_emulator->pending_character);

        ply_rich_text_set_character (terminal_emulator->current_line,
                                     terminal_emulator->current_style,
                                     terminal_emulator->cursor_column,
                                     character_bytes,
                                     character_size);
        ply_buffer_clear (terminal_emulator->pending_character);

        terminal_emulator->cursor_column++;

        ply_rich_text_get_mutable_span (terminal_emulator->current_line, &span);

        maximum_characters = span.offset + span.range;

        if (terminal_emulator->cursor_column >= maximum_characters) {
                terminal_emulator->cursor_row_offset++;
                terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_RESET_CURSOR_COLUMN;
                break_string = PLY_TERMINAL_EMULATOR_BREAK_STRING;
        }

        return break_string;
}

void
ply_terminal_emulator_parse_substring (ply_terminal_emulator_t *terminal_emulator,
                                       ply_rich_text_t         *terminal_emulator_line,
                                       const char              *input,
                                       size_t                   number_of_bytes_to_parse,
                                       const char             **unparsed_input,
                                       size_t                  *number_of_unparsed_bytes)
{
        size_t input_length = number_of_bytes_to_parse;
        size_t new_length;
        size_t i = 0;
        ply_terminal_emulator_break_string_t break_string = PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
        ply_terminal_emulator_command_t *command;
        ply_rich_text_span_t span;
        size_t maximum_characters;
        ply_list_node_t *node;

        terminal_emulator->current_line = terminal_emulator_line;

        ply_rich_text_get_mutable_span (terminal_emulator->current_line, &span);
        maximum_characters = span.offset + span.range;

        if (terminal_emulator->cursor_column >= maximum_characters)
                terminal_emulator->cursor_column = 0;

        new_length = ply_rich_text_get_length (terminal_emulator->current_line);

        if (terminal_emulator->cursor_column >= new_length)
                fill_offsets_with_padding (terminal_emulator, new_length, terminal_emulator->cursor_column);

        while (i < input_length) {
                ply_utf8_character_byte_type_t character_byte_type;
                char debug_string[1] = "X";
                const char *input_bytes = &input[i];

                if (terminal_emulator->show_escape_sequences) {
                        if (iscntrl (*input_bytes) && *input_bytes != '\n' &&
                            (input_bytes[0] != '\e' || ((i + 1 < input_length) && input_bytes[1] == '['))) {
                                ply_buffer_clear (terminal_emulator->pending_character);
                                ply_buffer_append_bytes (terminal_emulator->pending_character, "^", 1);
                                terminal_emulator->pending_character_size = 1;
                                ply_terminal_emulator_flush_pending_character_to_line (terminal_emulator);

                                debug_string[0] = *input_bytes + PLY_TERMINAL_CONTROL_CODE_LETTER_OFFSET;
                                input_bytes = debug_string;
                        }
                }

                if (break_string == PLY_TERMINAL_EMULATOR_BREAK_STRING && terminal_emulator->state == PLY_TERMINAL_EMULATOR_TERMINAL_STATE_UNESCAPED) {
                        break_string = PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;
                        break;
                }

                terminal_emulator->break_action = PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_PRESERVE_CURSOR_COLUMN;

                character_byte_type = ply_utf8_character_get_byte_type (*input_bytes);

                if (character_byte_type != PLY_UTF8_CHARACTER_BYTE_TYPE_CONTINUATION)
                        ply_buffer_clear (terminal_emulator->pending_character);

                /* If the previous byte was also a UTF-8 leading byte, handle it as an invalid character */
                if (terminal_emulator->pending_character_state == PLY_TERMINAL_EMULATOR_UTF8_CHARACTER_PARSE_STATE_MULTI_BYTE &&
                    character_byte_type != PLY_UTF8_CHARACTER_BYTE_TYPE_CONTINUATION &&
                    terminal_emulator->state == PLY_TERMINAL_EMULATOR_TERMINAL_STATE_UNESCAPED) {
                        ply_buffer_append_bytes (terminal_emulator->pending_character, "?", 1);
                        break_string = ply_terminal_emulator_flush_pending_character_to_line (terminal_emulator);
                }

                if (PLY_UTF8_CHARACTER_BYTE_TYPE_IS_MULTI_BYTE (character_byte_type)) {
                        /* Multi-byte Unicode characters */
                        terminal_emulator->pending_character_state = PLY_TERMINAL_EMULATOR_UTF8_CHARACTER_PARSE_STATE_MULTI_BYTE;
                        terminal_emulator->pending_character_size = ply_utf8_character_get_size_from_byte_type (character_byte_type);

                        ply_buffer_append_bytes (terminal_emulator->pending_character, input_bytes, 1);

                        i++;
                        continue;
                } else if (character_byte_type == PLY_UTF8_CHARACTER_BYTE_TYPE_1_BYTE) {
                        /* Ascii characters could potentially be used in escape sequences */
                        terminal_emulator->pending_character_state = PLY_TERMINAL_EMULATOR_UTF8_CHARACTER_PARSE_STATE_SINGLE_BYTE;
                        terminal_emulator->pending_character_size = ply_utf8_character_get_size_from_byte_type (character_byte_type);
                } else if (character_byte_type == PLY_UTF8_CHARACTER_BYTE_TYPE_END_OF_STRING) {
                        i++;
                        continue;
                } else if (character_byte_type == PLY_UTF8_CHARACTER_BYTE_TYPE_INVALID) {
                        i++;
                        continue;
                } else if (character_byte_type == PLY_UTF8_CHARACTER_BYTE_TYPE_CONTINUATION) {
                        if (terminal_emulator->pending_character_state == PLY_TERMINAL_EMULATOR_UTF8_CHARACTER_PARSE_STATE_MULTI_BYTE) {
                                /* Handle the auxiliary unicode byte if handling a multi-byte character */
                                if (terminal_emulator->pending_character_state == PLY_TERMINAL_EMULATOR_UTF8_CHARACTER_PARSE_STATE_MULTI_BYTE)
                                        ply_buffer_append_bytes (terminal_emulator->pending_character, input_bytes, 1);

                                i++;

                                /* The multi-byte character is not finished yet, continue the loop */
                                if (ply_buffer_get_size (terminal_emulator->pending_character) < terminal_emulator->pending_character_size)
                                        continue;
                        } else {
                                /* If this is an auxiliary Unicode byte when not handling a multi-byte character, replace it with a placeholder */
                                terminal_emulator->pending_character_size = 1;
                                ply_buffer_clear (terminal_emulator->pending_character);
                                ply_buffer_append_bytes (terminal_emulator->pending_character, "?", 1);

                                break_string = ply_terminal_emulator_flush_pending_character_to_line (terminal_emulator);
                                i++;

                                continue;
                        }
                }

                /* If the current character is a multi-byte character, and all the bytes are received */
                if (terminal_emulator->pending_character_state == PLY_TERMINAL_EMULATOR_UTF8_CHARACTER_PARSE_STATE_MULTI_BYTE) {
                        /* Drop and skip the multi-byte character if is still escaped */
                        if (terminal_emulator->state != PLY_TERMINAL_EMULATOR_TERMINAL_STATE_UNESCAPED) {
                                ply_buffer_clear (terminal_emulator->pending_character);
                                continue;
                        }

                        terminal_emulator->pending_character_state = PLY_TERMINAL_EMULATOR_UTF8_CHARACTER_PARSE_STATE_SINGLE_BYTE;
                        break_string = ply_terminal_emulator_flush_pending_character_to_line (terminal_emulator);
                        continue;
                }

                switch (terminal_emulator->state) {
                case PLY_TERMINAL_EMULATOR_TERMINAL_STATE_UNESCAPED:
                        if (*input_bytes == '\e') {
                                terminal_emulator->staged_command = ply_terminal_emulator_command_new ();

                                terminal_emulator->state = PLY_TERMINAL_EMULATOR_TERMINAL_STATE_ESCAPED;
                        } else if (iscntrl (*input_bytes) && *input_bytes != '\e') {
                                terminal_emulator->staged_command = ply_terminal_emulator_command_new ();
                                terminal_emulator->staged_command->code = *input_bytes;
                                terminal_emulator->staged_command->type = PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_CHARACTER;
                                ply_list_append_data (terminal_emulator->pending_commands, terminal_emulator->staged_command);
                        } else {
                                ply_buffer_append_bytes (terminal_emulator->pending_character, input_bytes, 1);
                                break_string = ply_terminal_emulator_flush_pending_character_to_line (terminal_emulator);
                        }
                        break;
                case PLY_TERMINAL_EMULATOR_TERMINAL_STATE_ESCAPED:
                        if (*input_bytes == '[') {
                                terminal_emulator->pending_parameter_value = 0;
                                terminal_emulator->staged_command->parameters = ply_array_new (PLY_ARRAY_ELEMENT_TYPE_UINT32);
                                terminal_emulator->staged_command->type = PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_SEQUENCE;
                                terminal_emulator->staged_command->parameters_valid = true;

                                terminal_emulator->last_parameter_was_integer = false;

                                terminal_emulator->state = PLY_TERMINAL_EMULATOR_TERMINAL_STATE_CONTROL_SEQUENCE_PARAMETER;
                        } else {
                                terminal_emulator->staged_command->code = *input_bytes;
                                terminal_emulator->staged_command->type = PLY_TERMINAL_EMULATOR_COMMAND_TYPE_ESCAPE;
                                ply_list_append_data (terminal_emulator->pending_commands, terminal_emulator->staged_command);
                                terminal_emulator->state = PLY_TERMINAL_EMULATOR_TERMINAL_STATE_UNESCAPED;
                        }
                        break;
                case PLY_TERMINAL_EMULATOR_TERMINAL_STATE_CONTROL_SEQUENCE_PARAMETER:
                        /* Characters that end the control sequence, and define the command */
                        if ((unsigned char) *input_bytes >= PLY_TERMINAL_ESCAPE_CODE_COMMAND_MINIMUM &&
                            (unsigned char) *input_bytes <= PLY_TERMINAL_ESCAPE_CODE_COMMAND_MAXIMUM) {
                                terminal_emulator->state = PLY_TERMINAL_EMULATOR_TERMINAL_STATE_UNESCAPED;
                                terminal_emulator->staged_command->code = *input_bytes;


                                ply_array_add_uint32_element (terminal_emulator->staged_command->parameters, terminal_emulator->pending_parameter_value);
                                terminal_emulator->pending_parameter_value = 0;
                                ply_list_append_data (terminal_emulator->pending_commands, terminal_emulator->staged_command);

                                break;
                        } else if (iscntrl (*input_bytes) && *input_bytes != '\e') {
                                ply_terminal_emulator_command_t *nested_command = ply_terminal_emulator_command_new ();
                                nested_command->code = *input_bytes;
                                nested_command->type = PLY_TERMINAL_EMULATOR_COMMAND_TYPE_CONTROL_CHARACTER;
                                ply_list_append_data (terminal_emulator->pending_commands, nested_command);
                        } else if (*input_bytes == ';' || (isdigit (*input_bytes))) {
                                if (isdigit (*input_bytes)) {
                                        /* If the previous character was an integer, and this one is an integer, it is probably the next digit */
                                        terminal_emulator->pending_parameter_value = terminal_emulator->pending_parameter_value * 10;
                                        terminal_emulator->pending_parameter_value += *input_bytes - '0';

                                        terminal_emulator->last_parameter_was_integer = true;
                                } else if (*input_bytes == ';') {
                                        /* Double ;;'s imply a 0 */
                                        if (terminal_emulator->last_parameter_was_integer == false) {
                                                ply_array_add_uint32_element (terminal_emulator->staged_command->parameters, 0);
                                        } else {
                                                ply_array_add_uint32_element (terminal_emulator->staged_command->parameters, terminal_emulator->pending_parameter_value);
                                        }

                                        terminal_emulator->pending_parameter_value = 0;
                                        terminal_emulator->last_parameter_was_integer = false;
                                }
                                break;
                        } else {
                                /* invalid characters in the middle of the escape sequence invalidate it */
                                terminal_emulator->staged_command->parameters_valid = false;
                        }
                        break;
                }

                if (terminal_emulator->state == PLY_TERMINAL_EMULATOR_TERMINAL_STATE_UNESCAPED) {
                        ply_list_foreach (terminal_emulator->pending_commands, node) {
                                ply_terminal_emulator_break_string_t break_string_value = PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE;

                                command = ply_list_node_get_data (node);

                                break_string_value = ply_terminal_emulator_dispatch_control_sequence_command (terminal_emulator, command);
                                if (break_string_value != PLY_TERMINAL_EMULATOR_BREAK_STRING_NONE)
                                        break_string = break_string_value;

                                free (command);
                        }
                        ply_list_remove_all_nodes (terminal_emulator->pending_commands);
                }

                i++;
        }

        *unparsed_input = &input[i];
        *number_of_unparsed_bytes = number_of_bytes_to_parse - i;

        /* Moving down, so create new lines */
        while (terminal_emulator->cursor_row_offset > 0) {
                ply_rich_text_t *terminal_emulator_line;

                terminal_emulator->cursor_row_offset--;
                terminal_emulator_line = ply_terminal_emulator_get_nth_line (terminal_emulator, terminal_emulator->line_count);

                if (terminal_emulator) {
                        ply_rich_text_remove_characters (terminal_emulator_line);
                }

                terminal_emulator->line_count++;
        }

        if (terminal_emulator->break_action == PLY_TERMINAL_EMULATOR_BREAK_STRING_ACTION_RESET_CURSOR_COLUMN)
                terminal_emulator->cursor_column = 0;

        terminal_emulator->current_line = NULL;
}

void
ply_terminal_emulator_parse_lines (ply_terminal_emulator_t *terminal_emulator,
                                   const char              *text,
                                   size_t                   size)
{
        ply_rich_text_t *terminal_emulator_line = NULL;
        size_t cursor_row;
        size_t first_row;
        size_t last_row;
        size_t unparsed_text_length;
        const char *unparsed_text;

        unparsed_text = text;
        unparsed_text_length = size;
        while (unparsed_text_length > 0) {
                assert (terminal_emulator->line_count != 0);

                first_row = 0;
                last_row = terminal_emulator->line_count - 1;

                /* Moving up, make sure to stop it at the top */
                if (terminal_emulator->cursor_row_offset < 0) {
                        size_t lines_to_move_up = -1 * terminal_emulator->cursor_row_offset;

                        if (lines_to_move_up > terminal_emulator->line_count)
                                terminal_emulator->cursor_row_offset = first_row;
                }

                cursor_row = last_row + terminal_emulator->cursor_row_offset;

                terminal_emulator_line = ply_terminal_emulator_get_nth_line (terminal_emulator, cursor_row);
                ply_terminal_emulator_parse_substring (terminal_emulator, terminal_emulator_line, unparsed_text, unparsed_text_length, &unparsed_text, &unparsed_text_length);
        }

        if (unparsed_text != text)
                ply_trigger_pull (terminal_emulator->output_trigger, text);
}

void
ply_terminal_emulator_convert_boot_buffer (ply_terminal_emulator_t *terminal_emulator,
                                           ply_buffer_t            *boot_buffer)
{
        ply_terminal_emulator_parse_lines (terminal_emulator, ply_buffer_get_bytes (boot_buffer), ply_buffer_get_size (boot_buffer));
}

void
ply_terminal_emulator_watch_for_output (ply_terminal_emulator_t               *terminal_emulator,
                                        ply_terminal_emulator_output_handler_t handler,
                                        void                                  *user_data)
{
        ply_trigger_add_handler (terminal_emulator->output_trigger,
                                 (ply_trigger_handler_t)
                                 handler,
                                 user_data);
}
