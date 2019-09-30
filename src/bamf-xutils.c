/*
 * Copyright (C) 2012 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Jason Smith <jason.smith@canonical.com>
 *              Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */

#include "bamf-xutils.h"
#include <X11/Xatom.h>
#include <string.h>

static Display *
get_xdisplay (gboolean *opened)
{
  Display *xdisplay;
  xdisplay = gdk_x11_get_default_xdisplay ();

  if (opened)
    *opened = FALSE;

  if (!xdisplay)
    {
      xdisplay = XOpenDisplay (NULL);

      if (xdisplay)
        {
          if (opened)
            *opened = TRUE;
        }
    }

  return xdisplay;
}

static gboolean
gdk_error_trap_pop_and_print (Display *dpy)
{
  gdk_flush ();
  gint error_code = gdk_error_trap_pop ();

  if (error_code)
    {
      gchar tmp[1024];
      XGetErrorText (dpy, error_code, tmp, sizeof (tmp) - 1);
      tmp[sizeof (tmp) - 1] = '\0';

      g_warning("Got an X error: %s\n", tmp);

      return TRUE;
    }

  return FALSE;
}

static void
bamf_xutils_get_string_window_hint_and_type (Window xid, const char *atom_name,
                                             gchar** return_hint, Atom* return_type)
{
  Display *XDisplay;
  gint format;
  gulong numItems;
  gulong bytesAfter;
  Atom type;
  unsigned char *buffer;
  gboolean close_display;

  if (return_hint)
    *return_hint = NULL;

  if (return_type)
    *return_type = AnyPropertyType;

  g_return_if_fail (xid != 0);
  g_return_if_fail (return_hint || return_type);

  close_display = FALSE;
  XDisplay = get_xdisplay (&close_display);

  if (!XDisplay)
  {
    g_warning ("%s: Unable to get a valid XDisplay", G_STRFUNC);
    return;
  }

  gdk_error_trap_push ();

  int result = XGetWindowProperty (XDisplay,  xid,
                                   gdk_x11_get_xatom_by_name (atom_name),
                                   0,  G_MAXINT, False, AnyPropertyType,
                                   &type, &format, &numItems,
                                   &bytesAfter, &buffer);

  if (close_display)
    XCloseDisplay (XDisplay);

  if (result == Success && numItems > 0 && !gdk_error_trap_pop_and_print (XDisplay))
    {
      if (return_type)
        *return_type = type;

      if (return_hint && buffer && buffer[0] != '\0')
        {
          if (type == XA_STRING || type == gdk_x11_get_xatom_by_name("UTF8_STRING"))
            *return_hint = g_strdup ((char*) buffer);
        }

      XFree (buffer);
    }
}

char *
bamf_xutils_get_string_window_hint (Window xid, const char *atom_name)
{
  gchar *hint = NULL;
  bamf_xutils_get_string_window_hint_and_type (xid, atom_name, &hint, NULL);

  return hint;
}

void
bamf_xutils_set_string_window_hint (Window xid, const char *atom_name, const char *value)
{
  Display *XDisplay;
  Atom type;
  gboolean close_display = FALSE;

  g_return_if_fail (xid != 0);
  g_return_if_fail (atom_name);
  g_return_if_fail (value);

  XDisplay = get_xdisplay (&close_display);

  if (!XDisplay)
  {
    g_warning ("%s: Unable to get a valid XDisplay", G_STRFUNC);
    return;
  }

  bamf_xutils_get_string_window_hint_and_type (xid, atom_name, NULL, &type);

  if (type == AnyPropertyType)
    {
      type = XA_STRING;
    }
  else if (type != XA_STRING && type != gdk_x11_get_xatom_by_name("UTF8_STRING"))
    {
      g_error ("Impossible to set the atom %s on Window %lu", atom_name, xid);

      if (close_display)
        XCloseDisplay (XDisplay);

      return;
    }

  gdk_error_trap_push ();

  XChangeProperty (XDisplay, xid, gdk_x11_get_xatom_by_name (atom_name),
                   type, 8, PropModeReplace, (unsigned char *) value, strlen (value));

  gdk_error_trap_pop_and_print (XDisplay);

  if (close_display)
    XCloseDisplay (XDisplay);
}

void
bamf_xutils_get_window_class_hints (Window xid, char **class_instance_name, char **class_name)
{
  Display *xdisplay;
  gboolean close_display = FALSE;

  xdisplay = get_xdisplay (&close_display);

  if (!xdisplay)
  {
    g_warning ("%s: Unable to get a valid XDisplay", G_STRFUNC);
    return;
  }

  XClassHint class_hint;
  class_hint.res_name = NULL;
  class_hint.res_class = NULL;

  gdk_error_trap_push ();

  XGetClassHint(xdisplay, xid, &class_hint);

  if (!gdk_error_trap_pop_and_print (xdisplay))
    {
      if (class_name && class_hint.res_class && class_hint.res_class[0] != 0)
        *class_name = g_convert (class_hint.res_class, -1, "utf-8", "iso-8859-1",
                                 NULL, NULL, NULL);

      if (class_instance_name && class_hint.res_name && class_hint.res_name[0] != 0)
        *class_instance_name = g_convert (class_hint.res_name, -1, "utf-8", "iso-8859-1",
                                          NULL, NULL, NULL);
    }

  XFree (class_hint.res_class);
  XFree (class_hint.res_name);

  if (close_display)
    XCloseDisplay (xdisplay);
}
