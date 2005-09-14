#include <string.h>
#include <math.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <X11/keysym.h>

#include <gconf/gconf.h>

#include "gnome-settings-locate-pointer.h"
#include "gnome-settings-daemon.h"

#define DEFAULT_PTR_MAP_SIZE 128

static void
set_left_handed (gboolean left_handed)
{
  unsigned char *buttons;
  gint n_buttons, i;
  gint idx_1 = 0, idx_3 = 1;

  buttons = g_alloca (DEFAULT_PTR_MAP_SIZE);
  n_buttons = XGetPointerMapping (GDK_DISPLAY (), buttons, DEFAULT_PTR_MAP_SIZE);
  if (n_buttons > DEFAULT_PTR_MAP_SIZE) {
    buttons = g_alloca (n_buttons);
    n_buttons = XGetPointerMapping (GDK_DISPLAY (), buttons, n_buttons);
  }

  for (i = 0; i < n_buttons; i++)
    {
      if (buttons[i] == 1)
	idx_1 = i;
      else if (buttons[i] == ((n_buttons < 3) ? 2 : 3))
	idx_3 = i;
    }

  if ((left_handed && idx_1 < idx_3) ||
      (!left_handed && idx_1 > idx_3))
    {
      buttons[idx_1] = ((n_buttons < 3) ? 2 : 3);
      buttons[idx_3] = 1;
    }

  XSetPointerMapping (GDK_DISPLAY (), buttons, n_buttons);
}

static void
set_motion_acceleration (gfloat motion_acceleration)
{
  gint numerator, denominator;

  if (motion_acceleration >= 1.0)
    {
      /* we want to get the acceleration, with a resolution of 0.5
       */
      if ((motion_acceleration - floor (motion_acceleration)) < 0.25)
	{
	  numerator = floor (motion_acceleration);
	  denominator = 1;
	}
      else if ((motion_acceleration - floor (motion_acceleration)) < 0.5)
	{
	  numerator = ceil (2.0 * motion_acceleration);
	  denominator = 2;
	}
      else if ((motion_acceleration - floor (motion_acceleration)) < 0.75)
	{
	  numerator = floor (2.0 *motion_acceleration);
	  denominator = 2;
	}
      else
	{
	  numerator = ceil (motion_acceleration);
	  denominator = 1;
	}
    }
  else if (motion_acceleration < 1.0 && motion_acceleration > 0)
    {
      /* This we do to 1/10ths */
      numerator = floor (motion_acceleration * 10) + 1;
      denominator= 10;
    }
  else
    {
      numerator = -1;
      denominator = -1;
    }

  XChangePointerControl (GDK_DISPLAY (), True, False,
			 numerator, denominator,
			 0);
}

static void
set_motion_threshold (gint motion_threshold)
{
  XChangePointerControl (GDK_DISPLAY (), False, True,
			 0, 0, motion_threshold);
}


#define KEYBOARD_GROUP_SHIFT 13
#define KEYBOARD_GROUP_MASK ((1 << 13) | (1 << 14))

/* Owen magic */
static GdkFilterReturn
filter (GdkXEvent *xevent,
	GdkEvent  *event,
	gpointer   data)
{
  XEvent *xev = (XEvent *) xevent;
  guint keyval;
  gint group;

  GdkScreen *screen = (GdkScreen *)data;
	
  if (xev->type == KeyPress ||
      xev->type == KeyRelease)
    {
      /* get the keysym */
      group = (xev->xkey.state & KEYBOARD_GROUP_MASK) >> KEYBOARD_GROUP_SHIFT;
      gdk_keymap_translate_keyboard_state (gdk_keymap_get_default (),
					   xev->xkey.keycode,
					   xev->xkey.state,
					   group,
					   &keyval,
					   NULL, NULL, NULL);
      if (keyval == GDK_Control_L || keyval == GDK_Control_R)
	{
	  if (xev->type == KeyPress)
	    {
	      XAllowEvents (gdk_x11_get_default_xdisplay (),
			    SyncKeyboard,
			    xev->xkey.time);
	    }
	  else
	    {
	      XAllowEvents (gdk_x11_get_default_xdisplay (),
			    AsyncKeyboard,
			    xev->xkey.time);
	      gnome_settings_locate_pointer (screen);
	    }
	}
      else
	{
	  XAllowEvents (gdk_x11_get_default_xdisplay (),
			ReplayKeyboard,
			xev->xkey.time);
	  XUngrabKeyboard (gdk_x11_get_default_xdisplay (),
			   xev->xkey.time);
	}

      return GDK_FILTER_REMOVE;
    }
  return GDK_FILTER_CONTINUE;
}

static void
set_locate_pointer (gboolean locate_pointer)
{
  GdkKeymapKey *keys;
  GdkDisplay *display;
  int n_screens;
  int n_keys;
  gboolean has_entries;
  static const guint keyvals[] = { GDK_Control_L, GDK_Control_R };
  unsigned j;

  display = gdk_display_get_default ();
  n_screens = gdk_display_get_n_screens (display);

  for (j = 0 ; j < G_N_ELEMENTS (keyvals) ; j++) {
    has_entries = gdk_keymap_get_entries_for_keyval (gdk_keymap_get_default (),
							   keyvals[j],
							   &keys,
							   &n_keys);
    if (has_entries)
    {
      gint i, j;

      for (i = 0; i < n_keys; i++)
	{
	  for(j=0; j< n_screens; j++)
	    {
		GdkScreen *screen = gdk_display_get_screen (display, j);
		Window xroot = gdk_x11_drawable_get_xid (gdk_screen_get_root_window (screen));

	      if (locate_pointer) 
	        {
	          XGrabKey (GDK_DISPLAY_XDISPLAY (display),
		    	    keys[i].keycode,
			    0,
			    xroot,
			    False,
			    GrabModeAsync,
			    GrabModeSync);
	          XGrabKey (GDK_DISPLAY_XDISPLAY (display),
			    keys[i].keycode,
			    LockMask,
			    xroot,
			    False,
			    GrabModeAsync,
			    GrabModeSync);
	          XGrabKey (GDK_DISPLAY_XDISPLAY (display),
			    keys[i].keycode,
			    Mod2Mask,
			    xroot,
			    False,
			    GrabModeAsync,
			    GrabModeSync);
	          XGrabKey (GDK_DISPLAY_XDISPLAY (display),
			    keys[i].keycode,
			    Mod4Mask,
			    xroot,
			    False,
			    GrabModeAsync,
			    GrabModeSync);
	        }
	      else 
	        {
	          XUngrabKey (GDK_DISPLAY_XDISPLAY (display),
			      keys[i].keycode,
			      Mod4Mask,
			      xroot);
	          XUngrabKey (GDK_DISPLAY_XDISPLAY (display),
			      keys[i].keycode,
			      Mod2Mask,
			      xroot);
	          XUngrabKey (GDK_DISPLAY_XDISPLAY (display),
			      keys[i].keycode,
			      LockMask,
			      xroot);
	          XUngrabKey (GDK_DISPLAY_XDISPLAY (display),
			      keys[i].keycode,
			      0,
			      xroot);
	        }
	    }
	}
      g_free (keys);
      if (locate_pointer)
	{
	  for (i = 0; i < n_screens; i++)
	    {
		GdkScreen *screen;
		screen = gdk_display_get_screen (display, i);
		gdk_window_add_filter (gdk_screen_get_root_window (screen),
				       filter,
				       screen);
	    }
	}        
      else
	{
	  for (i = 0; i < n_screens; i++)
	    {
		GdkScreen *screen;
		screen = gdk_display_get_screen (display, i);
		gdk_window_remove_filter (gdk_screen_get_root_window (screen),
					  filter,
					  screen);
	    }
	}        
    }
  }
}

static void
mouse_callback (GConfEntry *entry)
{
  if (! strcmp (entry->key, "/desktop/gnome/peripherals/mouse/left_handed"))
    {
      if (entry->value->type == GCONF_VALUE_BOOL)
	set_left_handed (gconf_value_get_bool (entry->value));
    }
  else if (! strcmp (entry->key, "/desktop/gnome/peripherals/mouse/motion_acceleration"))
    {
      if (entry->value->type == GCONF_VALUE_FLOAT)
	set_motion_acceleration (gconf_value_get_float (entry->value));
    }
  else if (! strcmp (entry->key, "/desktop/gnome/peripherals/mouse/motion_threshold"))
    {
      if (entry->value->type == GCONF_VALUE_INT)
	set_motion_threshold (gconf_value_get_int (entry->value));
    }
  else if (! strcmp (entry->key, "/desktop/gnome/peripherals/mouse/locate_pointer"))
    {
      if (entry->value->type == GCONF_VALUE_BOOL)
	set_locate_pointer (gconf_value_get_bool (entry->value));
    }
}

void
gnome_settings_mouse_init (GConfClient *client)
{
  gnome_settings_daemon_register_callback ("/desktop/gnome/peripherals/mouse", mouse_callback);
}


void
gnome_settings_mouse_load (GConfClient *client)
{
  set_left_handed (gconf_client_get_bool (client, "/desktop/gnome/peripherals/mouse/left_handed", NULL));
  set_motion_acceleration (gconf_client_get_float (client, "/desktop/gnome/peripherals/mouse/motion_acceleration", NULL));
  set_motion_threshold (gconf_client_get_int (client, "/desktop/gnome/peripherals/mouse/motion_threshold", NULL));
  set_locate_pointer (gconf_client_get_bool (client, "/desktop/gnome/peripherals/mouse/locate_pointer", NULL));
}
