#!/usr/bin/python3
PYTHON_EXECUTABLE = "python3"

from gi.repository import GLib

def run():
    GLib.spawn_async(argv=[PYTHON_EXECUTABLE,
                           "-cfrom Onboard.settings import Settings\ns = Settings(False)"],
                     flags=GLib.SpawnFlags.SEARCH_PATH)

