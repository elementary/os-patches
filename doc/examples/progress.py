#!/usr/bin/python

import sys
import time

import apt_pkg
import apt
import apt.progress.base


class TextProgress(apt.progress.base.OpProgress):

    def __init__(self):
        self.last = 0.0

    def update(self, percent):
        if (self.last + 1.0) <= percent:
            sys.stdout.write("\rProgress: %i.2          " % (percent))
            self.last = percent
        if percent >= 100:
            self.last = 0.0

    def done(self):
        self.last = 0.0
        print("\rDone                      ")


class TextFetchProgress(apt.progress.base.AcquireProgress):

    def __init__(self):
        pass

    def start(self):
        pass

    def stop(self):
        pass

    def fail(self, item):
        print('fail', item)

    def fetch(self, item):
        print('fetch', item)

    def ims_hit(self, item):
        print('ims_hit', item)

    def pulse(self, owner):
        print("pulse: CPS: %s/s; Bytes: %s/%s; Item: %s/%s" % (
            apt_pkg.size_to_str(self.current_cps),
            apt_pkg.size_to_str(self.current_bytes),
            apt_pkg.size_to_str(self.total_bytes),
            self.current_items,
            self.total_items))
        return True

    def media_change(self, medium, drive):
        print("Please insert medium %s in drive %s" % (medium, drive))
        sys.stdin.readline()
        #return False


class TextInstallProgress(apt.progress.base.InstallProgress):

    def __init__(self):
        apt.progress.base.InstallProgress.__init__(self)
        pass

    def start_update(self):
        print("start_update")

    def finish_update(self):
        print("finish_update")

    def status_change(self, pkg, percent, status):
        print("[%s] %s: %s" % (percent, pkg, status))

    def update_interface(self):
        apt.progress.base.InstallProgress.update_interface(self)
        # usefull to e.g. redraw a GUI
        time.sleep(0.1)


class TextCdromProgress(apt.progress.base.CdromProgress):

    def __init__(self):
        pass

    # update is called regularly so that the gui can be redrawn

    def update(self, text, step):
        # check if we actually have some text to display
        if text != "":
            print("Update: %s %s" % (text.strip(), step))

    def ask_cdrom_name(self):
        sys.stdout.write("Please enter cd-name: ")
        cd_name = sys.stdin.readline()
        return (True, cd_name.strip())

    def change_cdrom(self):
        print("Please insert cdrom and press <ENTER>")
        answer = sys.stdin.readline()
        print(answer)
        return True


if __name__ == "__main__":
    c = apt.Cache()
    pkg = c["3dchess"]
    if pkg.is_installed:
        pkg.mark_delete()
    else:
        pkg.mark_install()

    res = c.commit(TextFetchProgress(), TextInstallProgress())

    print(res)
