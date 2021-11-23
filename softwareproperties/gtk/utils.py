# Copyright (C) 2009 Canonical
#
# Authors:
#  Michael Vogt
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; version 3.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

from __future__ import print_function

import aptsources.distro
from datetime import datetime
import distro_info
from functools import wraps
import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gio, Gtk
import json
import os
import subprocess

import logging
LOG=logging.getLogger(__name__)

import time

UA_STATUS_JSON = "/var/lib/ubuntu-advantage/status.json"

def setup_ui(self, path, domain):
    # setup ui
    self.builder = Gtk.Builder()
    self.builder.set_translation_domain(domain)
    self.builder.add_from_file(path)
    self.builder.connect_signals(self)
    for o in self.builder.get_objects():
        if issubclass(type(o), Gtk.Buildable):
            name = Gtk.Buildable.get_name(o)
            setattr(self, name, o)
        else:
            logging.debug("can not get name for object '%s'" % o)

def has_gnome_online_accounts():
    try:
        d = Gio.DesktopAppInfo.new('gnome-online-accounts-panel.desktop')
        return d != None
    except Exception:
        return False

def is_current_distro_lts():
    distro = aptsources.distro.get_distro()
    di = distro_info.UbuntuDistroInfo()
    return di.is_lts(distro.codename)

def is_current_distro_supported():
    distro = aptsources.distro.get_distro()
    di = distro_info.UbuntuDistroInfo()
    return distro.codename in di.supported(datetime.now().date())

def current_distro():
    distro = aptsources.distro.get_distro()
    di = distro_info.UbuntuDistroInfo()
    releases = di.get_all(result="object")
    for release in releases:
        if release.series == distro.codename:
            return release


def get_ua_status():
    """Return a dict of all UA status information or empty dict on error."""
    # status.json will exist on any attached system. It will also be created
    # by the systemd timer ua-timer which will update UA_STATUS_JSON every 12
    # hours to reflect current status of UA subscription services.
    # Invoking `ua status` with subp will result in a network call to
    # contracts.canonical.com which could raise Timeouts on network limited
    # machines. So, prefer the status.json file when possible.
    status_json = ""
    if os.path.exists(UA_STATUS_JSON):
        with open(UA_STATUS_JSON) as stream:
            status_json = stream.read()
    else:
        try:
            # Success writes UA_STATUS_JSON
            result = subprocess.run(
                ['ua', 'status', '--format=json'], stdout=subprocess.PIPE
            )
        except Exception as e:
            print("Failed to run `ua status`:\n%s" % e)
            return {}
        if result.returncode != 0:
            print(
                "Ubuntu Advantage client returned code %d" % result.returncode
            )
            return {}
        status_json = result.stdout
    if not status_json:
        print(
            "Warning: no Ubuntu Advantage status found."
            " Is ubuntu-advantage-tools installed?"
        )
        return {}
    try:
        status = json.loads(status_json)
    except json.JSONDecodeError as e:
        print("Failed to parse ubuntu advantage client JSON:\n%s" % e)
        return {}
    if status.get("_schema_version", "0.1") != "0.1":
        print(
            "UA status schema version change: %s" % status["_schema_version"]
        )
    return status


def get_ua_service_status(service_name='esm-infra', status=None):
    """Get service availability and status for a specific UA service.

    Return a tuple (available, service_status).
      :boolean available: set True when either:
        - attached contract is entitled to the service
        - unattached machine reports service "availability" as "yes"
      :str service_status: will be one of the following:
        - "disabled" when the service is available and applicable but not
          active
        - "enabled" when the service is available and active
        - "n/a" when the service is not applicable to the environment or not
          entitled for the attached contract
    """
    if not status:
        status = get_ua_status()
    # Assume unattached on empty status dict
    available = False
    service_status = "n/a"
    for service in status.get("services", []):
        if service.get("name") != service_name:
            continue
        if "available" in service:
            available = bool("yes" == service["available"])
        if "status" in service:
            service_status = service["status"]  # enabled, disabled or n/a
    return (available, service_status)


def retry(exceptions, tries=10, delay=0.1, backoff=2):
    """
    Retry calling the decorated function using an exponential backoff.

    Args:
        exceptions: The exception to check. may be a tuple of
            exceptions to check.
        tries: Number of times to try (not retry) before giving up.
        delay: Initial delay between retries in seconds.
        backoff: Backoff multiplier (e.g. value of 2 will double the delay
            each retry).
    """
    def deco_retry(f):

        @wraps(f)
        def f_retry(*args, **kwargs):
            mtries, mdelay = tries, delay
            while mtries > 1:
                try:
                    return f(*args, **kwargs)
                except exceptions as e:
                    msg = '{}, Retrying in {} seconds...'.format(e, mdelay)
                    logging.warning(msg)
                    time.sleep(mdelay)
                    mtries -= 1
                    mdelay *= backoff
            return f(*args, **kwargs)

        return f_retry  # true decorator

    return deco_retry
