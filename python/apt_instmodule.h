// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt_instmodule.h,v 1.2 2002/01/08 06:53:04 jgg Exp $
/* ######################################################################

   Prototypes for the module

   ##################################################################### */
									/*}}}*/
#ifndef APT_INSTMODULE_H
#define APT_INSTMODULE_H

#include <Python.h>
#include "generic.h"
#include <apt-pkg/extracttar.h>


extern PyTypeObject PyArMember_Type;
extern PyTypeObject PyArArchive_Type;
extern PyTypeObject PyDebFile_Type;
extern PyTypeObject PyTarFile_Type;
extern PyTypeObject PyTarMember_Type;
extern PyTypeObject PyFileFd_Type;
struct PyTarFileObject : public CppPyObject<ExtractTar*> {
    int min;
    FileFd Fd;
};

#endif
