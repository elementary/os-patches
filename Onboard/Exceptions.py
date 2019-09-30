# -*- coding: utf-8 -*-

from __future__ import division, print_function, unicode_literals

from traceback import format_tb
import sys
from Onboard.utils import unicode_str

class ChainableError(Exception):
    """
    Base class for Onboard errors

    We want Python to print the stacktrace of the first exception in the chain
    so we store the last stacktrace if the previous exception in the chain
    has not.
    """

    _last_exception = None

    def __init__(self, message, chained_exception = None):
        self._message = message
        self.chained_exception = chained_exception

        if chained_exception:
            if not (isinstance(chained_exception, ChainableError) \
                    and chained_exception.traceback):

                # Store last traceback
                self._last_exception = sys.exc_info()


    def _get_traceback(self):
        if self._last_exception:
            return self._last_exception[2]
        elif self.chained_exception \
                and isinstance(self.chained_exception, ChainableError):
            return self.chained_exception.traceback
        else:
            return None

    traceback = property(_get_traceback)

    def __str__(self):
        message = unicode_str(self._message)
        if self.chained_exception:
            message += ", " + unicode_str(self.chained_exception)
        return message

class SVGSyntaxError(ChainableError):
    """Error raised when Onboard can't comprehend SVG layout file."""
    pass

class LayoutFileError(ChainableError):
    """Error raised when Onboard can't comprehend layout definition file."""
    pass

class ThemeFileError(ChainableError):
    """Error raised when Onboard can't comprehend theme definition file."""
    pass

class ColorSchemeFileError(ChainableError):
    """Error raised when Onboard can't comprehend color
       scheme definition file."""
    pass

class SchemaError(ChainableError):
    """Error raised when a gesettings schema does not exist """
    pass

def chain_handler(type, value, traceback):
    """
    Wrap the default handler so that we can get the traceback from chained
    exceptions.
    """
    if isinstance(value, ChainableError) and value.traceback:
        traceback = value.traceback

    sys.__excepthook__(type, value, traceback)
