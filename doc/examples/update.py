#!/usr/bin/python3
import apt
import apt_pkg
import os.path

if __name__ == "__main__":
    apt_pkg.config.set("APT::Update::Pre-Invoke::",
                       "touch /tmp/update-about-to-run")
    apt_pkg.config.set("APT::Update::Post-Invoke::",
                       "touch /tmp/update-was-run")
    c = apt.Cache()
    res = c.update(apt.progress.TextFetchProgress())
    print("res: ", res)
    assert(os.path.exists("/tmp/update-about-to-run"))
