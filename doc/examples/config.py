#!/usr/bin/python
# Example demonstrating how to use the configuration/commandline system
# for configuration.
# Some valid command lines..
#   config.py -h --help            ; Turn on help
#   config.py -no-h --no-help --help=no  ; Turn off help
#   config.py -qqq -q=3            ; verbosity to 3
#   config.py -c /etc/apt/apt.conf ; include that config file]
#   config.py -o help=true         ; Turn on help by giving a
#                                  ; config file string
#   config.py -no-h -- -help       ; Turn off help, specify the file '-help'
# -c and -o are standard APT-program options.

# This shows how to use the system for configuration and option control.
# The other varient is for ISC object config files. See configisc.py.
import apt_pkg
import sys
import posixpath

# Create a new empty Configuration object - there is also the system global
# configuration object apt_pkg.config which is used interally by apt-pkg
# routines to control unusual situations. I recommend using the sytem global
# whenever possible..
Cnf = apt_pkg.Configuration()

print "Command line is", sys.argv

# Load the default configuration file, init_config() does this better..
Cnf.set("config-file", "/etc/apt/apt.conf")  # or Cnf["config-file"] = ".."
if posixpath.exists(Cnf.find_file("config-file")):
    apt_pkg.read_config_file(Cnf, "/etc/apt/apt.conf")

# Merge the command line arguments into the configuration space
Arguments = [('h', "help", "help"),
             ('v', "version", "version"),
             ('q', "quiet", "quiet", "IntLevel"),
             ('c', "config-file", "", "ConfigFile"),
             ('o', "option", "", "ArbItem")]
print "FileNames", apt_pkg.parse_commandline(Cnf, Arguments, sys.argv)

print "Quiet level selected is", Cnf.find_i("quiet", 0)

# Do some stuff with it
if Cnf.find_b("version", 0) == 1:
    print "Version selected - 1.1"

if Cnf.find_b("help", 0) == 1:
    print "python-apt", apt_pkg.VERSION, \
          "compiled on", apt_pkg.DATE, apt_pkg.TIME
    print "Hi, I am the help text for this program"
    sys.exit(0)

print "No help for you, try -h"

# Print the configuration space
print "The Configuration space looks like:"
for I in Cnf.keys():
    print "%s \"%s\";" % (I, Cnf[I])
