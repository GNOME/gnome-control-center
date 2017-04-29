/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * vim: set et sw=8 ts=8:
 *
 * Copyright (c) 2008, Novell, Inc.
 * Copyright (c) 2012, Red Hat, Inc.
 *
 * Authors: Vincent Untz <vuntz@gnome.org>
 * Bastien Nocera <hadess@hadess.net>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <glib.h>
#include <gio/gio.h>

#include "gsd-disk-space-helper.h"

gboolean
gsd_should_ignore_unix_mount (GUnixMountEntry *mount)
{
        const char *fs, *device;
        guint i;

        /* This is borrowed from GLib and used as a way to determine
         * which mounts we should ignore by default. GLib doesn't
         * expose this in a way that allows it to be used for this
         * purpose
         */

         /* We also ignore network filesystems */

        const gchar *ignore_fs[] = {
                "adfs",
                "afs",
                "auto",
                "autofs",
                "autofs4",
                "cifs",
                "cxfs",
                "devfs",
                "devpts",
                "ecryptfs",
                "fdescfs",
                "gfs",
                "gfs2",
                "gpfs",
                "kernfs",
                "linprocfs",
                "linsysfs",
                "lustre",
                "lustre_lite",
                "ncpfs",
                "nfs",
                "nfs4",
                "nfsd",
                "ocfs2",
                "proc",
                "procfs",
                "ptyfs",
                "rootfs",
                "rpc_pipefs",
                "selinuxfs",
                "smbfs",
                "sysfs",
                "tmpfs",
                "usbfs",
                "zfs",
                NULL
        };
        const gchar *ignore_devices[] = {
                "none",
                "sunrpc",
                "devpts",
                "nfsd",
                "/dev/loop",
                "/dev/vn",
                NULL
        };

        fs = g_unix_mount_get_fs_type (mount);
        device = g_unix_mount_get_device_path (mount);

        for (i = 0; ignore_fs[i] != NULL; i++)
                if (g_str_equal (ignore_fs[i], fs))
                        return TRUE;

        for (i = 0; ignore_devices[i] != NULL; i++)
                if (g_str_equal (ignore_devices[i], device))
                        return TRUE;

        return FALSE;
}

gboolean
gsd_is_removable_mount (GUnixMountEntry *mount)
{
        const char *mount_path;
        char *path;

        mount_path = g_unix_mount_get_mount_path (mount);
        if (mount_path == NULL)
                return FALSE;

        path = g_strdup_printf ("/run/media/%s", g_get_user_name ());
        if (g_str_has_prefix (mount_path, path)) {
                g_free (path);
                return TRUE;
        }
        g_free (path);
        return FALSE;
}
