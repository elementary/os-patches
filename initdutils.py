# Support for scanning init scripts for LSB info
from __future__ import print_function

import re, sys, os
import pickle

from io import StringIO

class RFC822Parser(dict):
    "A dictionary-like object."
    __linere = re.compile(r'([^:]+):\s*(.*)$')
    
    def __init__(self, fileob=None, strob=None, startcol=0, basedict=None):
        if fileob is None and strob is None:
            raise ValueError('need a file or string')
        if not basedict:
            basedict = {}
        
        super(RFC822Parser, self).__init__(basedict)

        if not fileob:
            fileob = StringIO(strob)

        key = None
        for line in fileob:
            if startcol:
                line = line[startcol:]

            if not line.strip():
                continue

            # Continuation line
            if line[0].isspace():
                if not key:
                    continue
                self[key] += '\n' + line.strip()
                continue

            m = self.__linere.match(line)
            if not m:
                # Not a valid RFC822 header
                continue
            key, value = m.groups()
            self[key] = value.strip()

# End of RFC882Parser

LSBLIB = '/var/lib/lsb'
FACILITIES = os.path.join(LSBLIB, 'facilities')
DEPENDS = os.path.join(LSBLIB, 'depends')
LSBINSTALL = os.path.join(LSBLIB, 'lsbinstall')

beginre = re.compile(re.escape('### BEGIN INIT INFO'))
endre = re.compile(re.escape('### END INIT INFO'))
#linere = re.compile(r'\#\s+([^:]+):\s*(.*)')

def scan_initfile(initfile):
    headerlines = ''
    scanning = False
    
    for line in open(initfile):
        line = line.rstrip()
        if beginre.match(line):
            scanning = True
            continue
        elif scanning and endre.match(line):
            scanning = False
            continue
        elif not scanning:
            continue

        if line.startswith('# '):
            headerlines += line[2:] + '\n'
        elif line.startswith('#\t'):
            headerlines += line[1:] + '\n'

    inheaders = RFC822Parser(strob=headerlines)
    headers = {}
    for header, body in inheaders.items():
        # Ignore empty headers
        if not body.strip():
            continue
        
        if header in ('Default-Start', 'Default-Stop'):
            headers[header] = list(map(int, body.split()))
        elif header in ('Required-Start', 'Required-Stop', 'Provides',
                        'Should-Start', 'Should-Stop'):
            headers[header] = body.split()
        else:
            headers[header] = body

    return headers

def save_facilities(facilities):
    if not facilities:
        try:
            os.unlink(FACILITIES)
        except OSError:
            pass
        return
    
    fh = open(FACILITIES, 'w')
    for facility, entries in facilities.items():
        # Ignore system facilities
        if facility.startswith('$'): continue
        for (scriptname, pri) in entries.items():
            start, stop = pri
            print("%(scriptname)s %(facility)s %(start)d %(stop)d" % locals(), file=fh)
    fh.close()

def load_facilities():
    facilities = {}
    if os.path.exists(FACILITIES):
        for line in open(FACILITIES):
            try:
                scriptname, name, start, stop = line.strip().split()
                facilities.setdefault(name, {})[scriptname] = (int(start),
                                                               int(stop))
            except ValueError as x:
                print('Invalid facility line', line, file=sys.stderr)

    return facilities

def load_depends():
    depends = {}

    if os.path.exists(DEPENDS):
        independs = RFC822Parser(fileob=open(DEPENDS))
        for initfile, facilities in independs.items():
            depends[initfile] = facilities.split()
    return depends

def save_depends(depends):
    if not depends:
        try:
            os.unlink(DEPENDS)
        except OSError:
            pass
        return
    
    fh = open(DEPENDS, 'w')
    for initfile, facilities in depends.items():
        print('%s: %s' % (initfile, ' '.join(facilities)), file=fh)
    fh.close()

# filemap entries are mappings, { (package, filename) : instloc }
def load_lsbinstall_info():
    if not os.path.exists(LSBINSTALL):
        return {}
    
    fh = open(LSBINSTALL, 'rb')
    filemap = cPickle.load(fh)
    fh.close()

    # Just in case it's corrupted somehow
    if not isinstance(filemap, dict):
        return {}

    return filemap

def save_lsbinstall_info(filemap):
    if not filemap:
        try:
            os.unlink(LSBINSTALL)
        except OSError:
            pass
        return
    
    fh = open(LSBINSTALL, 'wb')
    cPickle.dump(fh, filemap)
    fh.close()

if __name__ == '__main__':
    print(scan_initfile('init-fragment'))
