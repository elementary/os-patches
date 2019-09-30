/*
 * Copyright © 2012 Gerd Kohlberger
 *
 * Onboard is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Onboard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "osk_module.h"

#include <gdk/gdkx.h>
#include <X11/Xatom.h>

typedef struct {
    PyObject_HEAD
} OskStruts;

OSK_REGISTER_TYPE (OskStruts, osk_struts, "Struts")

static int
osk_struts_init (OskStruts *odc, PyObject *args, PyObject *kwds)
{
    return 0;
}

static void
osk_struts_dealloc (OskStruts *struts)
{
    OSK_FINISH_DEALLOC (struts);
}

static PyObject *
osk_struts_set (PyObject *self, PyObject *args)
{
    Display       *dpy;
    unsigned long  xid;
    unsigned long  struts[12] = { 0, };
    PyObject      *obj, *seq, **items;
    int            i;

    if (!PyArg_ParseTuple (args, "kO", &xid, &obj))
        return NULL;

    seq = PySequence_Fast (obj, "expected sequence type");
    if (!seq)
        return NULL;

    if (PySequence_Fast_GET_SIZE (seq) != 12)
    {
        PyErr_SetString (PyExc_ValueError, "expected 12 values");
        Py_DECREF (seq);
        return NULL;
    }

    items = PySequence_Fast_ITEMS (seq);

    for (i = 0; i < 12; i++, items++)
    {
        struts[i] = PyLong_AsUnsignedLongMask (*items);

        if (PyErr_Occurred ())
        {
            Py_DECREF (seq);
            return NULL;
        }

        if (struts[i] < 0)
        {
            PyErr_SetString (PyExc_ValueError, "expected value >= 0");
            Py_DECREF (seq);
            return NULL;
        }
    }

    Py_DECREF (seq);

    dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    gdk_error_trap_push ();

    XChangeProperty (dpy, xid,
                     XInternAtom (dpy, "_NET_WM_STRUT", False),
                     XA_CARDINAL, 32, PropModeReplace,
                     (unsigned char *) &struts, 4);

    XChangeProperty (dpy, xid,
                     XInternAtom (dpy, "_NET_WM_STRUT_PARTIAL", False),
                     XA_CARDINAL, 32, PropModeReplace,
                     (unsigned char *) &struts, 12);


    gdk_error_trap_pop_ignored ();

    Py_RETURN_NONE;
}

static PyObject *
osk_struts_clear (PyObject *self, PyObject *args)
{
    Display      *dpy;
    unsigned long xid;

    if (!PyArg_ParseTuple (args, "k", &xid))
        return NULL;

    dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    gdk_error_trap_push ();

    XDeleteProperty (dpy, xid, XInternAtom (dpy, "_NET_WM_STRUT", False));
    XDeleteProperty (dpy, xid, XInternAtom (dpy, "_NET_WM_STRUT_PARTIAL", False));

    gdk_error_trap_pop_ignored ();

    Py_RETURN_NONE;
}

static PyMethodDef osk_struts_methods[] = {
    { "set",   osk_struts_set,   METH_VARARGS, NULL },
    { "clear", osk_struts_clear, METH_VARARGS, NULL },
    { NULL,    NULL,             0,            NULL }
};

