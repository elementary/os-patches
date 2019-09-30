/*
 * Copyright © 2013 Gerd Kohlberger
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
#include <canberra.h>

#define DEFAULT_SOUND_ID 0

typedef struct {
    PyObject_HEAD

    ca_context* ca;
} OskAudio;

static gboolean osk_audio_init_canberra(OskAudio* audio);

OSK_REGISTER_TYPE(OskAudio, osk_audio, "Audio")

static int
osk_audio_init(OskAudio* audio, PyObject* args, PyObject* kwds)
{
    if (!osk_audio_init_canberra(audio))
    {
        PyErr_SetString(OSK_EXCEPTION, "failed to initialize canberra");
        return -1;
    }
    return 0;
}

static void
osk_audio_dealloc(OskAudio* audio)
{
    if (audio->ca)
        ca_context_destroy(audio->ca);

    OSK_FINISH_DEALLOC(audio);
}

static gboolean
osk_audio_init_canberra(OskAudio* audio)
{
    GdkScreen* screen;
    ca_proplist* props;
    const char* name;
    int nr;

    if (ca_context_create(&audio->ca) != CA_SUCCESS)
        return FALSE;

    screen = gdk_screen_get_default();
    nr = gdk_screen_get_number(screen);
    name = gdk_display_get_name(gdk_screen_get_display(screen));

    /* Set default application properties */
    ca_proplist_create(&props);
    ca_proplist_sets(props, CA_PROP_APPLICATION_NAME, "Onboard");
    ca_proplist_sets(props, CA_PROP_APPLICATION_ID, "org.onboard.Onboard");
    ca_proplist_sets(props, CA_PROP_APPLICATION_ICON_NAME, "onboard");
    ca_proplist_sets(props, CA_PROP_WINDOW_X11_DISPLAY, name);
    ca_proplist_setf(props, CA_PROP_WINDOW_X11_SCREEN, "%i", nr);
    ca_context_change_props_full(audio->ca, props);
    ca_proplist_destroy(props);

    return TRUE;
}

static PyObject*
osk_audio_play(PyObject* self, PyObject* args)
{
    OskAudio* audio = (OskAudio*) self;
    GdkScreen* screen;
    ca_proplist* props;
    const char* event_id;
    float x, y;
    int sw, sh, ret;

    if (!PyArg_ParseTuple(args, "sff", &event_id, &x, &y))
        return NULL;

    screen = gdk_screen_get_default();
    sw = gdk_screen_get_width(screen);
    sh = gdk_screen_get_height(screen);

    ca_proplist_create(&props);
    ca_proplist_sets(props, CA_PROP_EVENT_ID, event_id);

    if (x != -1 && y != -1)
    {
        ca_proplist_setf(props, CA_PROP_EVENT_MOUSE_X, "%0.0f", x);
        ca_proplist_setf(props, CA_PROP_EVENT_MOUSE_Y, "%0.0f", y);

        /* comment from canberra-gtk.c:
         * We use these strange format strings here to avoid that libc
         * applies locale information on the formatting of floating numbers. */
        ca_proplist_setf(props, CA_PROP_EVENT_MOUSE_HPOS, "%i.%03i",
                         (int) x / (sw - 1), (int) (1000.0 * x / (sw - 1)) % 1000);
        ca_proplist_setf(props, CA_PROP_EVENT_MOUSE_VPOS, "%i.%03i",
                         (int) y / (sh - 1), (int) (1000.0 * y / (sh - 1)) % 1000);
    }
    ret = ca_context_play_full(audio->ca, DEFAULT_SOUND_ID, props, NULL, NULL);

    ca_proplist_destroy(props);

    if (ret < 0)
    {
        PyErr_SetString(OSK_EXCEPTION, ca_strerror(ret));
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject*
osk_audio_cancel(PyObject* self, PyObject* args)
{
    OskAudio* audio = (OskAudio*) self;
    int ret;

    ret = ca_context_cancel(audio->ca, DEFAULT_SOUND_ID);
    if (ret < 0)
    {
        PyErr_SetString(OSK_EXCEPTION, ca_strerror(ret));
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject*
osk_audio_enable(PyObject* self, PyObject* args)
{
    OskAudio* audio = (OskAudio*) self;
    ca_context_change_props(audio->ca, CA_PROP_CANBERRA_ENABLE, "1", NULL);
    Py_RETURN_NONE;
}

static PyObject*
osk_audio_disable(PyObject* self, PyObject* args)
{
    OskAudio* audio = (OskAudio*) self;
    ca_context_change_props(audio->ca, CA_PROP_CANBERRA_ENABLE, "0", NULL);
    Py_RETURN_NONE;
}

static PyObject*
osk_audio_set_theme(PyObject* self, PyObject* args)
{
    OskAudio* audio = (OskAudio*) self;
    const char* theme;
    int ret;

    if (!PyArg_ParseTuple(args, "s", &theme))
        return NULL;

    ret = ca_context_change_props(audio->ca,
                                  CA_PROP_CANBERRA_XDG_THEME_NAME, theme,
                                  NULL);
    if (ret < 0)
    {
        PyErr_SetString(OSK_EXCEPTION, ca_strerror(ret));
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
osk_audio_cache_sample(PyObject* self, PyObject* args)
{
    OskAudio* audio = (OskAudio*) self;
    ca_proplist* props;
    const char* event_id;
    int ret;

    if (!PyArg_ParseTuple(args, "s", &event_id))
        return NULL;

    ca_proplist_create(&props);
    ca_proplist_sets(props, CA_PROP_EVENT_ID, event_id);
    ret = ca_context_cache_full(audio->ca, props);
    ca_proplist_destroy(props);

    if (ret < 0)
    {
        PyErr_SetString(OSK_EXCEPTION, ca_strerror(ret));
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef osk_audio_methods[] = {
    { "play",         osk_audio_play,         METH_VARARGS, NULL },
    { "cancel",       osk_audio_cancel,       METH_NOARGS,  NULL },
    { "enable",       osk_audio_enable,       METH_NOARGS,  NULL },
    { "disable",      osk_audio_disable,      METH_NOARGS,  NULL },
    { "set_theme",    osk_audio_set_theme,    METH_VARARGS, NULL },
    { "cache_sample", osk_audio_cache_sample, METH_VARARGS, NULL },
    { NULL, NULL, 0, NULL }
};

