#include <config.h>

#include <string.h>
#include <math.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <X11/keysym.h>

#ifdef HAVE_XINPUT
#include <X11/extensions/XInput.h>
#endif

#include <gconf/gconf.h>

#include "gnome-settings-locate-pointer.h"
#include "gnome-settings-module.h"

typedef struct {
	GnomeSettingsModule parent;
} GnomeSettingsModuleMouse;

typedef struct {
	GnomeSettingsModuleClass parent_class;
} GnomeSettingsModuleMouseClass;

static GnomeSettingsModuleRunlevel gnome_settings_module_mouse_get_runlevel (GnomeSettingsModule *module);
static gboolean gnome_settings_module_mouse_initialize (GnomeSettingsModule *module, GConfClient *config_client);
static gboolean gnome_settings_module_mouse_start (GnomeSettingsModule *module);
static gboolean gnome_settings_module_mouse_stop (GnomeSettingsModule *module);

static void
gnome_settings_module_mouse_class_init (GnomeSettingsModuleMouseClass *klass)
{
	GnomeSettingsModuleClass *module_class;

	module_class = (GnomeSettingsModuleClass *) klass;
	module_class->get_runlevel = gnome_settings_module_mouse_get_runlevel;
	module_class->initialize = gnome_settings_module_mouse_initialize;
	module_class->start = gnome_settings_module_mouse_start;
	module_class->stop = gnome_settings_module_mouse_stop;
}

static void
gnome_settings_module_mouse_init (GnomeSettingsModuleMouse *module)
{
}

GType
gnome_settings_module_mouse_get_type (void)
{
	static GType module_type = 0;
  
	if (!module_type) {
		static const GTypeInfo module_info = {
			sizeof (GnomeSettingsModuleMouseClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gnome_settings_module_mouse_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GnomeSettingsModuleMouse),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gnome_settings_module_mouse_init,
		};
      
		module_type = g_type_register_static (GNOME_SETTINGS_TYPE_MODULE,
						      "GnomeSettingsModuleMouse",
						      &module_info, 0);
	}
  
	return module_type;
}

static GnomeSettingsModuleRunlevel
gnome_settings_module_mouse_get_runlevel (GnomeSettingsModule *module)
{
	return GNOME_SETTINGS_MODULE_RUNLEVEL_XSETTINGS;
}

#ifdef HAVE_XINPUT
static gboolean
supports_xinput_devices (void)
{
	gint op_code, event, error;

	return XQueryExtension (GDK_DISPLAY (), "XInputExtension",
				&op_code, &event, &error);
}
#endif

static void
configure_button_layout (guchar   *buttons,
                         gint      n_buttons,
                         gboolean  left_handed)
{
	const gint left_button = 1;
	gint right_button;
	gint i;

	/* if the button is higher than 2 (3rd button) then it's
	 * probably one direction of a scroll wheel or something else
	 * uninteresting
	 */
	right_button = MIN (n_buttons, 3);

	/* If we change things we need to make sure we only swap buttons.
	 * If we end up with multiple physical buttons assigned to the same
	 * logical button the server will complain. This code assumes physical
	 * button 0 is the physical left mouse button, and that the physical
	 * button other than 0 currently assigned left_button or right_button
	 * is the physical right mouse button.
	 */

	/* check if the current mapping satisfies the above assumptions */
	if (buttons[left_button - 1] != left_button && 
	    buttons[left_button - 1] != right_button)
		/* The current mapping is weird. Swapping buttons is probably not a
		 * good idea.
		 */
		return;

	/* check if we are left_handed and currently not swapped */
	if (left_handed && buttons[left_button - 1] == left_button) {
		/* find the right button */
		for (i = 0; i < n_buttons; i++) {
			if (buttons[i] == right_button) {
				buttons[i] = left_button;
				break;
			}
		}
		/* swap the buttons */
		buttons[left_button - 1] = right_button;
	}
	/* check if we are not left_handed but are swapped */
	else if (!left_handed && buttons[left_button - 1] == right_button) {
		/* find the right button */
		for (i = 0; i < n_buttons; i++) {
			if (buttons[i] == left_button) {
				buttons[i] = right_button;
				break;
			}
		}
		/* swap the buttons */
		buttons[left_button - 1] = left_button;
	}
}

#ifdef HAVE_XINPUT
static gboolean
xinput_device_has_buttons (XDeviceInfo *device_info)
{
	int i;
	XAnyClassInfo *class_info;

	class_info = device_info->inputclassinfo;
	for (i = 0; i < device_info->num_classes; i++) {
		if (class_info->class == ButtonClass) {
			XButtonInfo *button_info;

			button_info = (XButtonInfo *) class_info;
			if (button_info->num_buttons > 0)
				return TRUE;
		}

		class_info = (XAnyClassInfo *) (((guchar *) class_info) + 
						class_info->length);
	}
	return FALSE;
}

static void
set_xinput_devices_left_handed (gboolean left_handed)
{
	XDeviceInfo *device_info;
	gint n_devices;
	guchar *buttons;
	gsize buttons_capacity = 16;
	gint n_buttons;
	gint i;

	device_info = XListInputDevices (GDK_DISPLAY (), &n_devices);

	if (n_devices > 0)
		buttons = g_new (guchar, buttons_capacity);
	else
		buttons = NULL;
   
	for (i = 0; i < n_devices; i++) {
		XDevice *device = NULL;

		if ((device_info[i].use != IsXExtensionDevice) ||
		    (!xinput_device_has_buttons (&device_info[i])))
			continue;

		gdk_error_trap_push ();

		device = XOpenDevice (GDK_DISPLAY (), device_info[i].id);

		if ((gdk_error_trap_pop () != 0) ||
		    (device == NULL))
			continue;

		n_buttons = XGetDeviceButtonMapping (GDK_DISPLAY (), device,
						     buttons, 
						     buttons_capacity);

		while (n_buttons > buttons_capacity) {
			buttons_capacity = n_buttons;
			buttons = (guchar *) g_realloc (buttons, 
							buttons_capacity * sizeof (guchar));

			n_buttons = XGetDeviceButtonMapping (GDK_DISPLAY (), device,
							     buttons, 
							     buttons_capacity);
		}

		configure_button_layout (buttons, n_buttons, left_handed);
      
		XSetDeviceButtonMapping (GDK_DISPLAY (), device, buttons, n_buttons);
		XCloseDevice (GDK_DISPLAY (), device);
	}
	g_free (buttons);

	if (device_info != NULL)
		XFreeDeviceList (device_info);
}
#endif

static void
set_left_handed (gboolean left_handed)
{
	guchar *buttons ;
	gsize buttons_capacity = 16;
	gint n_buttons, i;

#ifdef HAVE_XINPUT
	if (supports_xinput_devices ())
		set_xinput_devices_left_handed (left_handed);
#endif

	buttons = g_new (guchar, buttons_capacity);
	n_buttons = XGetPointerMapping (GDK_DISPLAY (), buttons, 
					(gint) buttons_capacity);
	while (n_buttons > buttons_capacity) {
		buttons_capacity = n_buttons;
		buttons = (guchar *) g_realloc (buttons, 
						buttons_capacity * sizeof (guchar));

		n_buttons = XGetPointerMapping (GDK_DISPLAY (), buttons,
						(gint) buttons_capacity);
	}

	configure_button_layout (buttons, n_buttons, left_handed);

	/* X refuses to change the mapping while buttons are engaged,
	 * so if this is the case we'll retry a few times
	 */
	for (i = 0;
	     i < 20 && XSetPointerMapping (GDK_DISPLAY (), buttons, n_buttons) == MappingBusy;
	     ++i) {
		g_usleep (300);
	}

	g_free (buttons);
}

static void
set_motion_acceleration (gfloat motion_acceleration)
{
	gint numerator, denominator;

	if (motion_acceleration >= 1.0) {
		/* we want to get the acceleration, with a resolution of 0.5
		 */
		if ((motion_acceleration - floor (motion_acceleration)) < 0.25) {
			numerator = floor (motion_acceleration);
			denominator = 1;
		} else if ((motion_acceleration - floor (motion_acceleration)) < 0.5) {
			numerator = ceil (2.0 * motion_acceleration);
			denominator = 2;
		} else if ((motion_acceleration - floor (motion_acceleration)) < 0.75) {
			numerator = floor (2.0 *motion_acceleration);
			denominator = 2;
		} else {
			numerator = ceil (motion_acceleration);
			denominator = 1;
		}
	} else if (motion_acceleration < 1.0 && motion_acceleration > 0) {
		/* This we do to 1/10ths */
		numerator = floor (motion_acceleration * 10) + 1;
		denominator= 10;
	} else {
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
	    xev->type == KeyRelease) {
		/* get the keysym */
		group = (xev->xkey.state & KEYBOARD_GROUP_MASK) >> KEYBOARD_GROUP_SHIFT;
		gdk_keymap_translate_keyboard_state (gdk_keymap_get_default (),
						     xev->xkey.keycode,
						     xev->xkey.state,
						     group,
						     &keyval,
						     NULL, NULL, NULL);
		if (keyval == GDK_Control_L || keyval == GDK_Control_R) {
			if (xev->type == KeyPress) {
				XAllowEvents (gdk_x11_get_default_xdisplay (),
					      SyncKeyboard,
					      xev->xkey.time);
			} else {
				XAllowEvents (gdk_x11_get_default_xdisplay (),
					      AsyncKeyboard,
					      xev->xkey.time);
				gnome_settings_locate_pointer (screen);
			}
		} else {
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
		if (has_entries) {
			gint i, j;

			for (i = 0; i < n_keys; i++) {
				for(j=0; j< n_screens; j++) {
					GdkScreen *screen = gdk_display_get_screen (display, j);
					Window xroot = gdk_x11_drawable_get_xid (gdk_screen_get_root_window (screen));

					if (locate_pointer) {
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
					} else {
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
			if (locate_pointer) {
				for (i = 0; i < n_screens; i++) {
					GdkScreen *screen;
					screen = gdk_display_get_screen (display, i);
					gdk_window_add_filter (gdk_screen_get_root_window (screen),
							       filter,
							       screen);
				}
			} else {
				for (i = 0; i < n_screens; i++) {
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
	if (! strcmp (entry->key, "/desktop/gnome/peripherals/mouse/left_handed")) {
		if (entry->value->type == GCONF_VALUE_BOOL)
			set_left_handed (gconf_value_get_bool (entry->value));
	} else if (! strcmp (entry->key, "/desktop/gnome/peripherals/mouse/motion_acceleration")) {
		if (entry->value->type == GCONF_VALUE_FLOAT)
			set_motion_acceleration (gconf_value_get_float (entry->value));
	} else if (! strcmp (entry->key, "/desktop/gnome/peripherals/mouse/motion_threshold")) {
		if (entry->value->type == GCONF_VALUE_INT)
			set_motion_threshold (gconf_value_get_int (entry->value));
	} else if (! strcmp (entry->key, "/desktop/gnome/peripherals/mouse/locate_pointer")) {
		if (entry->value->type == GCONF_VALUE_BOOL)
			set_locate_pointer (gconf_value_get_bool (entry->value));
	}
}

static gboolean
gnome_settings_module_mouse_initialize (GnomeSettingsModule *module, GConfClient *config_client)
{
	gnome_settings_register_config_callback ("/desktop/gnome/peripherals/mouse", mouse_callback);

	return TRUE;
}


static gboolean
gnome_settings_module_mouse_start (GnomeSettingsModule *module)
{
	GConfClient *client = gnome_settings_module_get_config_client (module);

	set_left_handed (gconf_client_get_bool (client, "/desktop/gnome/peripherals/mouse/left_handed", NULL));
	set_motion_acceleration (gconf_client_get_float (client,
							 "/desktop/gnome/peripherals/mouse/motion_acceleration", NULL));
	set_motion_threshold (gconf_client_get_int (client,
						    "/desktop/gnome/peripherals/mouse/motion_threshold", NULL));
	set_locate_pointer (gconf_client_get_bool (client,
						   "/desktop/gnome/peripherals/mouse/locate_pointer", NULL));

	return TRUE;
}

static gboolean
gnome_settings_module_mouse_stop (GnomeSettingsModule *module)
{
	return TRUE;
}
