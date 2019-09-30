#!/usr/bin/python3

from __future__ import print_function

import os
import sys
import re
import glob
import subprocess
from os.path import dirname, abspath, join, split
from distutils.core import Extension, Command
from distutils      import version
from contextlib import contextmanager
from subprocess import getstatusoutput

# Building in pbuilder for Precise with Python 3.2 and
# python3-distutils-extra 2.34-0ubuntu0.1
# still needs this workaround, else UnicodeDecodeError.
# Skip this in python 3.3 or 'open' calls will fail later.
if sys.version_info.major == 3 and \
   sys.version_info.minor <= 2:
    import locale
    locale.getpreferredencoding = lambda *x: 'UTF-8'

try:
    import DistUtilsExtra.auto
except ImportError:
    print('To build Onboard you need https://launchpad.net/python-distutils-extra', file=sys.stderr)
    sys.exit(1)

current_ver = version.StrictVersion(DistUtilsExtra.auto.__version__)
required_ver = version.StrictVersion('2.12')
assert current_ver >= required_ver , 'needs DistUtilsExtra.auto >= 2.12'

@contextmanager
def import_path(path):
    """ temporily change python import path """
    old_path = sys.path
    sys.path = [path] + sys.path
    yield
    sys.path = old_path


def pkgconfig(*packages, **kw):
    command = "pkg-config --libs --cflags %s" % ' '.join(packages)
    status, output = getstatusoutput(command)

    # print command and ouput to console to aid in debugging
    if "sdist" in sys.argv or \
       "build" in sys.argv or \
       "build_ext" in sys.argv:
        print("setup.py: running pkg-config:", command)
        print("setup.py:", output)

    if status != 0:
        print('setup.py: pkg-config returned exit code %d' % status, file=sys.stderr)
        print('setup.py: sdist needs libgtk-3-dev, libxtst-dev, libxkbfile-dev, libdconf-dev, libcanberra-dev and libhunspell-dev')
        sys.exit(1)


    flag_map = {'-I': 'include_dirs', '-L': 'library_dirs', '-l': 'libraries'}
    for token in output.split():
        if token[:2] in flag_map:
            kw.setdefault(flag_map.get(token[:2]), []).append(token[2:])
        else:
            kw.setdefault('extra_link_args', []).append(token)
    for k, v in kw.items():
        kw[k] = list(set(v))

    return kw


def get_pkg_version(package):
    """ get major, minor version of package """
    command = "pkg-config --modversion " + package
    status, output = getstatusoutput(command)
    if status != 0:
        print("setup.py: get_pkg_version({}): "
              "pkg-config returned exit code {}" \
              .format(repr(package), status), file=sys.stderr)
        sys.exit(2)

    version = re.search('(?:(?:\d+)\.)+\d+', output).group()
    components = version.split(".")
    major, minor = int(components[0]), int(components[1])
    revision = int(components[2]) if len(components) >= 3 else 0
    return major, minor, revision


# Make xgettext extract translatable strings from _format() calls too.
var = "XGETTEXT_ARGS"
os.environ[var] = os.environ.get(var, "") + " --keyword=_format"


##### private extension 'osk' #####

MODULE_NAME_OSK = 'Onboard.osk'

class Extension_osk(Extension):
    sources = ['osk_module.c',
               'osk_virtkey.c',
               'osk_devices.c',
               'osk_util.c',
               'osk_dconf.c',
               'osk_struts.c',
               'osk_audio.c',
               'osk_hunspell.c',
               'osk_click_mapper.c',
              ]

    depends = ['osk_module.h']

    # even MINOR numbers for stable versions
    defines = [('MAJOR_VERSION', '0'),
               ('MINOR_VERSION', '4'),
               ('MICRO_VERSION', '0'),
              ]

    def __init__(self, root = ""):
        path = join(root, 'Onboard', 'osk')
        sources = [join(path, x) for x in self.sources]
        depends = [join(path, x) for x in self.depends]
        defines = self.defines

        # dconf had an API change between 0.12 and 0.13, tell osk
        major, minor, revision = get_pkg_version("dconf")
        if major == 0 and minor <= 12:
            defines.append(("DCONF_API_0", 0))
        print("found dconf version {}.{}.{}".format(major, minor, revision))

        Extension.__init__(self,
                           MODULE_NAME_OSK,

                           sources = sources,
                           depends = depends,
                           define_macros = defines,

                           **pkgconfig('gdk-3.0', 'x11', 'xi', 'xtst', 'xkbfile',
                                       'dconf', 'libcanberra', 'hunspell')
                          )

extension_osk = Extension_osk()


##### private extension lm #####

MODULE_NAME_LM = 'Onboard.pypredict.lm'

class Extension_lm(Extension):
    sources = ['lm.cpp',
               'lm_unigram.cpp',
               'lm_dynamic.cpp',
               'lm_merged.cpp',
               'lm_python.cpp',
               'pool_allocator.cpp']

    depends = ['lm.h',
               'lm_unigram.h',
               'lm_dynamic.h',
               'lm_dynamic_impl.h',
               'lm_dynamic_kn.h',
               'lm_dynamic_cached.h',
               'lm_merged.h']

    def __init__(self, root = "", module_root = ""):
        path = join(root, 'pypredict', 'lm')
        sources = [join(path, x) for x in self.sources]
        depends = [join(path, x) for x in self.depends]

        module_name = "pypredict.lm"
        if module_root:
            module_name = module_root + "." + module_name

        Extension.__init__(self,
                           module_name,
                           sources = sources,
                           depends = depends,
                           undef_macros = [],
                           library_dirs = [],
                           libraries = [],
                           define_macros=[('NDEBUG', '1')],
                          )

extension_lm = Extension_lm("Onboard", "Onboard")


#### custom test command ####

class TestCommand(Command):
    user_options = [] # required by Command

    depends = ["python3-nose",
               "hunspell",
               "hunspell-en-us",
               "hunspell-de-de",
               "myspell-es",
               "myspell-pt-pt",
               "hunspell-fr",
               "hunspell-ru",
               "myspell-it",
               "myspell-el-gr",
              ]

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        if not self.check_test_dependencies():
            sys.exit(2)

        # onboard must no be running at test begin
        subprocess.call(["killall", "onboard"])

        import nose
        if nose.run(argv=[__file__, "--with-doctest"]):
            sys.exit(0)
        else:
            sys.exit(1)

    def check_test_dependencies(self):
        status = subprocess.getstatusoutput("dpkg --version ")
        if status[0] == 0: # dpkg exists?
            status = subprocess.getstatusoutput("dpkg --status " + \
                                            " ".join(d for d in self.depends))
            if status[0]:
                for d in self.depends:
                    status = subprocess.getstatusoutput("dpkg --status " + d)
                    if status[0] != 0:
                        print("Missing test dependency '{}'. "
                              "You can install all required "
                              "test dependencies by typing:" \
                              .format(d))
                        print("sudo apt-get install " + \
                              " ".join(d for d in self.depends))
                return False

        return True

# scan for translatable layout strings in layouts
# disabled until know how to make it work.
# layoutstring.px has those string manually added now.
if 0:
    if "build_i18n" in sys.argv:
        args = ["./tools/gen_i18n_strings",
                "-o./data/layoutstrings_generated.py"]
        print("Running '{}'".format(" ".join(args)))
        subprocess.check_call(args)

##### setup #####

DistUtilsExtra.auto.setup(
    name = 'onboard',
    version = '1.0.1',
    author = 'Onboard devel team',
    author_email = 'https://launchpad.net/~onboard/+contactuser',
    url = 'http://launchpad.net/onboard/',
    license = 'gpl',
    description = 'Simple On-screen Keyboard',

    packages = ['Onboard', 'Onboard.pypredict'],

    data_files = [('share/glib-2.0/schemas', glob.glob('data/*.gschema.xml')),
                  ('share/onboard', glob.glob('AUTHORS')),
                  ('share/onboard', glob.glob('CHANGELOG')),
                  ('share/onboard', glob.glob('COPYING')),
                  ('share/onboard', glob.glob('NEWS')),
                  ('share/onboard', glob.glob('README')),
                  ('share/onboard', glob.glob('onboard-defaults.conf.example')),
                  ('share/onboard', glob.glob('onboard-defaults.conf.example.nexus7')),
                  ('share/icons/hicolor/scalable/apps', glob.glob('icons/hicolor/*')),
                  ('share/icons/HighContrast/scalable/apps', glob.glob('icons/HighContrast/*')),
                  ('share/icons/ubuntu-mono-dark/status/22', glob.glob('icons/ubuntu-mono-dark/*')),
                  ('share/icons/ubuntu-mono-light/status/22', glob.glob('icons/ubuntu-mono-light/*')),
                  ('share/sounds/freedesktop/stereo', glob.glob('sounds/*')),
                  ('share/onboard/docs', glob.glob('docs/*')),
                  ('share/onboard/layouts', glob.glob('layouts/*.*')),
                  ('share/onboard/layouts/images', glob.glob('layouts/images/*')),
                  ('share/onboard/themes', glob.glob('themes/*')),
                  ('share/onboard/scripts', glob.glob('scripts/*')),
                  ('/etc/xdg/autostart', glob.glob('data/onboard-autostart.desktop')),

                  ('share/onboard/models', glob.glob('models/*.lm')),
                  ('share/onboard/tools', glob.glob('Onboard/pypredict/tools/checkmodels')),
                 ],

    scripts = ['onboard', 'onboard-settings'],

    # don't let distutils-extra import our files
    requires = [MODULE_NAME_OSK, MODULE_NAME_LM],

    ext_modules = [extension_osk, extension_lm],

    cmdclass = {'test': TestCommand},
)

# Link the extensions back to the project directory
# so Onboard can be run from source as usual, without --inplace.
# Remove this at any time if there is a better way.
if "build" in sys.argv or \
   "build_ext" in sys.argv:
    root = dirname(abspath(__file__))
    build_root = join(root, 'build', 'lib*{}.*'.format(sys.version_info.major))
    libs = [['Onboard', 'osk*.so'],
            ['Onboard/pypredict', 'lm*.so']]
    for path, pattern in libs:
        files = glob.glob(join(build_root, path, pattern))
        for file in files:
            dstfile = join(path, split(file)[1])
            print("symlinking {} to {}".format(file, dstfile))

            try: os.unlink(dstfile)
            except OSError: pass
            os.symlink(file, dstfile)

