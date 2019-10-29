#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright © 2018 Endless Mobile, Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

import argparse
import os
import pwd
import sys
import gi
gi.require_version('Malcontent', '0')  # noqa
from gi.repository import Malcontent, GLib, Gio


# Exit codes, which are a documented part of the API.
EXIT_SUCCESS = 0
EXIT_INVALID_OPTION = 1
EXIT_PERMISSION_DENIED = 2
EXIT_PATH_NOT_ALLOWED = 3


def __get_app_filter(user_id, interactive):
    """Get the app filter for `user_id` off the bus.

    If `interactive` is `True`, interactive polkit authorisation dialogues will
    be allowed. An exception will be raised on failure."""
    if interactive:
        flags = Malcontent.GetAppFilterFlags.INTERACTIVE
    else:
        flags = Malcontent.GetAppFilterFlags.NONE

    connection = Gio.bus_get_sync(Gio.BusType.SYSTEM)
    manager = Malcontent.Manager.new(connection)
    return manager.get_app_filter(
        user_id=user_id,
        flags=flags, cancellable=None)


def __get_app_filter_or_error(user_id, interactive):
    """Wrapper around __get_app_filter() which prints an error and raises
    SystemExit, rather than an internal exception."""
    try:
        return __get_app_filter(user_id, interactive)
    except GLib.Error as e:
        print('Error getting app filter for user {}: {}'.format(
            user_id, e.message), file=sys.stderr)
        raise SystemExit(EXIT_PERMISSION_DENIED)


def __set_app_filter(user_id, app_filter, interactive):
    """Set the app filter for `user_id` off the bus.

    If `interactive` is `True`, interactive polkit authorisation dialogues will
    be allowed. An exception will be raised on failure."""
    if interactive:
        flags = Malcontent.GetAppFilterFlags.INTERACTIVE
    else:
        flags = Malcontent.GetAppFilterFlags.NONE

    connection = Gio.bus_get_sync(Gio.BusType.SYSTEM)
    manager = Malcontent.Manager.new(connection)
    manager.set_app_filter(
        user_id=user_id, app_filter=app_filter,
        flags=flags, cancellable=None)


def __set_app_filter_or_error(user_id, app_filter, interactive):
    """Wrapper around __set_app_filter() which prints an error and raises
    SystemExit, rather than an internal exception."""
    try:
        __set_app_filter(user_id, app_filter, interactive)
    except GLib.Error as e:
        print('Error setting app filter for user {}: {}'.format(
            user_id, e.message), file=sys.stderr)
        raise SystemExit(EXIT_PERMISSION_DENIED)


def __lookup_user_id(user):
    """Convert a command-line specified username or ID into a user ID. If
    `user` is empty, use the current user ID.

    Raise KeyError if lookup fails."""
    if user == '':
        return os.getuid()
    elif user.isdigit():
        return int(user)
    else:
        return pwd.getpwnam(user).pw_uid


def __lookup_user_id_or_error(user):
    """Wrapper around __lookup_user_id() which prints an error and raises
    SystemExit, rather than an internal exception."""
    try:
        return __lookup_user_id(user)
    except KeyError:
        print('Error getting ID for username {}'.format(user), file=sys.stderr)
        raise SystemExit(EXIT_INVALID_OPTION)


oars_value_mapping = {
    Malcontent.AppFilterOarsValue.UNKNOWN: "unknown",
    Malcontent.AppFilterOarsValue.NONE: "none",
    Malcontent.AppFilterOarsValue.MILD: "mild",
    Malcontent.AppFilterOarsValue.MODERATE: "moderate",
    Malcontent.AppFilterOarsValue.INTENSE: "intense",
}


def __oars_value_to_string(value):
    """Convert an Malcontent.AppFilterOarsValue to a human-readable
    string."""
    try:
        return oars_value_mapping[value]
    except KeyError:
        return "invalid (OARS value {})".format(value)


def __oars_value_from_string(value_str):
    """Convert a human-readable string to an
    Malcontent.AppFilterOarsValue."""
    for k, v in oars_value_mapping.items():
        if v == value_str:
            return k
    raise KeyError('Unknown OARS value ‘{}’'.format(value_str))


def command_get(user, quiet=False, interactive=True):
    """Get the app filter for the given user."""
    user_id = __lookup_user_id_or_error(user)
    app_filter = __get_app_filter_or_error(user_id, interactive)

    print('App filter for user {} retrieved:'.format(user_id))

    sections = app_filter.get_oars_sections()
    for section in sections:
        value = app_filter.get_oars_value(section)
        print('  {}: {}'.format(section, oars_value_mapping[value]))
    if not sections:
        print('  (No OARS values)')

    if app_filter.is_user_installation_allowed():
        print('App installation is allowed to user repository')
    else:
        print('App installation is disallowed to user repository')

    if app_filter.is_system_installation_allowed():
        print('App installation is allowed to system repository')
    else:
        print('App installation is disallowed to system repository')


def command_monitor(user, quiet=False, interactive=True):
    """Monitor app filter changes for the given user."""
    if user == '':
        filter_user_id = 0
    else:
        filter_user_id = __lookup_user_id_or_error(user)
    apply_filter = (user != '')

    def _on_app_filter_changed(manager, changed_user_id):
        if not apply_filter or changed_user_id == filter_user_id:
            print('App filter changed for user ID {}'.format(changed_user_id))

    connection = Gio.bus_get_sync(Gio.BusType.SYSTEM)
    manager = Malcontent.Manager.new(connection)
    manager.connect('app-filter-changed', _on_app_filter_changed)

    if apply_filter:
        print('Monitoring app filter changes for '
              'user ID {}'.format(filter_user_id))
    else:
        print('Monitoring app filter changes for all users')

    # Loop until Ctrl+C is pressed.
    context = GLib.MainContext.default()
    while True:
        try:
            context.iteration(may_block=True)
        except KeyboardInterrupt:
            break


# Simple check to check whether @arg is a valid flatpak ref - it uses the
# same logic as 'MctAppFilter' to determine it and should be kept in sync
# with its implementation
def is_valid_flatpak_ref(arg):
    parts = arg.split('/')
    return (len(parts) == 4 and \
            (parts[0] == 'app' or parts[0] == 'runtime') and \
            parts[1] != '' and parts[2] != '' and parts[3] != '')


# Simple check to check whether @arg is a valid content type - it uses the
# same logic as 'MctAppFilter' to determine it and should be kept in sync
# with its implementation
def is_valid_content_type(arg):
    parts = arg.split('/')
    return (len(parts) == 2 and \
            parts[0] != '' and parts[1] != '')


def command_check(user, arg, quiet=False, interactive=True):
    """Check the given path, content type or flatpak ref is runnable by the
    given user, according to their app filter."""
    user_id = __lookup_user_id_or_error(user)
    app_filter = __get_app_filter_or_error(user_id, interactive)

    is_maybe_flatpak_id = arg.startswith('app/') and arg.count('/') < 3
    is_maybe_flatpak_ref = is_valid_flatpak_ref(arg)
    # Only check if arg is a valid content type if not already considered a
    # valid flatpak id, otherwise we always get multiple types recognised
    # when passing flatpak IDs as argument
    is_maybe_content_type = not is_maybe_flatpak_id and is_valid_content_type(arg)
    is_maybe_path = os.path.exists(arg)

    recognised_types = sum([is_maybe_flatpak_id, is_maybe_flatpak_ref,
                            is_maybe_content_type, is_maybe_path])
    if recognised_types == 0:
        print('Unknown argument ‘{}’'.format(arg), file=sys.stderr)
        raise SystemExit(EXIT_INVALID_OPTION)
    elif recognised_types > 1:
        print('Ambiguous argument ‘{}’ recognised as multiple types'.format(arg),
              file=sys.stderr)
        raise SystemExit(EXIT_INVALID_OPTION)
    elif is_maybe_flatpak_id:
        # Flatpak app ID
        arg = arg[4:]
        is_allowed = app_filter.is_flatpak_app_allowed(arg)
        noun = 'Flatpak app ID'
    elif is_maybe_flatpak_ref:
        # Flatpak ref
        is_allowed = app_filter.is_flatpak_ref_allowed(arg)
        noun = 'Flatpak ref'
    elif is_maybe_content_type:
        # Content type
        is_allowed = app_filter.is_content_type_allowed(arg)
        noun = 'Content type'
    elif is_maybe_path:
        path = os.path.abspath(arg)
        is_allowed = app_filter.is_path_allowed(path)
        noun = 'Path'
    else:
        raise AssertionError('code should not be reached')

    if is_allowed:
        if not quiet:
            print('{} {} is allowed by app filter for user {}'.format(
                noun, arg, user_id))
        return
    else:
        if not quiet:
            print('{} {} is not allowed by app filter for user {}'.format(
                noun, arg, user_id))
        raise SystemExit(EXIT_PATH_NOT_ALLOWED)


def command_oars_section(user, section, quiet=False, interactive=True):
    """Get the value of the given OARS section for the given user, according
    to their OARS filter."""
    user_id = __lookup_user_id_or_error(user)
    app_filter = __get_app_filter_or_error(user_id, interactive)

    value = app_filter.get_oars_value(section)
    print('OARS section ‘{}’ for user {} has value ‘{}’'.format(
        section, user_id, __oars_value_to_string(value)))


def command_set(user, allow_user_installation=True,
                allow_system_installation=False, app_filter_args=None,
                quiet=False, interactive=True):
    """Set the app filter for the given user."""
    user_id = __lookup_user_id_or_error(user)
    builder = Malcontent.AppFilterBuilder.new()
    builder.set_allow_user_installation(allow_user_installation)
    builder.set_allow_system_installation(allow_system_installation)

    for arg in app_filter_args:
        if '=' in arg:
            [section, value_str] = arg.split('=', 2)
            try:
                value = __oars_value_from_string(value_str)
            except KeyError:
                print('Unknown OARS value ‘{}’'.format(value_str),
                      file=sys.stderr)
                raise SystemExit(EXIT_INVALID_OPTION)
            builder.set_oars_value(section, value)
        else:
            is_maybe_flatpak_ref = is_valid_flatpak_ref(arg)
            is_maybe_content_type = is_valid_content_type(arg)
            is_maybe_path = os.path.exists(arg)

            recognised_types = sum([is_maybe_flatpak_ref,
                                    is_maybe_content_type, is_maybe_path])
            if recognised_types == 0:
                print('Unknown argument ‘{}’'.format(arg), file=sys.stderr)
                raise SystemExit(EXIT_INVALID_OPTION)
            elif recognised_types > 1:
                print('Ambiguous argument ‘{}’ recognised as multiple types'.format(arg),
                      file=sys.stderr)
                raise SystemExit(EXIT_INVALID_OPTION)
            elif is_maybe_flatpak_ref:
                builder.blacklist_flatpak_ref(arg)
            elif is_maybe_content_type:
                builder.blacklist_content_type(arg)
            elif is_maybe_path:
                path = os.path.abspath(arg)
                builder.blacklist_path(path)
            else:
                raise AssertionError('code should not be reached')

    app_filter = builder.end()

    __set_app_filter_or_error(user_id, app_filter, interactive)

    if not quiet:
        print('App filter for user {} set'.format(user_id))


def main():
    # Parse command line arguments
    parser = argparse.ArgumentParser(
        description='Query and update parental controls.')
    subparsers = parser.add_subparsers(metavar='command',
                                       help='command to run (default: ‘get’)')
    parser.set_defaults(function=command_get)
    parser.add_argument('-q', '--quiet', action='store_true',
                        help='output no informational messages')
    parser.set_defaults(quiet=False)

    # Common options for the subcommands which might need authorisation.
    common_parser = argparse.ArgumentParser(add_help=False)
    group = common_parser.add_mutually_exclusive_group()
    group.add_argument('-n', '--no-interactive', dest='interactive',
                       action='store_false',
                       help='do not allow interactive polkit authorization '
                            'dialogues')
    group.add_argument('--interactive', dest='interactive',
                       action='store_true',
                       help='opposite of --no-interactive')
    common_parser.set_defaults(interactive=True)

    # ‘get’ command
    parser_get = subparsers.add_parser('get', parents=[common_parser],
                                       help='get current parental controls '
                                            'settings')
    parser_get.set_defaults(function=command_get)
    parser_get.add_argument('user', default='', nargs='?',
                            help='user ID or username to get the app filter '
                                 'for (default: current user)')

    # ‘monitor’ command
    parser_monitor = subparsers.add_parser('monitor',
                                           help='monitor parental controls '
                                                'settings changes')
    parser_monitor.set_defaults(function=command_monitor)
    parser_monitor.add_argument('user', default='', nargs='?',
                                help='user ID or username to monitor the app '
                                     'filter for (default: all users)')

    # ‘check’ command
    parser_check = subparsers.add_parser('check', parents=[common_parser],
                                         help='check whether a path, content '
                                              'type or flatpak ref is '
                                              'allowed by app filter')
    parser_check.set_defaults(function=command_check)
    parser_check.add_argument('user', default='', nargs='?',
                              help='user ID or username to get the app filter '
                                   'for (default: current user)')
    parser_check.add_argument('arg',
                              help='path to a program, content type or '
                                   'flatpak ref to check')

    # ‘oars-section’ command
    parser_oars_section = subparsers.add_parser('oars-section',
                                                parents=[common_parser],
                                                help='get the value of a '
                                                     'given OARS section')
    parser_oars_section.set_defaults(function=command_oars_section)
    parser_oars_section.add_argument('user', default='', nargs='?',
                                     help='user ID or username to get the '
                                          'OARS filter for (default: current '
                                          'user)')
    parser_oars_section.add_argument('section', help='OARS section to get')

    # ‘set’ command
    parser_set = subparsers.add_parser('set', parents=[common_parser],
                                       help='set current parental controls '
                                            'settings')
    parser_set.set_defaults(function=command_set)
    parser_set.add_argument('user', default='', nargs='?',
                            help='user ID or username to get the app filter '
                                 'for (default: current user)')
    parser_set.add_argument('--allow-user-installation',
                            dest='allow_user_installation',
                            action='store_true',
                            help='allow installation to the user flatpak '
                                 'repo in general')
    parser_set.add_argument('--disallow-user-installation',
                            dest='allow_user_installation',
                            action='store_false',
                            help='unconditionally disallow installation to '
                                 'the user flatpak repo')
    parser_set.add_argument('--allow-system-installation',
                            dest='allow_system_installation',
                            action='store_true',
                            help='allow installation to the system flatpak '
                                 'repo in general')
    parser_set.add_argument('--disallow-system-installation',
                            dest='allow_system_installation',
                            action='store_false',
                            help='unconditionally disallow installation to '
                                 'the system flatpak repo')
    parser_set.add_argument('app_filter_args', nargs='*',
                            help='paths, content types or flatpak refs to '
                                 'blacklist and OARS section=value '
                                 'pairs to store')
    parser_set.set_defaults(allow_user_installation=True,
                            allow_system_installation=False)

    # Parse the command line arguments and run the subcommand.
    args = parser.parse_args()
    args_dict = dict((k, v) for k, v in vars(args).items() if k != 'function')
    args.function(**args_dict)


if __name__ == '__main__':
    main()
