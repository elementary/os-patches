#  Copyright (c) 2005-2009 Canonical
#
#  Author: Michael Vogt <michael.vogt@ubuntu.com>
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
#  USA
# import the core of apt_pkg
"""High-Level Interface for working with apt."""
import apt_pkg

from apt.cache import Cache as Cache
from apt.cache import ProblemResolver as ProblemResolver

# import some fancy classes
from apt.package import Package as Package
from apt.package import Version as Version

Cache  # pyflakes
ProblemResolver  # pyflakes
Version  # pyflakes
from apt.cdrom import Cdrom as Cdrom

# init the package system, but do not re-initialize config
if "APT" not in apt_pkg.config:
    apt_pkg.init_config()
apt_pkg.init_system()

__all__ = ["Cache", "Cdrom", "Package"]
