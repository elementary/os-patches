# -*- coding: utf-8 -*-

# Copyright © 2013, Gerd Kohlberger
#
# This file is part of Onboard.
#
# Onboard is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# Onboard is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

from __future__ import division, print_function, unicode_literals

from os.path        import join
from Onboard.utils  import unicode_str

import Onboard.osk as osk
import logging

_logger = logging.getLogger(__name__)


class Sound(object):
    """
    Sound singleton.
    Wrapper for osk.Audio.
    """

    """ Event id for key feedback """
    key_feedback = "onboard-key-feedback"

    def __new__(cls, *args, **kwargs):
        """
        Singleton magic.
        """
        if not hasattr(cls, "self"):
            cls.self = object.__new__(cls, *args, **kwargs)
            cls.self.construct()
        return cls.self

    def __init__(self):
        """
        Called multiple times, do not use.
        """
        pass

    def construct(self):
        self._osk_audio = None

        try:
            self._osk_audio = osk.Audio()
        except Exception as ex:
            _logger.warning("Failed to create osk.Audio: " + unicode_str(ex))

        self.enable()
        self.cache_sample(self.key_feedback)

    def is_valid(self):
        """ Check initialization of the audio backend """
        return self._osk_audio is not None

    def play(self, event_id, x, y):
        """ Play a sound """
        try:
            if self.is_valid():
                self._osk_audio.play(event_id, x, y)
        except Exception as ex:
            _logger.warning("Failed to play sound: " + unicode_str(ex))

    def cancel(self):
        """ Cancel all playing sounds """
        try:
            if self.is_valid():
                self._osk_audio.cancel()
        except Exception as ex:
            _logger.warning("Failed to cancel sound: " + unicode_str(ex))

    def set_theme(self, name):
        """ Set the XDG sound theme """
        try:
            if self.is_valid():
                self._osk_audio.set_theme(name)
        except Exception as ex:
            _logger.warning("Failed to set sound theme: " + unicode_str(ex))

    def enable(self):
        """ Enable canberra audio output """
        try:
            if self.is_valid():
                self._osk_audio.enable()
        except Exception as ex:
            _logger.warning("Failed to enable audio: " + unicode_str(ex))

    def disable(self):
        """ Disable canberra audio output """
        try:
            if self.is_valid():
                self._osk_audio.disable()
        except Exception as ex:
            _logger.warning("Failed to disable audio: " + unicode_str(ex))

    def cache_sample(self, event_id):
        """ Upload sample to the sound server. Blocking call. """
        try:
            if self.is_valid():
                self._osk_audio.cache_sample(event_id)
        except Exception as ex:
            _logger.warning("Failed to cache sample: " + unicode_str(ex))


