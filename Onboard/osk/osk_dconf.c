/*
 * Copyright © 2012 marmuta
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

#include <dconf.h>

typedef struct {
    PyObject_HEAD

    DConfClient* client;
} OskDConf;

OSK_REGISTER_TYPE (OskDConf, osk_dconf, "DConf")

static int
osk_dconf_init (OskDConf *odc, PyObject *args, PyObject *kwds)
{
#ifdef DCONF_API_0  // libdconf0 0.12.x, Precise
    odc->client = dconf_client_new(NULL, NULL, NULL, NULL);
#else
    odc->client = dconf_client_new();
#endif
    if (odc->client == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "failed to create dconf client");
        return -1;
    }
    return 0;
}

static void
osk_dconf_dealloc (OskDConf *odc)
{
    g_clear_object(&odc->client);

    OSK_FINISH_DEALLOC (odc);
}

/**
 * unpack_variant:
 * @value: GVariant to unpack
 *
 * Converts @value into a python object.
 */
static PyObject*
unpack_variant(GVariant* value)
{
    PyObject* result = NULL;
    int i;

    GVariantClass class = g_variant_classify (value);
    switch (class)
    {
        case G_VARIANT_CLASS_BOOLEAN:
            result = PyBool_FromLong(g_variant_get_boolean (value));
            break;

        case G_VARIANT_CLASS_BYTE:
            result = PyLong_FromLong(g_variant_get_byte (value));
            break;

        case G_VARIANT_CLASS_INT16:
            result = PyLong_FromLong(g_variant_get_int16 (value));
            break;

        case G_VARIANT_CLASS_UINT16:
            result = PyLong_FromLong(g_variant_get_uint16 (value));
            break;

        case G_VARIANT_CLASS_INT32:
            result = PyLong_FromLong(g_variant_get_int32 (value));
            break;

        case G_VARIANT_CLASS_UINT32:
            result = PyLong_FromLong(g_variant_get_uint32 (value));
            break;

        case G_VARIANT_CLASS_INT64:
            result = PyLong_FromLong(g_variant_get_int64 (value));
            break;

        case G_VARIANT_CLASS_UINT64:
            result = PyLong_FromLong(g_variant_get_uint64 (value));
            break;

        case G_VARIANT_CLASS_DOUBLE:
            result = PyFloat_FromDouble(g_variant_get_double (value));
            break;

        case G_VARIANT_CLASS_STRING:
            result = PyUnicode_FromString(g_variant_get_string (value, NULL));
            break;

        case G_VARIANT_CLASS_ARRAY:
        {
            gsize len = g_variant_n_children (value);

            const GVariantType* type = g_variant_get_type(value);
            if (g_variant_type_is_subtype_of (type, G_VARIANT_TYPE_DICTIONARY))
            {
                // Dictionary
                result = PyDict_New();
                for (i=0; i<len; i++)
                {
                    GVariant* child = g_variant_get_child_value(value, i);
                    GVariant* key   = g_variant_get_child_value(child, 0);
                    GVariant* val   = g_variant_get_child_value(child, 1);

                    PyObject* key_val = unpack_variant(key);
                    PyObject* val_val = unpack_variant(val);
                    g_variant_unref(key);
                    g_variant_unref(val);
                    g_variant_unref(child);
                    if (val_val == NULL || key_val == NULL)
                    {
                        Py_XDECREF(key_val);
                        Py_XDECREF(val_val);
                        Py_DECREF(result);
                        result = NULL;
                        break;
                    }

                    PyDict_SetItem(result, key_val, val_val);
                    Py_DECREF(key_val);
                    Py_DECREF(val_val);
                }
            }
            else
            {
                // Array
                result = PyList_New(len);
                for (i=0; i<len; i++)
                {
                    GVariant* child = g_variant_get_child_value(value, i);
                    PyObject* child_val = unpack_variant(child);
                    g_variant_unref(child);
                    if (child_val == NULL)
                    {
                        Py_DECREF(result);
                        return NULL;
                    }
                    PyList_SetItem(result, i, child_val);
                }
            }
            break;
        }

        case G_VARIANT_CLASS_TUPLE:
        {
            gsize len = g_variant_n_children (value);
            result = PyTuple_New(len);
            if (result == NULL)
                break;
            for (i=0; i<len; i++)
            {
                GVariant* child = g_variant_get_child_value(value, i);
                PyObject* child_val = unpack_variant(child);
                g_variant_unref(child);
                if (child_val == NULL)
                {
                    Py_DECREF(result);
                    result = NULL;
                    break;
                }
                PyTuple_SetItem(result, i, child_val);
            }
            break;
        }

        default:
            PyErr_Format(PyExc_TypeError,
                         "unsupported variant class '%c'",
                         class);
    }

    return result;
}

static PyObject *
osk_dconf_read_key (PyObject *self, PyObject *args)
{
    OskDConf *odc = (OskDConf*) self;
    PyObject* result = NULL;
    char* key;

    if (!PyArg_ParseTuple (args, "s:read_key", &key))
        return NULL;

    GVariant* value = dconf_client_read(odc->client, key);

    if (value)
    {
        result = unpack_variant(value);
        g_variant_unref(value);
    }

    if (PyErr_Occurred())
        return NULL;

    if (result)
        return result;

    Py_RETURN_NONE;
}

static PyMethodDef osk_dconf_methods[] = {
    { "read_key",
        osk_dconf_read_key,
        METH_VARARGS, NULL },

    { NULL, NULL, 0, NULL }
};

