#!/usr/bin/env python3

import os
import pathlib
import sys

if len(sys.argv) != 2:
    print('Expected 1 argument, but got {}'.format(len(sys.argv) - 1))
    sys.exit(1)

input_path = pathlib.Path(sys.argv[1])

variable_name = input_path.stem.replace('-', '_')
print('static const char *{}_string ='.format(variable_name))

for line in input_path.read_text().splitlines():
    # escape backlashes
    line = line.replace('\\', '\\\\')
    # escape internal quotes
    line = line.replace('"', '\\"')
    # print with quotes (so it becomes a C literal string)
    print('  "' + line + '\\n"')

print(';')
