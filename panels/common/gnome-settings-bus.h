/* Stub to replace gnome-settings-daemon's
 * gnome-settings-bus.h helpers */

#include <gdk/gdkx.h>

#ifdef GDK_WINDOWING_WAYLAND

static inline gboolean
gnome_settings_is_wayland (void)
{
  return !GDK_IS_X11_DISPLAY (gdk_display_get_default ());
}

#endif /* GDK_WINDOWING_WAYLAND */
