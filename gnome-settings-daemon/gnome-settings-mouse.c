#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gconf/gconf.h>
#include <math.h>

#define MAX_BUTTONS 10

static void
set_left_handed (gboolean left_handed)
{
  unsigned char buttons[MAX_BUTTONS];
  gint n_buttons, i;
  gint idx_1 = 0, idx_3 = 1;

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
set_drag_threshold (gint drag_threshold)
{

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
}

void
gnome_settings_mouse_init (GConfEngine *engine)
{
  gnome_settings_daemon_register_callback ("/desktop/gnome/peripherals/mouse", mouse_callback);
}


void
gnome_settings_mouse_load (GConfEngine *engine)
{

}
