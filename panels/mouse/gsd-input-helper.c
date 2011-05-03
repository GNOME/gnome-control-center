/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <sys/types.h>
#include <X11/Xatom.h>

#include "gsd-input-helper.h"

#define INPUT_DEVICES_SCHEMA "org.gnome.settings-daemon.peripherals.input-devices"
#define KEY_HOTPLUG_COMMAND  "hotplug-command"

gboolean
supports_xinput_devices (void)
{
        gint op_code, event, error;

        return XQueryExtension (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                "XInputExtension",
                                &op_code,
                                &event,
                                &error);
}

gboolean
device_is_touchpad (XDevice *xdevice)
{
        Atom realtype, prop;
        int realformat;
        unsigned long nitems, bytes_after;
        unsigned char *data;

	/* FIXME
	 * we don't check on the type being XI_TOUCHPAD, but having a "Synaptics Off"
	 * property should be enough */

        prop = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "Synaptics Off", False);
        if (!prop)
                return FALSE;

        gdk_error_trap_push ();
        if ((XGetDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), xdevice, prop, 0, 1, False,
                                XA_INTEGER, &realtype, &realformat, &nitems,
                                &bytes_after, &data) == Success) && (realtype != None)) {
                gdk_error_trap_pop_ignored ();
                XFree (data);
                return TRUE;
        }
        gdk_error_trap_pop_ignored ();

        return FALSE;
}

gboolean
touchpad_is_present (void)
{
        XDeviceInfo *device_info;
        gint n_devices;
        guint i;
        gboolean retval;

        if (supports_xinput_devices () == FALSE)
                return TRUE;

        retval = FALSE;

        device_info = XListInputDevices (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), &n_devices);
        if (device_info == NULL)
                return FALSE;

        for (i = 0; i < n_devices; i++) {
                XDevice *device;

                gdk_error_trap_push ();
                device = XOpenDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device_info[i].id);
                if (gdk_error_trap_pop () || (device == NULL))
                        continue;

                retval = device_is_touchpad (device);
                if (retval) {
                        XCloseDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device);
                        break;
                }

                XCloseDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device);
        }
        XFreeDeviceList (device_info);

        return retval;
}

static const char *
custom_command_to_string (CustomCommand command)
{
        switch (command) {
        case COMMAND_DEVICE_ADDED:
                return "added";
        case COMMAND_DEVICE_REMOVED:
                return "removed";
        case COMMAND_DEVICE_PRESENT:
                return "present";
        default:
                g_assert_not_reached ();
        }
}

/* Run a custom command on device presence events. Parameters passed into
 * the custom command are:
 * command -t [added|removed|present] -i <device ID> <device name>
 * Type 'added' and 'removed' signal 'device added' and 'device removed',
 * respectively. Type 'present' signals 'device present at
 * gnome-settings-daemon init'.
 *
 * The script is expected to run synchronously, and an exit value
 * of "1" means that no other settings will be applied to this
 * particular device.
 *
 * More options may be added in the future.
 *
 * This function returns TRUE if we should not apply any more settings
 * to the device.
 */
gboolean
run_custom_command (GdkDevice              *device,
                    CustomCommand           command)
{
        GSettings *settings;
        char *cmd;
        char *argv[5];
        int exit_status;
        gboolean rc;
        int id;

        settings = g_settings_new (INPUT_DEVICES_SCHEMA);
        cmd = g_settings_get_string (settings, KEY_HOTPLUG_COMMAND);
        g_object_unref (settings);

        if (!cmd || cmd[0] == '\0') {
                g_free (cmd);
                return FALSE;
        }

        /* Easter egg! */
        g_object_get (device, "device-id", &id, NULL);

        argv[0] = cmd;
        argv[1] = g_strdup_printf ("-t %s", custom_command_to_string (command));
        argv[2] = g_strdup_printf ("-i %d", id);
        argv[3] = g_strdup_printf ("%s", gdk_device_get_name (device));
        argv[4] = NULL;

        rc = g_spawn_sync (g_get_home_dir (), argv, NULL, G_SPAWN_SEARCH_PATH,
                           NULL, NULL, NULL, NULL, &exit_status, NULL);

        if (rc == FALSE)
                g_warning ("Couldn't execute command '%s', verify that this is a valid command.", cmd);

        g_free (argv[0]);
        g_free (argv[1]);
        g_free (argv[2]);

        return (exit_status == 0);
}
