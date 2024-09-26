// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: tag.cc,v 1.3 2002/02/26 01:36:15 mdz Exp $
/* ######################################################################

   Tag - Binding for the RFC 822 tag file parser

   Upon reflection I have make the TagSection wrapper look like a map..
   The other option was to use a sequence (which nicely matches the internal
   storage) but really makes no sense to a Python Programmer.. One
   specialized lookup is provided, the FindFlag lookup - as well as the
   usual set of duplicate things to match the C++ interface.

   The TagFile interface is also slightly different, it has a built in
   internal TagSection object that is used. Do not hold onto a reference
   to a TagSection and let TagFile go out of scope! The underlying storage
   for the section will go away and it will seg.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "generic.h"
#include "apt_pkgmodule.h"

#include <apt-pkg/tagfile.h>
#include <apt-pkg/fileutl.h>

#include <stdio.h>
#include <iostream>
#include <Python.h>

using namespace std;
									/*}}}*/
/* We need to keep a private copy of the data.. */
struct TagSecData : public CppPyObject<pkgTagSection>
{
   char *Data;
   bool Bytes;
#if PY_MAJOR_VERSION >= 3
   PyObject *Encoding;
#endif
};

// The owner of the TagFile is a Python file object.
struct TagFileData : public CppPyObject<pkgTagFile>
{
   TagSecData *Section;
   FileFd Fd;
   bool Bytes;
#if PY_MAJOR_VERSION >= 3
   PyObject *Encoding;
#endif
};

// Traversal and Clean for owned objects
int TagFileTraverse(PyObject *self, visitproc visit, void* arg) {
    Py_VISIT(((TagFileData *)self)->Section);
    Py_VISIT(((TagFileData *)self)->Owner);
    return 0;
}

int TagFileClear(PyObject *self) {
    Py_CLEAR(((TagFileData *)self)->Section);
    Py_CLEAR(((TagFileData *)self)->Owner);
    return 0;
}

// Helpers to return Unicode or bytes as appropriate.
#if PY_MAJOR_VERSION < 3
#define TagSecString_FromStringAndSize(self, v, len) \
    PyString_FromStringAndSize((v), (len))
#define TagSecString_FromString(self, v) CppPyString(v)
#else
static PyObject *TagSecString_FromStringAndSize(PyObject *self, const char *v,
	 				 Py_ssize_t len) {
   TagSecData *Self = (TagSecData *)self;
   if (Self->Bytes)
      return PyBytes_FromStringAndSize(v, len);
   else if (Self->Encoding)
      return PyUnicode_Decode(v, len, PyUnicode_AsString(Self->Encoding), 0);
   else
      return PyUnicode_FromStringAndSize(v, len);
}

static PyObject *TagSecString_FromString(PyObject *self, const char *v) {
   TagSecData *Self = (TagSecData *)self;
   if (Self->Bytes)
      return PyBytes_FromString(v);
   else if (Self->Encoding)
      return PyUnicode_Decode(v, strlen(v),
			      PyUnicode_AsString(Self->Encoding), 0);
   else
      return PyUnicode_FromString(v);
}
#endif


									/*}}}*/
// TagSecFree - Free a Tag Section					/*{{{*/
// ---------------------------------------------------------------------
/* */
void TagSecFree(PyObject *Obj)
{
   TagSecData *Self = (TagSecData *)Obj;
   delete [] Self->Data;
   CppDealloc<pkgTagSection>(Obj);
}
									/*}}}*/
// TagFileFree - Free a Tag File					/*{{{*/
// ---------------------------------------------------------------------
/* */
void TagFileFree(PyObject *Obj)
{
   #ifdef ALLOC_DEBUG
   std::cerr << "=== DEALLOCATING " << Obj->ob_type->tp_name << "^ ===\n";
   #endif
   PyObject_GC_UnTrack(Obj);
   TagFileData *Self = (TagFileData *)Obj;
   Py_CLEAR(Self->Section);
   Self->Object.~pkgTagFile();
   Self->Fd.~FileFd();
   Py_CLEAR(Self->Owner);
   Obj->ob_type->tp_free(Obj);
}
									/*}}}*/

// Tag Section Wrappers							/*{{{*/
static char *doc_Find =
    "find(name: str[, default = None]) -> str\n\n"
    "Find the key given by 'name' and return the value. If the key can\n"
    "not be found, return 'default'.";
static PyObject *TagSecFind(PyObject *Self,PyObject *Args)
{
   char *Name = 0;
   char *Default = 0;
   if (PyArg_ParseTuple(Args,"s|z",&Name,&Default) == 0)
      return 0;

   const char *Start;
   const char *Stop;
   if (GetCpp<pkgTagSection>(Self).Find(Name,Start,Stop) == false)
   {
      if (Default == 0)
	 Py_RETURN_NONE;
      return TagSecString_FromString(Self,Default);
   }
   return TagSecString_FromStringAndSize(Self,Start,Stop-Start);
}

static char *doc_FindRaw =
    "find_raw(name: str[, default = None] -> str\n\n"
    "Same as find(), but returns the complete 'key: value' field; instead of\n"
    "just the value.";
static PyObject *TagSecFindRaw(PyObject *Self,PyObject *Args)
{
   char *Name = 0;
   char *Default = 0;
   if (PyArg_ParseTuple(Args,"s|z",&Name,&Default) == 0)
      return 0;

   unsigned Pos;
   if (GetCpp<pkgTagSection>(Self).Find(Name,Pos) == false)
   {
      if (Default == 0)
	 Py_RETURN_NONE;
      return TagSecString_FromString(Self,Default);
   }

   const char *Start;
   const char *Stop;
   GetCpp<pkgTagSection>(Self).Get(Start,Stop,Pos);

   return TagSecString_FromStringAndSize(Self,Start,Stop-Start);
}

static char *doc_FindFlag =
    "find_flag(name: str) -> int\n\n"
    "Return 1 if the key's value is 'yes' or a similar value describing\n"
    "a boolean true. If the field does not exist, or does not have such a\n"
    "value, return 0.";
static PyObject *TagSecFindFlag(PyObject *Self,PyObject *Args)
{
   char *Name = 0;
   if (PyArg_ParseTuple(Args,"s",&Name) == 0)
      return 0;

   unsigned long Flag = 0;
   if (GetCpp<pkgTagSection>(Self).FindFlag(Name,Flag,1) == false)
   {
      Py_INCREF(Py_None);
      return Py_None;
   }
   return PyBool_FromLong(Flag);
}

static char *doc_Write =
    "write(file: int, order: List[str], rewrite: List[Tag]) -> None\n\n"
    "Rewrites the section into the given file descriptor, which should be\n"
    "a file descriptor or an object providing a fileno() method.\n"
    "\n"
    ".. versionadded:: 1.9.0";
static PyObject *TagSecWrite(PyObject *Self, PyObject *Args, PyObject *kwds)
{
   char *kwlist[] = {"file", "order", "rewrite", nullptr};
   PyObject *pFile;
   PyObject *pOrder;
   PyObject *pRewrite;
   if (PyArg_ParseTupleAndKeywords(Args,kwds, "OO!O!",kwlist, &pFile,&PyList_Type,&pOrder, &PyList_Type, &pRewrite) == 0)
      return nullptr;

   int fileno = PyObject_AsFileDescriptor(pFile);
   // handle invalid arguments
   if (fileno == -1)
   {
      PyErr_SetString(PyExc_TypeError,
                      "Argument must be string, fd or have a fileno() method");
      return 0;
   }

   FileFd file(fileno);
   const char **order = ListToCharChar(pOrder,true);
   if (order == nullptr)
      return nullptr;
   std::vector<pkgTagSection::Tag> rewrite;
   for (int I = 0; I != PySequence_Length(pRewrite); I++) {
      PyObject *item = PySequence_GetItem(pRewrite, I);
      if (!PyObject_TypeCheck(item, &PyTag_Type))
         return PyErr_SetString(PyExc_TypeError, "Wrong type for tag in list"), nullptr;
      rewrite.push_back(GetCpp<pkgTagSection::Tag>(item));
   }

   return HandleErrors(PyBool_FromLong(GetCpp<pkgTagSection>(Self).Write(file, order, rewrite)));
}


// Map access, operator []
static PyObject *TagSecMap(PyObject *Self,PyObject *Arg)
{
   const char *Name = PyObject_AsString(Arg);
   if (Name == 0)
      return 0;
   const char *Start;
   const char *Stop;
   if (GetCpp<pkgTagSection>(Self).Find(Name,Start,Stop) == false)
   {
      PyErr_SetString(PyExc_KeyError,Name);
      return 0;
   }

   return TagSecString_FromStringAndSize(Self,Start,Stop-Start);
}

// len() operation
static Py_ssize_t TagSecLength(PyObject *Self)
{
   pkgTagSection &Sec = GetCpp<pkgTagSection>(Self);
   return Sec.Count();
}

// Look like a mapping
static char *doc_Keys =
    "keys() -> list\n\n"
    "Return a list of all keys.";
static PyObject *TagSecKeys(PyObject *Self,PyObject *Args)
{
   pkgTagSection &Tags = GetCpp<pkgTagSection>(Self);
   if (PyArg_ParseTuple(Args,"") == 0)
      return 0;

   // Convert the whole configuration space into a list
   PyObject *List = PyList_New(0);
   for (unsigned int I = 0; I != Tags.Count(); I++)
   {
      const char *Start;
      const char *Stop;
      Tags.Get(Start,Stop,I);
      const char *End = Start;
      for (; End < Stop && *End != ':'; End++);

      PyObject *Obj;
      PyList_Append(List,Obj = PyString_FromStringAndSize(Start,End-Start));
      Py_DECREF(Obj);
   }
   return List;
}

#if PY_MAJOR_VERSION < 3
static char *doc_Exists =
    "has_key(name: str) -> bool\n\n"
    "Return True if the key given by 'name' exists, False otherwise.";
static PyObject *TagSecExists(PyObject *Self,PyObject *Args)
{
   char *Name = 0;
   if (PyArg_ParseTuple(Args,"s",&Name) == 0)
      return 0;

   const char *Start;
   const char *Stop;
   return PyBool_FromLong(GetCpp<pkgTagSection>(Self).Find(Name,Start,Stop));
}
#endif

static int TagSecContains(PyObject *Self,PyObject *Arg)
{
   const char *Name = PyObject_AsString(Arg);
   if (Name == 0)
      return 0;
   const char *Start;
   const char *Stop;
   if (GetCpp<pkgTagSection>(Self).Find(Name,Start,Stop) == false)
      return 0;
   return 1;
}

static char *doc_Bytes =
    "bytes() -> int\n\n"
    "Return the number of bytes this section is large.";
static PyObject *TagSecBytes(PyObject *Self,PyObject *Args)
{
   if (PyArg_ParseTuple(Args,"") == 0)
      return 0;

   return MkPyNumber(GetCpp<pkgTagSection>(Self).size());
}

static PyObject *TagSecStr(PyObject *Self)
{
   const char *Start;
   const char *Stop;
   GetCpp<pkgTagSection>(Self).GetSection(Start,Stop);
   return TagSecString_FromStringAndSize(Self,Start,Stop-Start);
}
									/*}}}*/
// TagFile Wrappers							/*{{{*/
static char *doc_Step =
    "step() -> bool\n\n"
    "Step forward in the file";
static PyObject *TagFileStep(PyObject *Self,PyObject *Args)
{
   if (PyArg_ParseTuple(Args,"") == 0)
      return 0;

   TagFileData &Obj = *(TagFileData *)Self;
   if (Obj.Object.Step(Obj.Section->Object) == false)
      return HandleErrors(PyBool_FromLong(0));

   return HandleErrors(PyBool_FromLong(1));
}

// TagFile Wrappers							/*{{{*/
static PyObject *TagFileNext(PyObject *Self)
{
   TagFileData &Obj = *(TagFileData *)Self;
   // Replace the section.
   Py_CLEAR(Obj.Section);
   Obj.Section = (TagSecData*)(&PyTagSection_Type)->tp_alloc(&PyTagSection_Type, 0);
   new (&Obj.Section->Object) pkgTagSection();
   Obj.Section->Owner = Self;
   Py_INCREF(Obj.Section->Owner);
   Obj.Section->Data = 0;
   Obj.Section->Bytes = Obj.Bytes;
#if PY_MAJOR_VERSION >= 3
   // We don't need to incref Encoding as the previous Section object already
   // held a reference to it.
   Obj.Section->Encoding = Obj.Encoding;
#endif
   if (Obj.Object.Step(Obj.Section->Object) == false)
      return HandleErrors(NULL);

   // Bug-Debian: http://bugs.debian.org/572596
   // Duplicate the data here and scan the duplicated section data; in order
   // to not use any shared storage.
   // TODO: Provide an API in apt-pkg to do this; this is really ugly.

   // Fetch old section data
   const char *Start;
   const char *Stop;
   Obj.Section->Object.GetSection(Start,Stop);
   // Duplicate the data and
   //  append a \n because GetSection() will only give us a single \n
   //  but Scan() needs \n\n to work
   Obj.Section->Data = new char[Stop-Start+2];

   memcpy(Obj.Section->Data, Start, Stop - Start);
   Obj.Section->Data[Stop-Start] = '\n';
   Obj.Section->Data[Stop-Start+1] = '\0';
   // Rescan it
   if(Obj.Section->Object.Scan(Obj.Section->Data, Stop-Start+2) == false)
      return HandleErrors(NULL);

   Py_INCREF(Obj.Section);
   return HandleErrors(Obj.Section);
}

static PyObject *TagFileIter(PyObject *Self) {
    Py_INCREF(Self);
    return Self;
}

static char *doc_Offset =
    "offset() -> int\n\n"
    "Return the current offset.";
static PyObject *TagFileOffset(PyObject *Self,PyObject *Args)
{
   if (PyArg_ParseTuple(Args,"") == 0)
      return 0;
   return MkPyNumber(((TagFileData *)Self)->Object.Offset());
   
}

static char *doc_Jump =
    "jump(offset: int) -> bool\n\n"
    "Jump to the given offset; return True on success. Note that jumping to\n"
    "an offset is not very reliable, and the 'section' attribute may point\n"
    "to an unexpected section.";
static PyObject *TagFileJump(PyObject *Self,PyObject *Args)
{
   int Offset;
   if (PyArg_ParseTuple(Args,"i",&Offset) == 0)
      return 0;

   TagFileData &Obj = *(TagFileData *)Self;
   if (Obj.Object.Jump(Obj.Section->Object,Offset) == false)
      return HandleErrors(PyBool_FromLong(0));

   return HandleErrors(PyBool_FromLong(1));
}

static char *doc_Close =
    "close()\n\n"
    "Close the file.";
static PyObject *TagFileClose(PyObject *self, PyObject *args)
{
    if (args != NULL && !PyArg_ParseTuple(args, ""))
        return NULL;

   TagFileData &Obj = *(TagFileData *) self;

   Obj.Fd.Close();

   Py_INCREF(Py_None);
   return HandleErrors(Py_None);
}

static PyObject *TagFileExit(PyObject *self, PyObject *args)
{

   PyObject *exc_type = 0;
   PyObject *exc_value = 0;
   PyObject *traceback = 0;
   if (!PyArg_UnpackTuple(args, "__exit__", 3, 3, &exc_type, &exc_value,
                          &traceback)) {
       return 0;
   }

   PyObject *res = TagFileClose(self, NULL);

   if (res == NULL) {
      // The close failed. If no exception happened within the suite, we
      // will raise an error here. Otherwise, we just display the error, so
      // Python can handle the original exception instead.
      if (exc_type == Py_None)
         return NULL;

      PyErr_WriteUnraisable(self);
   } else {
      Py_DECREF(res);
   }
   // Return False, as required by the context manager protocol.
   Py_RETURN_FALSE;
}

static PyObject *TagFileEnter(PyObject *self, PyObject *args)
{
   if (!PyArg_ParseTuple(args, ""))
      return NULL;

   Py_INCREF(self);

   return self;
}

									/*}}}*/
// ParseSection - Parse a single section from a tag file		/*{{{*/
// ---------------------------------------------------------------------
static PyObject *TagSecNew(PyTypeObject *type,PyObject *Args,PyObject *kwds) {
   char *Data;
   Py_ssize_t Len;
   char Bytes = 0;
   char *kwlist[] = {"text", "bytes", 0};

   // this allows reading "byte" types from python3 - but we don't
   // make (much) use of it yet
   if (PyArg_ParseTupleAndKeywords(Args,kwds,"s#|b",kwlist,&Data,&Len,&Bytes) == 0)
      return 0;
   if (memchr(Data, 0, Len) != nullptr) {
      PyErr_SetString(PyExc_ValueError, "Input contains NUL byte");
      return 0;
   }
   if (Data[Len] != 0) {
      PyErr_SetString(PyExc_ValueError, "Input is not terminated by NUL byte");
      return 0;
   }

   // Create the object..
   TagSecData *New = (TagSecData*)type->tp_alloc(type, 0);
   new (&New->Object) pkgTagSection();
   New->Data = new char[strlen(Data)+2];
   snprintf(New->Data,strlen(Data)+2,"%s\n",Data);
   New->Bytes = Bytes;
#if PY_MAJOR_VERSION >= 3
   New->Encoding = 0;
#endif

   if (New->Object.Scan(New->Data,strlen(New->Data)) == false)
   {
      cerr << New->Data << endl;
      Py_DECREF((PyObject *)New);
      PyErr_SetString(PyExc_ValueError,"Unable to parse section data");
      return 0;
   }

   New->Object.Trim();

   return New;
}

									/*}}}*/
// ParseTagFile - Parse a tagd file					/*{{{*/
// ---------------------------------------------------------------------
/* This constructs the parser state. */

static PyObject *TagFileNew(PyTypeObject *type,PyObject *Args,PyObject *kwds)
{
   PyObject *File = 0;
   char Bytes = 0;

   char *kwlist[] = {"file", "bytes", 0};
   if (PyArg_ParseTupleAndKeywords(Args,kwds,"O|b",kwlist,&File,&Bytes) == 0)
      return 0;

   // check if we got a filename or a file object
   int fileno = -1;
   PyApt_Filename filename;
   if (!filename.init(File)) {
      PyErr_Clear();
      fileno = PyObject_AsFileDescriptor(File);
   }

   // handle invalid arguments
   if (fileno == -1 && filename == NULL)
   {
      PyErr_SetString(PyExc_TypeError, 
                      "Argument must be string, fd or have a fileno() method");
      return 0;
   }

   PyApt_UniqueObject<TagFileData> New((TagFileData*)type->tp_alloc(type, 0));
   if (fileno != -1)
   {
#ifdef APT_HAS_GZIP
      new (&New->Fd) FileFd();
      New->Fd.OpenDescriptor(fileno, FileFd::ReadOnlyGzip, false);
#else
      new (&New->Fd) FileFd(fileno,false);
#endif
   }
   else 
   {
      // FileFd::Extension got added in this revision
#if (APT_PKG_MAJOR > 4 || (APT_PKG_MAJOR >= 4 && APT_PKG_MINOR >= 12))
      new (&New->Fd) FileFd(filename, FileFd::ReadOnly, FileFd::Extension, false);
#else
      new (&New->Fd) FileFd(filename, FileFd::ReadOnly, false);
#endif
   } 
   New->Bytes = Bytes;
   New->Owner = File;
   Py_INCREF(New->Owner);
#if PY_MAJOR_VERSION >= 3
   if (fileno != -1) {
      New->Encoding = PyObject_GetAttr(File, PyUnicode_FromString("encoding"));
      if (!New->Encoding)
         PyErr_Clear();
      if (New->Encoding && !PyUnicode_Check(New->Encoding))
         New->Encoding = 0;
   } else
      New->Encoding = 0;
   Py_XINCREF(New->Encoding);
#endif
   new (&New->Object) pkgTagFile(&New->Fd);

   // Create the section
   New->Section = (TagSecData*)(&PyTagSection_Type)->tp_alloc(&PyTagSection_Type, 0);
   new (&New->Section->Object) pkgTagSection();
   New->Section->Owner = New.get();
   Py_INCREF(New->Section->Owner);
   New->Section->Data = 0;
   New->Section->Bytes = Bytes;
#if PY_MAJOR_VERSION >= 3
   New->Section->Encoding = New->Encoding;
   Py_XINCREF(New->Section->Encoding);
#endif

   return HandleErrors(New.release());
}
									/*}}}*/

// Method table for the Tag Section object
static PyMethodDef TagSecMethods[] =
{
   // Query
   {"find",TagSecFind,METH_VARARGS,doc_Find},
   {"find_raw",TagSecFindRaw,METH_VARARGS,doc_FindRaw},
   {"find_flag",TagSecFindFlag,METH_VARARGS,doc_FindFlag},
   {"bytes",TagSecBytes,METH_VARARGS,doc_Bytes},
   {"write",(PyCFunction) TagSecWrite,METH_VARARGS|METH_KEYWORDS,doc_Write},

   // Python Special
   {"keys",TagSecKeys,METH_VARARGS,doc_Keys},
#if PY_MAJOR_VERSION < 3
   {"has_key",TagSecExists,METH_VARARGS,doc_Exists},
#endif
   {"get",TagSecFind,METH_VARARGS,doc_Find},
   {}
};


PySequenceMethods TagSecSeqMeth = {0,0,0,0,0,0,0,TagSecContains,0,0};
PyMappingMethods TagSecMapMeth = {TagSecLength,TagSecMap,0};


static char *doc_TagSec = "TagSection(text: str, [bytes: bool = False])\n\n"
   "Provide methods to access RFC822-style header sections, like those\n"
   "found in debian/control or Packages files.\n\n"
   "TagSection() behave like read-only dictionaries and also provide access\n"
   "to the functions provided by the C++ class (e.g. find).\n\n"
   "By default, text read from files is treated as strings (binary data in\n"
   "Python 2, Unicode strings in Python 3). Use bytes=True to cause all\n"
   "header values read from this TagSection to be bytes even in Python 3.\n"
   "Header names are always treated as Unicode.";
PyTypeObject PyTagSection_Type =
{
   PyVarObject_HEAD_INIT(&PyType_Type, 0)
   "apt_pkg.TagSection",                // tp_name
   sizeof(TagSecData),                  // tp_basicsize
   0,                                   // tp_itemsize
   // Methods
   TagSecFree,                          // tp_dealloc
   0,                                   // tp_print
   0,                                   // tp_getattr
   0,                                   // tp_setattr
   0,                                   // tp_compare
   0,                                   // tp_repr
   0,                                   // tp_as_number
   &TagSecSeqMeth,                      // tp_as_sequence
   &TagSecMapMeth,                      // tp_as_mapping
   0,                                   // tp_hash
   0,                                   // tp_call
   TagSecStr,                           // tp_str
   _PyAptObject_getattro,               // tp_getattro
   0,                                   // tp_setattro
   0,                                   // tp_as_buffer
   (Py_TPFLAGS_DEFAULT |                // tp_flags
    Py_TPFLAGS_BASETYPE |
    Py_TPFLAGS_HAVE_GC),
   doc_TagSec,                          // tp_doc
   CppTraverse<pkgTagSection>,     // tp_traverse
   CppClear<pkgTagSection>,         // tp_clear
   0,                                   // tp_richcompare
   0,                                   // tp_weaklistoffset
   0,                                   // tp_iter
   0,                                   // tp_iternext
   TagSecMethods,                       // tp_methods
   0,                                   // tp_members
   0,                                   // tp_getset
   0,                                   // tp_base
   0,                                   // tp_dict
   0,                                   // tp_descr_get
   0,                                   // tp_descr_set
   0,                                   // tp_dictoffset
   0,                                   // tp_init
   0,                                   // tp_alloc
   TagSecNew,                           // tp_new
};

// Method table for the Tag File object
static PyMethodDef TagFileMethods[] =
{
   // Query
   {"step",TagFileStep,METH_VARARGS,doc_Step},
   {"offset",TagFileOffset,METH_VARARGS,doc_Offset},
   {"jump",TagFileJump,METH_VARARGS,doc_Jump},
   {"close",TagFileClose,METH_VARARGS,doc_Close},
   {"__enter__",TagFileEnter,METH_VARARGS,"Context manager entry, return self."},
   {"__exit__",TagFileExit,METH_VARARGS,"Context manager exit, calls close."},

   {}
};

// Return the current section.
static PyObject *TagFileGetSection(PyObject *Self,void*) {
   PyObject *Obj = ((TagFileData *)Self)->Section;
   Py_INCREF(Obj);
   return Obj;
}

static PyGetSetDef TagFileGetSet[] = {
    {"section",TagFileGetSection,0,
     "The current section, as a TagSection object.",0},
    {}
};


static char *doc_TagFile = "TagFile(file, [bytes: bool = False])\n\n"
   "TagFile() objects provide access to debian control files, which consist\n"
   "of multiple RFC822-style sections.\n\n"
   "To provide access to those sections, TagFile objects provide an iterator\n"
   "which yields TagSection objects for each section.\n\n"
   "TagFile objects also provide another API which uses a shared TagSection\n"
   "object in the 'section' member. The functions step() and jump() can be\n"
   "used to navigate within the file; offset() returns the current\n"
   "position.\n\n"
   "It is important to not mix the use of both APIs, because this can have\n"
   "unwanted effects.\n\n"
   "The parameter 'file' refers to an object providing a fileno() method or\n"
   "a file descriptor (an integer).\n\n"
   "By default, text read from files is treated as strings (binary data in\n"
   "Python 2, Unicode strings in Python 3). Use bytes=True to cause all\n"
   "header values read from this TagFile to be bytes even in Python 3.\n"
   "Header names are always treated as Unicode.";

// Type for a Tag File
PyTypeObject PyTagFile_Type =
{
   PyVarObject_HEAD_INIT(&PyType_Type, 0)
   "apt_pkg.TagFile",                   // tp_name
   sizeof(TagFileData),                 // tp_basicsize
   0,                                   // tp_itemsize
   // Methods
   TagFileFree,                         // tp_dealloc
   0,                                   // tp_print
   0,                                   // tp_getattr
   0,                                   // tp_setattr
   0,                                   // tp_compare
   0,                                   // tp_repr
   0,                                   // tp_as_number
   0,                                   // tp_as_sequence
   0,                                   // tp_as_mapping
   0,                                   // tp_hash
   0,                                   // tp_call
   0,                                   // tp_str
   _PyAptObject_getattro,               // tp_getattro
   0,                                   // tp_setattro
   0,                                   // tp_as_buffer
   (Py_TPFLAGS_DEFAULT |                // tp_flags
    Py_TPFLAGS_BASETYPE |
    Py_TPFLAGS_HAVE_GC),
   doc_TagFile,                         // tp_doc
   TagFileTraverse,                     // tp_traverse
   TagFileClear,                        // tp_clear
   0,                                   // tp_richcompare
   0,                                   // tp_weaklistoffset
   TagFileIter,                         // tp_iter
   TagFileNext,                         // tp_iternext
   TagFileMethods,                      // tp_methods
   0,                                   // tp_members
   TagFileGetSet,                       // tp_getset
   0,                                   // tp_base
   0,                                   // tp_dict
   0,                                   // tp_descr_get
   0,                                   // tp_descr_set
   0,                                   // tp_dictoffset
   0,                                   // tp_init
   0,                                   // tp_alloc
   TagFileNew,                          // tp_new

};


// Return the current section.
static PyObject *TagGetAction(PyObject *Self,void*) {
   return MkPyNumber(GetCpp<pkgTagSection::Tag>(Self).Action);
}

static PyObject *TagGetName(PyObject *Self,void*) {
   return CppPyString(GetCpp<pkgTagSection::Tag>(Self).Name);
}

static PyObject *TagGetData(PyObject *Self,void*) {
   return CppPyString(GetCpp<pkgTagSection::Tag>(Self).Data);
}

static PyObject *PyTagRename_New(PyTypeObject *type,PyObject *Args,PyObject *kwds) {
   char *oldName;
   char *newName;
   char *kwlist[] = {"old_name", "new_name", 0};

   if (PyArg_ParseTupleAndKeywords(Args,kwds,"ss",kwlist, &oldName, &newName) == 0)
      return nullptr;
   if (oldName[0] == '\0') {
      PyErr_SetString(PyExc_ValueError, "Old tag name may not be empty.");
      return nullptr;
   }
   if (newName[0] == '\0') {
      PyErr_SetString(PyExc_ValueError, "New tag name may not be empty.");
      return nullptr;
   }

   auto tag = pkgTagSection::Tag::Rename(oldName, newName);
   return CppPyObject_NEW<pkgTagSection::Tag>(nullptr, type, tag);
}

static PyObject *PyTagRewrite_New(PyTypeObject *type,PyObject *Args,PyObject *kwds) {
   char *name;
   char *data;
   char *kwlist[] = {"name", "data", 0};

   if (PyArg_ParseTupleAndKeywords(Args,kwds,"ss",kwlist, &name, &data) == 0)
      return nullptr;
   if (name[0] == '\0') {
      PyErr_SetString(PyExc_ValueError, "Tag name may not be empty.");
      return nullptr;
   }
   if (data[0] == '\0') {
      PyErr_SetString(PyExc_ValueError, "New value may not be empty.");
      return nullptr;
   }

   auto tag = pkgTagSection::Tag::Rewrite(name, data);
   return CppPyObject_NEW<pkgTagSection::Tag>(nullptr, type, tag);
}

static PyObject *PyTagRemove_New(PyTypeObject *type,PyObject *Args,PyObject *kwds) {
   char *name;
   char *kwlist[] = {"name", nullptr};

   if (PyArg_ParseTupleAndKeywords(Args,kwds,"s",kwlist, &name) == 0)
      return nullptr;
   if (name[0] == '\0') {
      PyErr_SetString(PyExc_ValueError, "Tag name may not be empty.");
      return nullptr;
   }

   auto tag = pkgTagSection::Tag::Remove(name);
   return CppPyObject_NEW<pkgTagSection::Tag>(nullptr, type, tag);
}

static PyGetSetDef TagGetSet[] = {
    {"action",TagGetAction,0,
     "The action to perform.",0},
    {"name",TagGetName,0,
     "The name of the tag to perform the action on.",0},
    {"data",TagGetData,0,
     "The data to write instead (for REWRITE), or the new tag name (RENAME)",0},
    {}
};

static char doc_Tag[] = "Tag\n\n"
   "Identify actions to be executed on a task\n"
   "\n"
   "This is used in conjunction with :meth:`TagSection.write` to rewrite\n"
   "a tag section into a new one.\n"
   "\n"
   "This class is abstract, use one of the subclasses:\n"
   ":class:`TagRewrite`, :class:`TagRemove`, :class:`TagRename`\n"
   "\n"
   ".. versionadded:: 1.1";

static char doc_TagRewrite[] = "TagRewrite(name: str, data: str)\n\n"
   "Change the value of the tag to the string passed in *data*\n"
   "\n"
   ".. versionadded:: 1.1";
static char doc_TagRename[] = "TagRename(old_name: str, new_name: str)\n\n"
   "Rename the tag *old_name* to *new_name*\n"
   "\n"
   ".. versionadded:: 1.1";

static char doc_TagRemove[] = "TagRemove(name: str)\n\n"
   "Remove the tag *name* from the tag section\n"
   "\n"
   ".. versionadded:: 1.1";


// Type for a Tag File
PyTypeObject PyTag_Type =
{
   PyVarObject_HEAD_INIT(&PyType_Type, 0)
   "apt_pkg.Tag",                   // tp_name
   sizeof(CppPyObject<pkgTagSection::Tag>),                 // tp_basicsize
   0,                                   // tp_itemsize
   // Methods
   CppDealloc<pkgTagSection::Tag>,      // tp_dealloc
   0,                                   // tp_print
   0,                                   // tp_getattr
   0,                                   // tp_setattr
   0,                                   // tp_compare
   0,                                   // tp_repr
   0,                                   // tp_as_number
   0,                                   // tp_as_sequence
   0,                                   // tp_as_mapping
   0,                                   // tp_hash
   0,                                   // tp_call
   0,                                   // tp_str
   0,                                   // tp_getattro
   0,                                   // tp_setattro
   0,                                   // tp_as_buffer
   (Py_TPFLAGS_DEFAULT                  // tp_flags
    | Py_TPFLAGS_BASETYPE),
   doc_Tag,                             // tp_doc
   CppTraverse<pkgTagSection::Tag>,     // tp_traverse
   CppClear<pkgTagSection::Tag>,        // tp_clear
   0,                                   // tp_richcompare
   0,                                   // tp_weaklistoffset
   0,                                   // tp_iter
   0,                                   // tp_iternext
   0,                                   // tp_methods
   0,                                   // tp_members
   TagGetSet,                           // tp_getset
   0,                                   // tp_base
   0,                                   // tp_dict
   0,                                   // tp_descr_get
   0,                                   // tp_descr_set
   0,                                   // tp_dictoffset
   0,                                   // tp_init
   0,                                   // tp_alloc
   0,                                   // tp_new
};

// Type for a Tag File
PyTypeObject PyTagRewrite_Type =
{
   PyVarObject_HEAD_INIT(&PyType_Type, 0)
   "apt_pkg.TagRewrite",                // tp_name
   sizeof(CppPyObject<pkgTagSection::Tag>),                 // tp_basicsize
   0,                                   // tp_itemsize
   // Methods
   CppDealloc<pkgTagSection::Tag>,      // tp_dealloc
   0,                                   // tp_print
   0,                                   // tp_getattr
   0,                                   // tp_setattr
   0,                                   // tp_compare
   0,                                   // tp_repr
   0,                                   // tp_as_number
   0,                                   // tp_as_sequence
   0,                                   // tp_as_mapping
   0,                                   // tp_hash
   0,                                   // tp_call
   0,                                   // tp_str
   0,                                   // tp_getattro
   0,                                   // tp_setattro
   0,                                   // tp_as_buffer
   Py_TPFLAGS_DEFAULT,                  // tp_flags
   doc_TagRewrite,                      // tp_doc
   CppTraverse<pkgTagSection::Tag>,     // tp_traverse
   CppClear<pkgTagSection::Tag>,        // tp_clear
   0,                                   // tp_richcompare
   0,                                   // tp_weaklistoffset
   0,                                   // tp_iter
   0,                                   // tp_iternext
   0,                                   // tp_methods
   0,                                   // tp_members
   0,                                   // tp_getset
   &PyTag_Type,                         // tp_base
   0,                                   // tp_dict
   0,                                   // tp_descr_get
   0,                                   // tp_descr_set
   0,                                   // tp_dictoffset
   0,                                   // tp_init
   0,                                   // tp_alloc
   PyTagRewrite_New,                    // tp_new
};

// Type for a Tag File
PyTypeObject PyTagRemove_Type =
{
   PyVarObject_HEAD_INIT(&PyType_Type, 0)
   "apt_pkg.TagRemove",                 // tp_name
   sizeof(CppPyObject<pkgTagSection::Tag>),                 // tp_basicsize
   0,                                   // tp_itemsize
   // Methods
   CppDealloc<pkgTagSection::Tag>,      // tp_dealloc
   0,                                   // tp_print
   0,                                   // tp_getattr
   0,                                   // tp_setattr
   0,                                   // tp_compare
   0,                                   // tp_repr
   0,                                   // tp_as_number
   0,                                   // tp_as_sequence
   0,                                   // tp_as_mapping
   0,                                   // tp_hash
   0,                                   // tp_call
   0,                                   // tp_str
   0,                                   // tp_getattro
   0,                                   // tp_setattro
   0,                                   // tp_as_buffer
   Py_TPFLAGS_DEFAULT,                  // tp_flags
   doc_TagRemove,                       // tp_doc
   CppTraverse<pkgTagSection::Tag>,     // tp_traverse
   CppClear<pkgTagSection::Tag>,        // tp_clear
   0,                                   // tp_richcompare
   0,                                   // tp_weaklistoffset
   0,                                   // tp_iter
   0,                                   // tp_iternext
   0,                                   // tp_methods
   0,                                   // tp_members
   0,                                   // tp_getset
   &PyTag_Type,                         // tp_base
   0,                                   // tp_dict
   0,                                   // tp_descr_get
   0,                                   // tp_descr_set
   0,                                   // tp_dictoffset
   0,                                   // tp_init
   0,                                   // tp_alloc
   PyTagRemove_New,                     // tp_new
};

// Type for a Tag File
PyTypeObject PyTagRename_Type =
{
   PyVarObject_HEAD_INIT(&PyType_Type, 0)
   "apt_pkg.TagRename",                 // tp_name
   sizeof(CppPyObject<pkgTagSection::Tag>),                 // tp_basicsize
   0,                                   // tp_itemsize
   // Methods
   CppDealloc<pkgTagSection::Tag>,      // tp_dealloc
   0,                                   // tp_print
   0,                                   // tp_getattr
   0,                                   // tp_setattr
   0,                                   // tp_compare
   0,                                   // tp_repr
   0,                                   // tp_as_number
   0,                                   // tp_as_sequence
   0,                                   // tp_as_mapping
   0,                                   // tp_hash
   0,                                   // tp_call
   0,                                   // tp_str
   0,                                   // tp_getattro
   0,                                   // tp_setattro
   0,                                   // tp_as_buffer
   Py_TPFLAGS_DEFAULT,                  // tp_flags
   doc_TagRename,                       // tp_doc
   CppTraverse<pkgTagSection::Tag>,     // tp_traverse
   CppClear<pkgTagSection::Tag>,        // tp_clear
   0,                                   // tp_richcompare
   0,                                   // tp_weaklistoffset
   0,                                   // tp_iter
   0,                                   // tp_iternext
   0,                                   // tp_methods
   0,                                   // tp_members
   0,                                   // tp_getset
   &PyTag_Type,                         // tp_base
   0,                                   // tp_dict
   0,                                   // tp_descr_get
   0,                                   // tp_descr_set
   0,                                   // tp_dictoffset
   0,                                   // tp_init
   0,                                   // tp_alloc
   PyTagRename_New,                     // tp_new
};
