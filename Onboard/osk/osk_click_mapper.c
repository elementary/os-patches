/*
 * Copyright © 2011 Gerd Kohlberger
 * Copyright © 2012-2013 marmuta
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
#include <X11/extensions/XTest.h>


#define MAX_GRAB_DURATION 15   // max time to hold a pointer grab [s]

enum
{
    PRIMARY_BUTTON   = 1,
    MIDDLE_BUTTON    = 2,
    SECONDARY_BUTTON = 3,
};

enum
{
    CLICK_TYPE_SINGLE = 3,
    CLICK_TYPE_DOUBLE = 2,
    CLICK_TYPE_DRAG   = 1,
};


/* data for convert_primary_click() */
typedef struct {
    Display *xdisplay;
    unsigned int button;
    unsigned int click_type;
    unsigned int drag_started;
    unsigned int drag_button;
    int          drag_last_x;
    int          drag_last_y;
    gint64       drag_last_time;
    gint64       drag_slowdown_time;
    unsigned int modifier;
    Bool         enable_conversion;
    PyObject*    exclusion_rects;
    PyObject*    click_done_callback;
    guint        grab_release_timer;
} OskBMGrabInfo;

/* saved button mappings */
#define MAX_BUTTONS 512
typedef struct {
    int device_id;
    unsigned char buttons[MAX_BUTTONS];
    int num_buttons;
} PointerState;

/* data for map_pointer_button() */
typedef struct {
    Display *xdisplay;
    unsigned int button;
    PointerState* saved_pointer_states;
    unsigned int num_saved_pointer_states;
    unsigned int num_devices;
} OskBMMapInfo;

/* instance data */
typedef struct {
    PyObject_HEAD

    GdkDisplay *display;
    OskBMGrabInfo info;
    OskBMMapInfo map_info;
} OskButtonMapper;

static void stop_convert_click(OskBMGrabInfo* info);
static void restore_pointer_buttons(OskBMMapInfo* info);
static Display* get_x_display(OskButtonMapper* instance);

OSK_REGISTER_TYPE_WITH_MEMBERS (OskButtonMapper, osk_click_mapper,
                                "ClickMapper")

static int
osk_click_mapper_init (OskButtonMapper *instance, PyObject *args, PyObject *kwds)
{
    OskBMGrabInfo* info = &instance->info;
    memset(info, 0, sizeof(*info));
    info->button = PRIMARY_BUTTON;
    info->click_type = CLICK_TYPE_SINGLE;
    info->enable_conversion = True;
    instance->display = gdk_display_get_default ();

    OskBMMapInfo* map_info = &instance->map_info;
    memset(map_info, 0, sizeof(*map_info));

    Display* xdisplay = get_x_display(instance);
    if (xdisplay) // not on wayland?
    {
        int nop;
        if (!XTestQueryExtension (xdisplay, &nop, &nop, &nop, &nop))
        {
            PyErr_SetString (OSK_EXCEPTION, "failed initialize XTest extension");
            return -1;
        }

        /* send events inspite of other grabs */
        XTestGrabControl (xdisplay, True);
    }

    return 0;
}

static void
osk_click_mapper_dealloc (OskButtonMapper *instance)
{
    restore_pointer_buttons(&instance->map_info);
    stop_convert_click(&instance->info);

    OSK_FINISH_DEALLOC (instance);
}

static Display*
get_x_display (OskButtonMapper* instance)
{
    if (GDK_IS_X11_DISPLAY (instance->display)) // not on wayland?
        return GDK_DISPLAY_XDISPLAY (instance->display);
    return NULL;
}


typedef Bool (*EnumerateDeviceFunc)(OskBMMapInfo* info, XDevice* device);

/* 
 * Remap buttons of the given device
 */
static Bool
map_button_func(OskBMMapInfo* info, XDevice* device)
{
    if (info->saved_pointer_states == NULL)
    {
        // allocate space for saving the old button mappings
        info->saved_pointer_states = g_new0(PointerState, info->num_devices);
        if (!info->saved_pointer_states)
            return False;
        info->num_saved_pointer_states = 0;
    }

    // remap the buttons of all devices
    unsigned char buttons[MAX_BUTTONS];
    int num_buttons = XGetDeviceButtonMapping(info->xdisplay, device,
                                              buttons, sizeof(buttons));
    if (num_buttons >= 3)
    {
        //printf("mapping device %d\n", (int)device->device_id);

        PointerState* saved_state = info->saved_pointer_states +
                                    info->num_saved_pointer_states;
        saved_state->device_id = device->device_id;
        saved_state->num_buttons = num_buttons;
        memcpy(saved_state->buttons, buttons, sizeof(buttons));

        int button = info->button;
        unsigned char tmp = buttons[0];
        buttons[0] = buttons[button - 1];
        buttons[button - 1] = tmp;
        XSetDeviceButtonMapping(info->xdisplay, device,
                                buttons, num_buttons);

        info->num_saved_pointer_states++;
    }

    return True;
}

/* 
 * Restore button mapping of the given device.
 */
static Bool
restore_button_func(OskBMMapInfo* info, XDevice* device)
{
    int i;
    PointerState* states = info->saved_pointer_states;
    int n = info->num_saved_pointer_states;

    for (i=0; i<n; i++)
    {
        PointerState* state = states + i;
        if (state->device_id == device->device_id)
        {
            if (state->num_buttons)
            {
                //printf("restoring device %d\n", (int)device->device_id);
                XSetDeviceButtonMapping(info->xdisplay, device,
                                        state->buttons, state->num_buttons);
            }
            break;
        }
    }

    return True;
}

static void
for_each_x_pointer(OskBMMapInfo* info, EnumerateDeviceFunc func)
{
    Display* xdisplay = info->xdisplay;

    int i;
    int n = 0;
    XDeviceInfo* device_infos = XListInputDevices(xdisplay, &n);
    if (device_infos)
    {
        info->num_devices = n;
        for (i=0; i<n; i++)
        {
            XDeviceInfo* device_info = device_infos + i;
            if (device_info->use == IsXExtensionPointer)
            {
                XDevice* device = XOpenDevice(xdisplay, device_info->id);
                if (device)
                {
                    Bool ret = (*func)(info, device);
                    XCloseDevice(xdisplay, device);
                    if (!ret)
                        break;
                }
            }
        }
        XFreeDeviceList(device_infos);
    }
}

/* map the given button to the primary button */
static PyObject *
osk_click_mapper_map_pointer_button (PyObject *self, PyObject *args)
{
    OskButtonMapper *instance = (OskButtonMapper*) self;
    OskBMMapInfo *info = &instance->map_info;
    int button;

    if (!PyArg_ParseTuple (args, "I", &button))
        return NULL;

    if (button < 1 || button > 3)
    {
        PyErr_SetString (OSK_EXCEPTION, "unsupported button number");
        return NULL;
    }

    restore_pointer_buttons(info);

    Display* xdisplay = get_x_display(instance);
    if (xdisplay == NULL)
    {
        PyErr_SetString(PyExc_TypeError, "Not an X display");
        return NULL;
    }

    int event, error;
    int major_opcode;
    if (!XQueryExtension (xdisplay, "XInputExtension",
                          &major_opcode, &event, &error))
    {
        PyErr_SetString (OSK_EXCEPTION, "XInput extension unavailable");
        return NULL;
    }

    info->xdisplay = xdisplay;
    info->button = button;

    for_each_x_pointer(info, map_button_func);

    Py_RETURN_NONE;
}

static PyObject *
osk_click_mapper_restore_pointer_buttons (PyObject *self, PyObject *args)
{
    OskButtonMapper *instance = (OskButtonMapper*) self;
    OskBMMapInfo *info = &instance->map_info;

    restore_pointer_buttons(info);

    Py_RETURN_NONE;
}

/* restore all pointer button mappings */
static void
restore_pointer_buttons(OskBMMapInfo* info)
{
    if (info->xdisplay)
    {
        for_each_x_pointer(info, restore_button_func);

        g_free(info->saved_pointer_states);
        info->saved_pointer_states = NULL;
        info->num_saved_pointer_states = 0;
        info->xdisplay = NULL;
    }
}


static void
notify_click_done(PyObject* callback)
{
    if (callback)
        osk_util_idle_call(callback, NULL);
}

static Bool
can_convert_click(OskBMGrabInfo* info, int x_root, int y_root)
{
    if (!info->enable_conversion)
        return False;

    // Check if the the given point (x_root, y_root) lies
    // within any of the exclusion rectangles.
    if (info->exclusion_rects)
    {
        int i;
        int n = PySequence_Length(info->exclusion_rects);
        for (i = 0; i < n; i++)
        {
            PyObject* rect = PySequence_GetItem(info->exclusion_rects, i);
            if (rect == NULL)
                break;
            int m = PySequence_Length(rect);
            if (m != 4)
                break;

            PyObject* item;

            item = PySequence_GetItem(rect, 0);
            int x = PyInt_AsLong(item);
            Py_DECREF(item);

            item = PySequence_GetItem(rect, 1);
            int y = PyInt_AsLong(item);
            Py_DECREF(item);

            item = PySequence_GetItem(rect, 2);
            int w = PyInt_AsLong(item);
            Py_DECREF(item);

            item = PySequence_GetItem(rect, 3);
            int h = PyInt_AsLong(item);
            Py_DECREF(item);

            Py_DECREF(rect);

            if (x_root >= x && x_root < x + w &&
                y_root >= y && y_root < y + h)
            {
                return False;
            }
        }
    }

    return True;
}

static Bool
start_grab(OskBMGrabInfo* info)
{
    gdk_error_trap_push ();
    XGrabButton (info->xdisplay, Button1, info->modifier,
                 DefaultRootWindow (info->xdisplay),
                 False, // owner_events == False: Onboard itself can be clicked
                 ButtonPressMask | ButtonReleaseMask,
                 GrabModeSync, GrabModeAsync, None, None);
    gdk_flush ();
    return !gdk_error_trap_pop();
}

static void
stop_grab(OskBMGrabInfo* info)
{
    gdk_error_trap_push();
    XUngrabButton(info->xdisplay,
                  Button1,
                  info->modifier,
                  DefaultRootWindow(info->xdisplay));
    gdk_error_trap_pop_ignored();
}

typedef struct {
    OskBMGrabInfo* info;
} DragPollingData;

static gboolean
on_drag_polling (DragPollingData *data)
{
    const double MIN_DRAG_VELOCITY = 60.0; // min velocity to initiate drag end
    const int    DRAG_END_DELAY    = 1000; // ms below min velocity to end drag

    OskBMGrabInfo* info = data->info;
    if (!info->drag_started)
        return FALSE;  // stop on grab_release_timer

    Display* dpy = info->xdisplay;
    Window root, child;
    int x, y, x_root, y_root;
    unsigned int mask = 0;
    XQueryPointer (dpy, DefaultRootWindow (dpy),
                   &root, &child, &x_root, &y_root, &x, &y, &mask);

    int dx = x - info->drag_last_x;
    int dy = y - info->drag_last_y;
    double d = sqrt(dx * dx + dy * dy);
    gint64 now = g_get_monotonic_time();
    gint64 elapsed = now - info->drag_last_time;
    double velocity = d / elapsed * 1e6; // [s]
    if (velocity > MIN_DRAG_VELOCITY)
        info->drag_slowdown_time = now;

    info->drag_last_x = x;
    info->drag_last_y = y;
    info->drag_last_time = now;

    elapsed = (now - info->drag_slowdown_time) / 1000; // [ms]
    if (elapsed > DRAG_END_DELAY)
    {
        XTestFakeButtonEvent (dpy, info->drag_button, False, CurrentTime);

        PyObject* callback = info->click_done_callback;
        Py_XINCREF(callback);

        stop_convert_click(info);

        notify_click_done(callback);
        Py_XDECREF(callback);

        g_free (data);
        return FALSE;
    }
    return TRUE;
}

static void
start_drag_polling (OskBMGrabInfo* info)
{
    DragPollingData* data = g_new (DragPollingData, 1);
    data->info = info;

    g_timeout_add (100, (GSourceFunc) on_drag_polling, data);
}

static GdkFilterReturn
osk_click_mapper_event_filter (GdkXEvent       *gdk_xevent,
                       GdkEvent        *gdk_event,
                       OskBMGrabInfo *info)
{
    PyGILState_STATE state = PyGILState_Ensure();
    XEvent *event = gdk_xevent;

    if (event->type == ButtonPress || event->type == ButtonRelease)
    {
        XButtonEvent *bev = (XButtonEvent *) event;
        if (bev->button == Button1)
        {
            unsigned int button = info->button;
            unsigned int click_type = info->click_type;
            PyObject* callback = info->click_done_callback;
            Py_XINCREF(callback);

            /* Don't convert the click if the click is about to be canceled,
             * i.e. any of the click buttons was hit.
             */
            if (!can_convert_click(info, bev->x_root, bev->y_root))
            {
                /* Replay original event.
                 * This will usually give a regular left click.
                 */
                XAllowEvents (bev->display, ReplayPointer, bev->time);

                /*
                 * Don't stop the grab here, Onboard controls the
                 * cancellation from the python side. I does so by
                 * explicitely setting the convert click to
                 * PRIMARY_BUTTON, CLICK_TYPE_SINGLE.
                 */
            }
            else
            {
                /* Consume original event */
                XAllowEvents (bev->display, AsyncPointer, bev->time);

                if (event->type == ButtonRelease)
                {
                    /* Stop the grab before sending any fake events.
                     */
                    stop_grab(info);

                    /* Move the pointer to the actual click position.
                     * Else faked button presses on the touch screen of
                     * the Nexus 7 are offset by a couple hundred pixels.
                     */
                    XTestFakeMotionEvent(bev->display, -1, bev->x_root, bev->y_root, CurrentTime);

                    /* Synthesize button click */
                    unsigned long delay = 40;
                    switch (click_type)
                    {
                        case CLICK_TYPE_SINGLE:
                            XTestFakeButtonEvent (bev->display, button, True, CurrentTime);
                            XTestFakeButtonEvent (bev->display, button, False, 50);
                            break;

                        case CLICK_TYPE_DOUBLE:
                            XTestFakeButtonEvent (bev->display, button, True, CurrentTime);
                            XTestFakeButtonEvent (bev->display, button, False, delay);
                            XTestFakeButtonEvent (bev->display, button, True, delay);
                            XTestFakeButtonEvent (bev->display, button, False, delay);
                            break;

                        case CLICK_TYPE_DRAG:
                            XTestFakeButtonEvent (bev->display, button, True, CurrentTime);

                            gint64 now = g_get_monotonic_time();
                            info->drag_started = True;
                            info->drag_button = button;
                            info->drag_last_time = now;
                            info->drag_slowdown_time = now;
                            start_drag_polling(info);
                            break;
                    }

                    if (click_type != CLICK_TYPE_DRAG)
                    {
                        // notify python that the click is done
                        stop_convert_click(info);
                        notify_click_done(callback);
                    }
                }
            }
            Py_XDECREF(callback);
        }
    }

    PyGILState_Release(state);

    return GDK_FILTER_CONTINUE;
}

static void
stop_convert_click(OskBMGrabInfo* info)
{
    if (info->xdisplay)
    {
        gdk_window_remove_filter (NULL,
                                  (GdkFilterFunc) osk_click_mapper_event_filter,
                                  info);
        stop_grab(info);
    }
    info->button = PRIMARY_BUTTON;
    info->click_type = CLICK_TYPE_SINGLE;
    info->drag_started = False;
    info->drag_button = 0;
    info->xdisplay = NULL;

    Py_XDECREF(info->exclusion_rects);
    info->exclusion_rects = NULL;

    Py_XDECREF(info->click_done_callback);
    info->click_done_callback = NULL;

    if (info->grab_release_timer)
        g_source_remove (info->grab_release_timer);
    info->grab_release_timer = 0;
}

static unsigned int
get_modifier_state (Display *dpy)
{
    Window root, child;
    int x, y, x_root, y_root;
    unsigned int mask = 0;

    XQueryPointer (dpy, DefaultRootWindow (dpy),
                   &root, &child, &x_root, &y_root, &x, &y, &mask);

    /* remove mouse button states */
    return mask & 0xFF;
}

static
gboolean grab_release_timer_callback(gpointer user_data)
{
    OskButtonMapper*         instance = (OskButtonMapper*) user_data;
    OskBMGrabInfo* info = &instance->info;
    Display* xdisplay     = get_x_display(instance);
    PyObject* callback = info->click_done_callback;

    notify_click_done(callback);

    // Always release the XTest button.
    // -> recover from having the button stuck
    int button = Button1;
    if (info->drag_button)
        button = info->drag_button;
    XTestFakeButtonEvent (xdisplay, button, False, CurrentTime);

    stop_convert_click(info);
    restore_pointer_buttons(&instance->map_info);

    info->grab_release_timer = 0;

    return False;
}

/**
 * osk_click_mapper_convert_primary_click:
 * @button: Button number to convert (unsigned int)
 *
 * Converts the next mouse "left-click" to a @button click.
 */
static PyObject *
osk_click_mapper_convert_primary_click (PyObject *self, PyObject *args)
{
    OskButtonMapper *instance = (OskButtonMapper*) self;
    OskBMGrabInfo *info = &instance->info;
    Display         *dpy;
    unsigned int     button;
    unsigned int     click_type;
    unsigned int     modifier;
    PyObject*        exclusion_rects = NULL;
    PyObject*        callback = NULL;

    if (!PyArg_ParseTuple (args, "II|OO", &button,
                                          &click_type,
                                          &exclusion_rects,
                                          &callback))
        return NULL;

    if (button < 1 || button > 3)
    {
        PyErr_SetString (OSK_EXCEPTION, "unsupported button number");
        return NULL;
    }

    stop_convert_click(info);

    if (exclusion_rects)
    {
        if (!PySequence_Check(exclusion_rects))
        {
            PyErr_SetString(PyExc_ValueError, "expected sequence type");
            return False;
        }
        Py_INCREF(exclusion_rects);
        info->exclusion_rects = exclusion_rects;
    }

    /* cancel the click ? */
    if (button == PRIMARY_BUTTON &&
        click_type == CLICK_TYPE_SINGLE)
    {
        Py_RETURN_NONE;
    }

    dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    modifier = get_modifier_state (dpy);

    info->button = button;
    info->click_type = click_type;
    info->xdisplay = dpy;
    info->modifier = modifier;
    Py_XINCREF(callback);         /* Add a reference to new callback */
    Py_XDECREF(info->click_done_callback);   /* Dispose of previous callback */
    info->click_done_callback = callback;    /* Remember new callback */

    if (!start_grab(info))
    {
        stop_convert_click(info);
        PyErr_SetString (OSK_EXCEPTION, "failed to grab button");
        return NULL;
    }
    gdk_window_add_filter (NULL, (GdkFilterFunc) osk_click_mapper_event_filter, info);

    // Make sure the grab can't get stuck for long. On the Nexus 7 this
    // is a frequent occurrence.
    info->grab_release_timer = g_timeout_add_seconds(MAX_GRAB_DURATION,
                                                     grab_release_timer_callback,
                                                     instance);
    Py_RETURN_NONE;
}



static PyObject *
osk_click_mapper_generate_motion_event (PyObject *self, PyObject *args)
{
    OskButtonMapper *instance = (OskButtonMapper*) self;
    unsigned int     x_root, y_root;

    Display* xdisplay = get_x_display(instance);
    if (!xdisplay)
    {
        PyErr_SetString (OSK_EXCEPTION, "failed to get X display");
        return NULL;
    }

    if (!PyArg_ParseTuple (args, "II", &x_root, &y_root))
        return NULL;

    XTestFakeMotionEvent(xdisplay, -1, x_root, y_root, CurrentTime);

    Py_RETURN_NONE;
}

static PyObject *
osk_click_mapper_generate_button_event (PyObject *self, PyObject *args)
{
    OskButtonMapper *instance = (OskButtonMapper*) self;
    unsigned int button;
    unsigned long time = CurrentTime;

    Display* xdisplay = get_x_display(instance);
    if (!xdisplay)
    {
        PyErr_SetString (OSK_EXCEPTION, "failed to get X display");
        return NULL;
    }
#if PY_VERSION_HEX >= 0x03030000
    Bool press;
    if (!PyArg_ParseTuple (args, "Ip|k", &button, &press, &time))
        return NULL;
#else
    // On Precise with python 3.2 with format char 'p':
    // 'TypeError: must be impossible<bad format char>, not bool'
    int press;
    if (!PyArg_ParseTuple (args, "Ii|k", &button, &press, &time))
        return NULL;
#endif

    XTestFakeButtonEvent (xdisplay, button, press, time);

    Py_RETURN_NONE;
}

static PyMethodDef osk_click_mapper_methods[] = {
    { "convert_primary_click",
        osk_click_mapper_convert_primary_click,
        METH_VARARGS, NULL },
    { "map_pointer_button",
        osk_click_mapper_map_pointer_button,
        METH_VARARGS, NULL },
    { "restore_pointer_buttons",
        osk_click_mapper_restore_pointer_buttons,
        METH_VARARGS, NULL },

    { "generate_motion_event",
        osk_click_mapper_generate_motion_event,
        METH_VARARGS, NULL },
    { "generate_button_event",
        osk_click_mapper_generate_button_event,
        METH_VARARGS, NULL },

    { NULL, NULL, 0, NULL }
};

static PyMemberDef osk_click_mapper_members[] = {
    {"button", T_UINT, offsetof(OskButtonMapper, info) +
                       offsetof(OskBMGrabInfo, button), RESTRICTED, NULL },
    {"click_type", T_UINT, offsetof(OskButtonMapper, info) +
                       offsetof(OskBMGrabInfo, click_type), RESTRICTED, NULL },
    {NULL}
};

static PyGetSetDef osk_click_mapper_getsetters[] = {
    {NULL}
};

