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

char*
wm_common_get_current_window_manager (void)
{
  Atom utf8_string, atom, type;
  int result;
  char *retval;
  int format;
  gulong nitems;
  gulong bytes_after;
  guchar *val;

  if (wm_window == None)
      return WM_COMMON_UNKNOWN;

  utf8_string = XInternAtom (GDK_DISPLAY (), "UTF8_STRING", False);
  atom = XInternAtom (GDK_DISPLAY (), "_NET_WM_NAME", False);

  gdk_error_trap_push ();

  result = XGetWindowProperty (GDK_DISPLAY (),
		  	       wm_window,
			       atom,
			       0, G_MAXLONG,
			       False, utf8_string,
			       &type, &format, &nitems,
			       &bytes_after, (guchar **)&val);

  if (gdk_error_trap_pop () || result != Success)
    return WM_COMMON_UNKNOWN;

  if (type != utf8_string ||
      format !=8 ||
      nitems == 0)
    {
      if (val)
        XFree (val);
      return WM_COMMON_UNKNOWN;
    }

  if (!g_utf8_validate (val, nitems, NULL))
    {
      XFree (val);
      return WM_COMMON_UNKNOWN;
    }

  retval = g_strndup (val, nitems);

  XFree (val);

  return retval;
}

static void
update_wm_window (void)
{
  Window *xwindow;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;

  XGetWindowProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
		      XInternAtom (GDK_DISPLAY (), "_NET_SUPPORTING_WM_CHECK", False),
		      0, G_MAXLONG, False, XA_WINDOW, &type, &format,
		      &nitems, &bytes_after, (guchar **) &xwindow);

  if (type != XA_WINDOW)
    {
      wm_window = None;
     return;
    }

  gdk_error_trap_push ();
  XSelectInput (GDK_DISPLAY (), *xwindow, StructureNotifyMask | PropertyChangeMask);
  XSync (GDK_DISPLAY (), False);

  if (gdk_error_trap_pop ())
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
       xevent->xproperty.atom == (XInternAtom (GDK_DISPLAY (),  "_NET_SUPPORTING_WM_CHECK", False))) ||
      (xevent->type == PropertyNotify &&
       wm_window != None && xevent->xany.window == wm_window &&
       xevent->xproperty.atom == (XInternAtom (GDK_DISPLAY (), "_NET_WM_NAME", False))))
    {
      update_wm_window ();
      (* ncb_data->func) ((gpointer)wm_common_get_current_window_manager(),
		   	  ncb_data->data);
    }

  return GDK_FILTER_CONTINUE;
} 

void
wm_common_register_window_manager_change (GFunc    func,
					  gpointer data)
{
  WMCallbackData *ncb_data;

  ncb_data = g_new0 (WMCallbackData, 1);

  ncb_data->func = func;
  ncb_data->data = data;  

  gdk_window_add_filter (NULL, wm_window_event_filter, ncb_data);

  update_wm_window ();

  XSelectInput (GDK_DISPLAY (), GDK_ROOT_WINDOW (), PropertyChangeMask);
  XSync (GDK_DISPLAY (), False);
}


