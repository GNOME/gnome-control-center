#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include "wm-common.h"

typedef struct _WMCallbackData
{
  GFunc func;
  gpointer data;
} WMCallbackData;

/* Our WM Window */
static Window wm_window = None;

/*
 * These push/pop implementations are based on the GDK versions, except that they
 * use only non-deprecated API.
 */

static GPtrArray*
push_error_traps (void)
{
  GdkDisplayManager *manager;
  g_autoptr(GPtrArray) trapped_displays = NULL;
  g_autoptr(GSList) displays = NULL;
  GSList *l;

  manager = gdk_display_manager_get ();
  displays = gdk_display_manager_list_displays (manager);
  trapped_displays = g_ptr_array_new ();

  for (l = displays; l != NULL; l = l->next)
    {
      GdkDisplay *display = l->data;

#ifdef GDK_WINDOWING_X11
      if (GDK_IS_X11_DISPLAY (display))
        {
          gdk_x11_display_error_trap_push (display);
          g_ptr_array_add (trapped_displays, display);
        }
#endif
    }

  return g_steal_pointer (&trapped_displays);
}

static gint
pop_error_traps (GPtrArray *displays)
{
  guint i;
  gint result;

  result = 0;

  for (i = 0; displays && i < displays->len; i++)
    {
      GdkDisplay *display;
      gint code = 0;

      display = g_ptr_array_index (displays, i);

#ifdef GDK_WINDOWING_X11
      code = gdk_x11_display_error_trap_pop (display);
#endif

      if (code != 0)
        result = code;
    }

  return result;
}

static char *
wm_common_get_window_manager_property (Atom atom)
{
  g_autoptr(GPtrArray) trapped_displays = NULL;
  Atom utf8_string, type;
  int result;
  char *retval;
  int format;
  gulong nitems;
  gulong bytes_after;
  gchar *val;

  if (wm_window == None)
    return NULL;

  utf8_string = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "UTF8_STRING", False);

  trapped_displays = push_error_traps ();

  val = NULL;
  result = XGetWindowProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                               wm_window,
                               atom,
                               0, G_MAXLONG,
                               False, utf8_string,
                               &type, &format, &nitems,
                               &bytes_after, (guchar **) &val);

  if (pop_error_traps (trapped_displays) ||
      result != Success ||
      type != utf8_string ||
      format != 8 ||
      nitems == 0 ||
      !g_utf8_validate (val, nitems, NULL))
    {
      retval = NULL;
    }
  else
    {
      retval = g_strndup (val, nitems);
    }

  g_clear_pointer (&val, XFree);

  return retval;
}
static gchar*
wm_common_get_current_window_manager (void)
{
  Atom atom = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "_NET_WM_NAME", False);
  char *result;

  result = wm_common_get_window_manager_property (atom);
  if (result)
    return result;
  else
    return g_strdup (WM_COMMON_UNKNOWN);
}

GStrv
wm_common_get_current_keybindings (void)
{
  g_autofree gchar* keybindings = NULL;
  g_auto(GStrv) results = NULL;
  Atom keybindings_atom;

  keybindings_atom = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "_GNOME_WM_KEYBINDINGS", False);
  keybindings = wm_common_get_window_manager_property (keybindings_atom);

  if (keybindings)
    {
      GStrv p;

      results = g_strsplit (keybindings, ",", -1);

      for (p = results; p && *p; p++)
        g_strstrip (*p);
    }
  else
    {
      g_autofree gchar *wm_name = NULL;
      Atom wm_atom;
      gchar *to_copy[2] = { NULL, NULL };

      wm_atom = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "_NET_WM_NAME", False);
      wm_name = wm_common_get_window_manager_property (wm_atom);

      to_copy[0] = wm_name ? wm_name : WM_COMMON_UNKNOWN;

      results = g_strdupv (to_copy);
    }

  return g_steal_pointer (&results);
}

static void
update_wm_window (void)
{
  g_autoptr(GPtrArray) trapped_displays = NULL;
  Window *xwindow;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;

  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), GDK_ROOT_WINDOW (),
                      XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "_NET_SUPPORTING_WM_CHECK", False),
                      0, G_MAXLONG, False, XA_WINDOW, &type, &format,
                      &nitems, &bytes_after, (guchar **) &xwindow);

  if (type != XA_WINDOW)
    {
      wm_window = None;
     return;
    }

  trapped_displays = push_error_traps ();

  XSelectInput (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), *xwindow, StructureNotifyMask | PropertyChangeMask);
  XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), False);

  if (pop_error_traps (trapped_displays))
    {
       XFree (xwindow);
       wm_window = None;
       return;
    }

  wm_window = *xwindow;
  XFree (xwindow);
}

static GdkFilterReturn
wm_window_event_filter (GdkXEvent *xev,
                        GdkEvent  *event,
                        gpointer   data)
{
  WMCallbackData *ncb_data = (WMCallbackData*) data;
  XEvent *xevent = (XEvent *)xev;

  if ((xevent->type == DestroyNotify &&
       wm_window != None && xevent->xany.window == wm_window) ||
      (xevent->type == PropertyNotify &&
       xevent->xany.window == GDK_ROOT_WINDOW () &&
       xevent->xproperty.atom == (XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),  "_NET_SUPPORTING_WM_CHECK", False))) ||
      (xevent->type == PropertyNotify &&
       wm_window != None && xevent->xany.window == wm_window &&
       xevent->xproperty.atom == (XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "_NET_WM_NAME", False))))
    {
      update_wm_window ();
      (* ncb_data->func) ((gpointer) wm_common_get_current_window_manager (), ncb_data->data);
    }

  return GDK_FILTER_CONTINUE;
}

gpointer
wm_common_register_window_manager_change (GFunc    func,
                                          gpointer data)
{
  WMCallbackData *ncb_data;

  ncb_data = g_new0 (WMCallbackData, 1);

  ncb_data->func = func;
  ncb_data->data = data;

  gdk_window_add_filter (NULL, wm_window_event_filter, ncb_data);

  update_wm_window ();

  XSelectInput (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), GDK_ROOT_WINDOW (), PropertyChangeMask);
  XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), False);

  return ncb_data;
}

void
wm_common_unregister_window_manager_change (gpointer id)
{
  g_return_if_fail (id != NULL);

  gdk_window_remove_filter (NULL, wm_window_event_filter, id);
  g_free (id);
}
