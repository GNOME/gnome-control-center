#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gconf/gconf.h>
#include <math.h>

#include "gnome-settings-daemon.h"

#define MAX_BUTTONS 10

#if 0
GdkWindow *window = NULL;

static gint
locate_pointer_expose (GtkWidget *widget,
		       GdkExposeEvent *event,
		       gpointer data)
{
}


static void
create_window (void)
{
	  GdkWindowAttr attributes;
	  attributes.window_type = GDK_WINDOW_CHILD;
	  attributes.wclass = GDK_INPUT_OUTPUT;
	  attributes.visual = gtk_widget_get_visual (GTK_WIDGET (tree_view));
	  attributes.colormap = gtk_widget_get_colormap (GTK_WIDGET (tree_view));
	  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK;
	  window = gdk_window_new (gdk_get_default_root_window (),
				   &attributes,
				   GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP);
	  gdk_window_set_user_data (tree_view->priv->drag_highlight_window, gnome_settings_daemon_get_invisible ());
	  g_signal_connect (G_OBJECT (gnome_settings_daemon_get_invisible ()),
			    "expose_event",
			    locate_pointer_expose,
			    NULL);
}

static void
locate_pointer (void)
{
	GtkWidget *window;
	gint cursor_x, cursor_y;

	window = gtk_window_new (GTK_WINDOW_POPUP);
	gdk_window_get_pointer (NULL, &cursor_x, &cursor_y, NULL);
	
	if (window == NULL)
		create_window ();

}
#endif




static void
set_left_handed (gboolean left_handed)
{
  unsigned char buttons[MAX_BUTTONS];
  gint n_buttons, i;
  gint idx_1 = 0, idx_3 = 1;

  g_print ("daemon: set_left_handed %d\n", left_handed);
  n_buttons = XGetPointerMapping (GDK_DISPLAY (), buttons, MAX_BUTTONS);

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



static void
mouse_callback (GConfEntry *entry)
{
	g_print ("daemon: gconf callback %s\n", entry->key);
  if (! strcmp (entry->key, "/desktop/gnome/peripherals/mouse/left_handed"))
    {
      if (entry->value->type == GCONF_VALUE_BOOL)
	set_left_handed (gconf_value_get_bool (entry->value));
      else
	      g_warning ("wrong type!\n");
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
}

void
gnome_settings_mouse_init (GConfClient *client)
{
  gnome_settings_daemon_register_callback ("/desktop/gnome/peripherals/mouse", mouse_callback);
}


void
gnome_settings_mouse_load (GConfClient *client)
{
}
