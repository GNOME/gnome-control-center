/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright 2010 - 2014, 2018 Red Hat, Inc.
 *
 */

#include "nm-macros-internal.h"

#include <NetworkManager.h>
#include <nm-client.h>
#include <nm-utils.h>

#include "nm-test-libnm-utils.h"

#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <gtk/gtk.h>

#include "shell/cc-test-window.h"

static NMTstcServiceInfo *sinfo;
static NMClient *client;

#include "test-helpers.h"

NMDevice *main_ether;


static GtkWidget *shell;
static CcPanel *panel;


/*****************************************************************************/

static void
test_device_add (void)
{
	const gchar *device_path;

	/* Tell the test service to add a new device.
	 * We use some weird numbers so that the devices will not exist on the
	 * host system. */
	main_ether = nmtstc_service_add_wired_device (sinfo, client, "eth1000", "52:54:00:ab:db:23", NULL);
	device_path = nm_object_get_path (NM_OBJECT (main_ether));
	g_debug("Device added: %s\n", device_path);

	g_assert (gtk_test_find_label(shell, "Wired") != NULL);
}

/*****************************************************************************/

static void
test_second_device_add_remove (void)
{
	NMDevice *device;
	const gchar *device_path;

	device = nmtstc_service_add_wired_device (sinfo, client, "eth1001", "52:54:00:ab:db:24", NULL);
	device_path = nm_object_get_path (NM_OBJECT (device));
	g_debug("Second device added: %s\n", device_path);

	g_assert (gtk_test_find_label (shell, "Wired") == NULL);
	g_assert (gtk_test_find_label (shell, "Ethernet (eth1000)") != NULL);
	g_assert (gtk_test_find_label (shell, "Ethernet (eth1001)") != NULL);

	nmtst_remove_device (sinfo, device);
	g_debug("Second device removed again\n");

	g_assert (gtk_test_find_label (shell, "Wired") != NULL);
	g_assert (gtk_test_find_label(shell, "Ethernet (eth1000)") == NULL);
	g_assert (gtk_test_find_label(shell, "Ethernet (eth1001)") == NULL);
}


/*****************************************************************************/

static void
add_cb (GObject *object,
        GAsyncResult *result,
        gpointer user_data)
{
	NMClient *client = NM_CLIENT (object);
	EventWaitInfo *info = user_data;
	NMRemoteConnection *remote_conn;
	GError *error = NULL;

	g_debug("add_cb");
	remote_conn = nm_client_add_connection_finish (client, result, &error);
	g_assert_no_error (error);
	g_assert (remote_conn != NULL);
	g_object_unref (remote_conn);

	info->other_remaining--;
	WAIT_CHECK_REMAINING()
}


static void
test_connection_add (void)
{
	NMConnection *conn;
	GError *error = NULL;
	WAIT_DECL()

	conn = nmtst_create_minimal_connection ("test-inactive", NULL, NM_SETTING_WIRED_SETTING_NAME, NULL);
	nm_device_connection_compatible (main_ether, conn, &error);
	g_assert_no_error (error);

	nm_client_add_connection_async (client, conn, TRUE, NULL, add_cb, &info);

	info.other_remaining = 1;
	WAIT_CLIENT(client, 1, NM_CLIENT_CONNECTIONS);

	g_object_unref (conn);

	WAIT_FINISHED(5)

	/* We have one (non-active) connection only, so we get a special case */
	g_assert (gtk_test_find_label (shell, "Cable unplugged") != NULL);
}

/*****************************************************************************/

static void
test_unconnected_plug (void)
{
	nmtst_set_wired_speed (sinfo, main_ether, 1000);
	nmtst_set_device_state (sinfo, main_ether, NM_DEVICE_STATE_DISCONNECTED, NM_DEVICE_STATE_REASON_CARRIER);

	g_assert (gtk_test_find_label (shell, "1000 Mb/s") != NULL);
}

/*****************************************************************************/

static void
add_and_activate_cb (GObject *object,
                     GAsyncResult *result,
                     gpointer user_data)
{
	NMClient *client = NM_CLIENT (object);
	EventWaitInfo *info = user_data;
	GError *error = NULL;

	g_debug("add_and_activate_cb");
	info->ac = nm_client_add_and_activate_connection_finish (client, result, &error);
	g_assert_no_error (error);
	g_assert (info->ac != NULL);

	info->other_remaining--;
	WAIT_CHECK_REMAINING()
}

static void
test_connection_add_activate (void)
{
	NMConnection *conn;
	GError *error = NULL;
	WAIT_DECL()

	conn = nmtst_create_minimal_connection ("test-active", NULL, NM_SETTING_WIRED_SETTING_NAME, NULL);
	nm_device_connection_compatible (main_ether, conn, &error);
	g_assert_no_error (error);

	nm_client_add_and_activate_connection_async (client, conn, main_ether, NULL,
	                                             NULL, add_and_activate_cb, &info);

	info.other_remaining = 1;
	WAIT_CLIENT(client, 1, NM_CLIENT_ACTIVE_CONNECTIONS);
	WAIT_DEVICE(main_ether, 1, NM_DEVICE_ACTIVE_CONNECTION);

	g_object_unref (conn);

	WAIT_FINISHED(5)

	g_assert (info.ac != NULL);

	g_object_unref (info.ac);

	/* We now have two connections, one of them is active */
	g_assert (gtk_test_find_label (shell, "test-inactive") != NULL);
	g_assert (gtk_test_find_label (shell, "test-active") != NULL);
}

/*****************************************************************************/

static void
test_activated (void)
{
	nmtst_set_device_state (sinfo, main_ether, NM_DEVICE_STATE_ACTIVATED, NM_DEVICE_STATE_REASON_CARRIER);

	/* Once connected the hardware address will be shown in a label. */
	g_assert (gtk_test_find_label (shell, nm_device_get_hw_address (main_ether)) != NULL);
}


/*****************************************************************************/

extern GType cc_network_panel_get_type (void);

int
main (int argc, char **argv)
{
	GError *error = NULL;

	g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
	g_setenv ("LIBNM_USE_SESSION_BUS", "1", TRUE);
	g_setenv ("LC_ALL", "C", TRUE);

	gtk_test_init (&argc, &argv, NULL);

	/* Bring up the libnm service. */
	sinfo = nmtstc_service_init ();

	client = nm_client_new (NULL, &error);
	g_assert_no_error (error);

	shell = GTK_WIDGET (cc_test_window_new ());
	g_signal_connect (shell, "destroy", gtk_main_quit, NULL);

	panel = g_object_new (cc_network_panel_get_type (),
	                      "shell", CC_SHELL (shell),
	                      NULL);

	cc_shell_set_active_panel (CC_SHELL (shell), panel);
	gtk_widget_show_all (shell);

	g_test_add_func ("/network-panel/device-add", test_device_add);
	g_test_add_func ("/network-panel/second-device-add-remove", test_second_device_add_remove);
	g_test_add_func ("/network-panel/connection-add", test_connection_add);
	g_test_add_func ("/network-panel/unconnected-plug", test_unconnected_plug);
	g_test_add_func ("/network-panel/connection-add-activate", test_connection_add_activate);
	/* NOTE: The in between states are no different, is this a bug or should the connection
	 *       only be active after the state is activated? */
	g_test_add_func ("/network-panel/activated", test_activated);

	/* Use the following (in between tests) to see what is going on. */
/*
	g_test_run ();
	gtk_main();
	return 0;
*/
	return g_test_run ();
}

