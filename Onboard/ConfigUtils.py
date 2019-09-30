# -*- coding: utf-8
"""
File containing ConfigObject.
"""

from __future__ import division, print_function, unicode_literals

### Logging ###
import logging
_logger = logging.getLogger("ConfigUtils")
###############

import os
import sys
from ast import literal_eval
try:
    import configparser
except ImportError:
    # python2 fallback
    import ConfigParser as configparser

from gi.repository import GLib, Gio

from Onboard.Exceptions import SchemaError
from Onboard.utils import pack_name_value_list, unpack_name_value_list, \
                          unicode_str
import Onboard.osk as osk

_CAN_SET_HOOK       = "_can_set_"       # return true if value is valid
_GSETTINGS_GET_HOOK = "_gsettings_get_" # retrieve from gsettings
_GSETTINGS_SET_HOOK = "_gsettings_set_" # store into gsettings
_UNPACK_HOOK        = "_unpack_"        # convert gsettings value -> property
_PACK_HOOK          = "_pack_"          # convert property -> gsettings value
_POST_NOTIFY_HOOK   = "_post_notify_"   # runs after all listeners notified
_NOTIFY_CALLBACKS   = "_{}_notify_callbacks" # name of list of callbacka

class ConfigObject(object):
    """
    Class for a configuration object with multiple key-value tuples.
    It aims to unify the handling of python properties, gsettings keys,
    system default keys and command line options.

    Python properties and notification functions are created
    automagically for all keys added in _init_keys().
    """
    def __init__(self, parent = None, schema = ""):
        self.parent = parent       # parent ConfigObject
        self.children = []         # child config objects; not necessarily
                                   #   reflecting the gsettings hierarchy
        self.schema = schema       # schema-path to the gsettings object
        self.gskeys = {}           # key-value objects {property name, GSKey()}
        self.sysdef_section = None # system defaults section name
        self.system_defaults = {}  # system defaults {property name, value}
        self._osk_dconf = None

        # add keys in here
        self._init_keys()

        # check if the gsettings schema is installed
        if not self.schema in Gio.Settings.list_schemas():
            raise SchemaError(_("gsettings schema for '{}' is not installed").
                                                          format(self.schema))

        # create gsettings object and its python properties
        self.settings = Gio.Settings.new(self.schema)
        for gskey in list(self.gskeys.values()):
            gskey.settings = self.settings
            self._setup_property(gskey)

        # check hook function names
        self.check_hooks()

    def _init_keys(self):
        """ overload this and use add_key() to add key-value tuples """
        pass

    def add_key(self, key, default, type_string=None, enum=None,
                prop = None, sysdef = None, writable = True):
        """ Convenience function to create and add a new GSKey. """
        gskey = GSKey(None, key, default, type_string, enum,
                      prop, sysdef, writable)
        self.gskeys[gskey.prop] = gskey
        return gskey

    def add_optional_child(self, type):
        """ Add child ConfigObject or None if it's schema doesn't exist. """
        try:
            co = type(self)
            self.children.append(co)
        except SchemaError as e:
            _logger.warning(unicode_str(e))
            co = None
        return co

    def find_key(self, key):
        """ Search for key (gsettings name) """
        for gskey in self.gskeys.values():
            if gskey.key == key:
                return gskey
        return None

    def get_root(self):
        """ Return the root config object """
        co = self
        while co:
            if co.parent is None:
                return co
            co = co.parent

    def check_hooks(self):
        """
        Simple runtime plausibility check for all overloaded hook functions.
        Does the property part of the function name reference an existing
        config property?
        """
        prefixes = [_CAN_SET_HOOK,
                    _GSETTINGS_GET_HOOK,
                    _GSETTINGS_SET_HOOK,
                    _UNPACK_HOOK,
                    _PACK_HOOK,
                    _POST_NOTIFY_HOOK]

        for member in dir(self):
            for prefix in prefixes:
                if member.startswith(prefix):
                    prop = member[len(prefix):]
                    if not prop in self.gskeys:
                        # no need for translation
                        raise NameError(
                            "'{}' looks like a ConfigObject hook function, but "
                            "'{}' is not a known property of '{}'"
                            .format(member, prop, str(self)))

    def disconnect_notifications(self):
        """ Recursively remove all callbacks from all notification lists. """
        for gskey in list(self.gskeys.values()):
            prop = gskey.prop
            setattr(type(self), _NOTIFY_CALLBACKS.format(prop), [])

        for child in self.children:
            child.disconnect_notifications()

    def _setup_property(self, gskey):
        """ Setup python property and notification callback """
        prop = gskey.prop

        # list of callbacks
        setattr(type(self), _NOTIFY_CALLBACKS.format(prop), [])

        # method to add callback
        def _notify_add(self, callback, _prop=prop):
            """ method to add a callback to this property """
            getattr(self, _NOTIFY_CALLBACKS.format(prop)).append(callback)
        setattr(type(self), prop+'_notify_add', _notify_add)

        # method to remove a callback
        def _notify_remove(self, callback, _prop=prop):
            """ method to remove a callback from this property """
            try:
                getattr(self, _NOTIFY_CALLBACKS.format(prop)).remove(callback)
            except ValueError:
                pass
        setattr(type(self), prop+'_notify_remove', _notify_remove)

        # gsettings callback
        def _notify_changed_cb(self, settings, key, _gskey=gskey, _prop=prop):
            """ call back function for change notification """
            # get-gsettings hook, for reading values from gsettings
            # in non-standard ways, i.e. convert data types.
            value = self.get_unpacked(_gskey)

            # Can-set hook, for value validation.
            if not hasattr(self, _CAN_SET_HOOK + _prop) or \
                   getattr(self, _CAN_SET_HOOK + _prop)(value):

                if _gskey.value != value:
                    _gskey.value = value

                    if False:
                        # asynchronous callbacks
                        def notify(callbacks, value):
                            for callback in callbacks:
                                callback(value)

                        GLib.idle_add(notify,
                                        getattr(self, _NOTIFY_CALLBACKS.format(prop)),
                                        value)
                    else:
                        for callback in getattr(self, _NOTIFY_CALLBACKS.format(prop)):
                            callback(value)

            # Post-notification hook for anything that properties
            # need to do after all listeners have been notified.
            if hasattr(self, _POST_NOTIFY_HOOK + _prop):
                getattr(self, _POST_NOTIFY_HOOK + _prop)()

        setattr(type(self), '_'+prop+'_changed_cb', _notify_changed_cb)

        # connect callback function to gsettings
        if gskey.settings:
            gskey.settings.connect("changed::"+gskey.key,
                                    getattr(self, '_'+prop+'_changed_cb'))

        # getter function
        def get_value(self, _gskey = gskey, _prop = prop):
            """ property getter """
            return _gskey.value

        # setter function
        def set_value(self, value, save = True, _gskey = gskey, _prop = prop):
            """ property setter """
            # can-set hook, for value validation
            if not hasattr(self, _CAN_SET_HOOK +_prop) or \
                   getattr(self, _CAN_SET_HOOK +_prop)(value):

                if save:
                    if value != _gskey.value or _gskey.modified:
                        self.set_unpacked(_gskey, value)
                        _gskey.modified = False

                _gskey.value = value

        # create propery
        if not hasattr(self, 'get_'+prop):   # allow overloading
            setattr(type(self), 'get_'+prop, get_value)
        if not hasattr(self, 'set_'+prop):   # allow overloading
            setattr(type(self), 'set_'+prop, set_value)
        setattr(type(self), prop,
                            property(getattr(type(self), 'get_'+prop),
                                     getattr(type(self), 'set_'+prop)))

    def init_properties(self, options):
        """ initialize the values of all properties """

        # start from hard coded defaults, then try gsettings
        self.init_from_gsettings()

        # let system defaults override gsettings
        use_system_defaults = self.use_system_defaults or \
                              self._check_hints_file()
        if use_system_defaults:
            self.init_from_system_defaults()
            self.use_system_defaults = False    # write to gsettings

        # let command line options override everything
        for gskey in list(self.gskeys.values()):
            if hasattr(options, gskey.prop):  # command line option there?
                value = getattr(options, gskey.prop)
                if not value is None:
                    gskey.value = value

        return use_system_defaults


    def _check_hints_file(self):
        """
        Use system defaults if this file exists, then delete it.
        Workaround for the difficult to access gsettings/dconf
        database when running Onboard in lightdm."
        """
        filename = "/tmp/onboard-use-system-defaults"
        if os.path.exists(filename):
            _logger.warning("Hint file '{}' exists; applying system defaults." \
                         .format(filename))
            try:
                os.remove(filename)
            except (IOError, OSError) as ex:
                errno = ex.errno
                errstr = os.strerror(errno)
                _logger.error("failed to remove hint file '{}': {} ({})" \
                            .format(filename, errstr, errno))
            return True
        return False

    def init_from_gsettings(self):
        """ init propertiy values from gsettings """

        for prop, gskey in list(self.gskeys.items()):
            gskey.value = self.get_unpacked(gskey)

        for child in self.children:
            child.init_from_gsettings()

    def init_from_system_defaults(self):
        """ fill property values with system defaults """

        self.delay()
        for prop, value in list(self.system_defaults.items()):
            setattr(self, prop, value)  # write to gsettings
        self.apply()

        for child in self.children:
            child.init_from_system_defaults()

    def on_properties_initialized(self):
        for child in self.children:
            child.on_properties_initialized()

        self._osk_dconf = None

    def migrate_dconf_tree(self, old_root, current_root):
        """ Migrate data recursively for all keys and schemas. """
        self.delay()
        for gskey in list(self.gskeys.values()):
            old_schema = old_root + self.schema[len(current_root):]
            dconf_key = "/" + old_schema.replace(".", "/") + "/" + gskey.key
            self.migrate_dconf_value(dconf_key, gskey)
        self.apply()

        for child in self.children:
           child.migrate_dconf_tree(old_root, current_root)

    def migrate_dconf_value(self, dconf_key, gskey):
        """ Copy the value of dconf_key into the given gskey """
        if not self._osk_dconf:
            self._osk_dconf = osk.DConf()
        try:
            value = self._osk_dconf.read_key(dconf_key)
        except (ValueError, TypeError) as e:
            value = None
            _logger.warning("migrate_dconf_value: {}".format(e))

        if not value is None:
            # Enums are stored as strings in dconf, convert them to int.
            if gskey.enum:
                value = gskey.enum.get(value, 0)

            # Optionally convert from gsettings value to property value.
            hook = _UNPACK_HOOK + gskey.prop
            if hasattr(self, hook):
                v = value
                value = getattr(self, hook)(value)

            _logger.info("migrate_dconf_value: {key} -> {path} {gskey}, value={value}" \
                          .format(key=dconf_key,
                                  path=self.schema,
                                  gskey=gskey.key, value=value))

            try:
                setattr(self, gskey.prop, value)
            except Exception as ex:
                _logger.error("Migrating dconf key failed: '{key}={value}'; "
                              "possibly due to incompatible dconf type; "
                              "skipping this key. "
                              "Exception: {exception}" \
                              .format(key=dconf_key, value=value,
                                      exception=unicode_str(ex)))

    def migrate_dconf_key(self, dconf_key, key):
        gskey = self.find_key(key)
        if gskey.is_default():
            self.migrate_dconf_value(dconf_key, gskey)

    def get_unpacked(self, gskey):
        """ Read from gsettings and unpack to property value. """
        # read from gsettings
        hook = _GSETTINGS_GET_HOOK + gskey.prop
        if hasattr(self, hook):
            value = getattr(self, hook)(gskey)
        else:
            value = gskey.gsettings_get()

        # Optionally convert to gsettings value to a different property value.
        # Never do this in _gsettings_get_ hooks or the migrations fail.
        hook = _UNPACK_HOOK + gskey.prop
        if hasattr(self, hook):
            v = value
            value = getattr(self, hook)(value)
        return value

    def set_unpacked(self, gskey, value):
        """ Pack property value and write to gsettings. """
        # optionally convert property value to gsettings value
        hook = _PACK_HOOK + gskey.prop
        if hasattr(self, hook):
            # pack hook, custom conversion, property -> gsettings
            value = getattr(self, hook)(value)

        # save to gsettings
        hook = _GSETTINGS_SET_HOOK + gskey.prop
        if hasattr(self, hook):
            # gsettings-set hook, custom value setter
            getattr(self, hook)(gskey, value)
        else:
            gskey.gsettings_set(value)

    def delay(self):
        self.settings.delay()

    def apply(self):
        self.settings.apply()

    @staticmethod
    def _get_user_sys_filename_gs(gskey, final_fallback, \
                            user_filename_func = None,
                            system_filename_func = None):
        """ Convenience function, takes filename from gskey. """
        return ConfigObject._get_user_sys_filename(gskey.value, gskey.key,
                                                   final_fallback,
                                                   user_filename_func,
                                                   system_filename_func)

    @staticmethod
    def _get_user_sys_filename(filename, description, \
                               final_fallback = None,
                               user_filename_func = None,
                               system_filename_func = None):
        """
        Checks a filename's validity and if necessary expands it to a
        fully qualified path pointing to either the user or system directory.
        User directory has precedence over the system one.
        """

        filepath = filename

        if filename and not ConfigObject._is_valid_filename(filename):
            # assume filename is just a basename instead of a full file path
            _logger.debug(_format("{description} '{filename}' not found yet, "
                                  "retrying in default paths", \
                                  description=description, filename=filename))

            filepath = ConfigObject._expand_user_sys_filename(
                                                     filename,
                                                     user_filename_func,
                                                     system_filename_func)
            if not filepath:
                _logger.info(_format("unable to locate '{filename}', "
                                     "loading default {description} instead",
                                     description=description,
                                     filename=filename))

        if not filepath and not final_fallback is None:
            filepath = final_fallback

        if not os.path.isfile(filepath):
            _logger.error(_format("failed to find {description} '{filename}'",
                                  description=description, filename=filename))
            filepath = ""
        else:
            _logger.debug(_format("{description} '{filepath}' found.",
                                  description=description, filepath=filepath))

        return filepath

    @staticmethod
    def _expand_user_sys_filename(filename, \
                                  user_filename_func = None,
                                  system_filename_func = None):
        result = filename
        if result and not ConfigObject._is_valid_filename(result):
            if user_filename_func:
                result = user_filename_func(filename)
                if not os.path.isfile(result):
                    result = ""

            if  not result and system_filename_func:
                result = system_filename_func(filename)
                if not os.path.isfile(result):
                    result = ""

        return result

    @staticmethod
    def _is_valid_filename(filename):
        return bool(filename) and \
               os.path.isabs(filename) and \
               os.path.isfile(filename)

    @staticmethod
    def get_unpacked_string_list(gskey, type_spec):
        """ Store dictionary in a gsettings list key """
        _list = gskey.settings.get_strv(gskey.key)
        return ConfigObject.unpack_string_list(_list, type_spec)

    @staticmethod
    def set_packed_string_list(gskey, value):
        """ Store dictionary in a gsettings list key """
        _list = ConfigObject.pack_string_list(value)
        gskey.settings.set_strv(gskey.key, _list)

    @staticmethod
    def pack_string_list(value):
        """ very crude hard coded behavior, fixme as needed """
        if type(value) == dict:
            _dict = value
            if value:
                # has collection interface?
                key, _val = list(_dict.items())[0]
                if not hasattr(_val, "__iter__"):
                    _dict = dict([key, [value]] for key, value in _dict.items())
            return ConfigObject._dict_to_list(_dict)

        assert(False) # unsupported python type

    @staticmethod
    def unpack_string_list(_list, type_spec):
        """ very crude hard coded behavior, fixme as needed """
        if type_spec == "a{ss}":
            _dict = ConfigObject._list_to_dict(_list, str, num_values = 1)
            return dict([key, value[0]] for key, value in _dict.items())

        if type_spec == "a{s[ss]}":
            return ConfigObject._list_to_dict(_list, str, num_values = 2)

        if type_spec == "a{i[ss]}":
            return ConfigObject._list_to_dict(_list, int, num_values = 2)

        assert(False) # unsupported type_spec

    @staticmethod
    def _dict_to_list(_dict):
        """ Store dictionary in a gsettings list key """
        return pack_name_value_list(_dict)

    @staticmethod
    def _list_to_dict(_list, key_type = str, num_values = 2):
        """ Get dictionary from a gsettings list key """
        if sys.version_info.major == 2:
            _list = [unicode_str(x) for x in _list]  # translate to unicode

        return unpack_name_value_list(_list, key_type=key_type,
                                             num_values = num_values)

    def load_system_defaults(self, paths):
        """
        System default settings can be optionally provided for distribution
        specific customization or branding.
        They are stored in simple ini-style files, residing in a small choice
        of directories. The last setting found in the list of paths wins.
        """
        _logger.info(_format("Looking for system defaults in {paths}",
                             paths=paths))

        filename = None
        parser = configparser.SafeConfigParser()
        try:
            if sys.version_info.major == 2:
                filename = parser.read(paths)
            else:
                filename = parser.read(paths, "UTF-8")
        except configparser.ParsingError as ex:
            _logger.error(_("Failed to read system defaults. " + \
                            unicode_str(ex)))

        if not filename:
            _logger.info(_("No system defaults found."))
        else:
            _logger.info(_format("Loading system defaults from {filename}",
                                 filename=filename))
            self._read_sysdef_section(parser)


    def _read_sysdef_section(self, parser):
        """
        Read this instances (and its childrens) system defaults section.
        """

        for child in self.children:
            child._read_sysdef_section(parser)

        self.system_defaults = {}
        if self.sysdef_section and \
           parser.has_section(self.sysdef_section):
            items = parser.items(self.sysdef_section)

            if sys.version_info.major == 2:
                items = [(key, val.decode("UTF-8")) for key, val in items]

            # convert ini file strings to property values
            sysdef_gskeys = dict((k.sysdef, k) for k in list(self.gskeys.values()))
            for sysdef, value in items:
                _logger.info(_format("Found system default '[{}] {}={}'",
                                     self.sysdef_section, sysdef, value))

                gskey = sysdef_gskeys.get(sysdef, None)
                value = self._convert_sysdef_key(gskey, sysdef, value)

                if not value is None:
                    prop = gskey.prop if gskey else sysdef.replace("-", "_")
                    self.system_defaults[prop] = value


    def _convert_sysdef_key(self, gskey, sysdef, value):
        """
        Convert a system default string to a property value.
        Sysdef strings -> values of type of gskey's default value.
        """

        if gskey is None:
            _logger.warning(_format("System defaults: Unknown key '{}' "
                                    "in section '{}'",
                                    sysdef, self.sysdef_section))
        elif gskey.enum:
            try:
                value = gskey.enum[value]
            except KeyError as ex:
                _logger.warning(_format("System defaults: Invalid enum value"
                                        " for key '{}' in section '{}':"
                                        " {}",
                                        sysdef, self.sysdef_section, ex))
        else:
            _type = type(gskey.default)
            str_type = str if sys.version_info.major >= 3 \
                       else unicode
            if _type == str_type and value[0] != '"':
                value = '"' + value + '"'
            try:
                value = literal_eval(value)
            except (ValueError, SyntaxError) as ex:
                _logger.warning(_format("System defaults: Invalid value"
                                        " for key '{}' in section '{}'"
                                        "\n  {}",
                                        sysdef, self.sysdef_section, ex))
                return None  # skip key
        return value


class GSKey:
    """
    Class for a key-value tuple for ConfigObject.
    It associates python properties with gsettings keys,
    system default keys and command line options.
    """
    def __init__(self, settings, key, default, type_string, enum,
                       prop, sysdef, writable):
        if prop is None:
            prop = key.replace("-","_")
        if sysdef is None:
            sysdef = key
        self.settings    = settings    # gsettings object
        self.key         = key         # gsettings key name
        self.sysdef      = sysdef      # system default name
        self.prop        = prop        # python property name
        self.default     = default     # hard coded default, determines type
        self.type_string = type_string # GVariant type string or None
        self.enum        = enum        # dict of enum choices {si}
        self.value       = default     # current property value
        self.writable    = writable    # If False, never write the key
                                       #    to gsettings, even on accident.
        self.modified    = False       # If True, force writing to gsettings

    def is_default(self):
        return self.value == self.default

    def gsettings_get(self):
        """ Get value from gsettings. """
        value = self.default
        try:
            # Bug in Gio, gir1.2-glib-2.0, Oneiric
            # Onboard is accumulating open file handles
            # at "/home/<user>/.config/dconf/<user>' when
            # reading from gsettings before writing.
            # Check with:
            # lsof -w -p $( pgrep gio-test ) -Fn |sort|uniq -c|sort -n|tail
            #value = self.settings[self.key]

            if self.enum:
                value = self.settings.get_enum(self.key)
            elif self.type_string:
                value = self.settings.get_value(self.key).unpack()
            else:
                _type = type(self.default)
                if _type == str:
                    value = self.settings.get_string(self.key)
                elif _type == int:
                    value = self.settings.get_int(self.key)
                elif _type == float:
                    value = self.settings.get_double(self.key)
                else:
                    value = self.settings[self.key]

        except KeyError as ex:
            _logger.error(_("Failed to get gsettings value. ") + \
                          unicode_str(ex))

        return value

    def gsettings_set(self, value):
        """ Send value to gsettings. """
        if self.writable:
            if self.enum:
                self.settings.set_enum(self.key, value)
            elif self.type_string:
                variant = GLib.Variant(self.type_string, value)
                self.settings.set_value(self.key, variant)
            else:
                self.settings[self.key] = value

