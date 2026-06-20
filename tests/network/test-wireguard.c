/* test-wireguard.c
 *
 * Copyright 2026 The GNOME Project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#undef NDEBUG
#undef G_DISABLE_ASSERT
#undef G_DISABLE_CHECKS

#include <NetworkManager.h>
#include <adwaita.h>
#include <gtk/gtk.h>

#include "connection-editor/ce-page-wireguard.h"
#include "connection-editor/ce-page.h"

/* Exported by the connection-editor gresource (export: true). Referencing it
 * pulls in the resource object file, whose constructor auto-registers the
 * WireGuard page templates. */
extern GResource *net_connection_editor_get_resource (void);

/* A syntactically valid (base64, 32 byte) WireGuard public key. */
#define TEST_PUBKEY "dGVzdC1rZXktbXVzdC1iZS0zMi1ieXRlcy1sb25nISE="

static NMConnection *
create_wireguard_connection (void)
{
    NMConnection *connection;
    NMSettingConnection *s_con;
    NMSettingWireGuard *s_wg;
    NMWireGuardPeer *peer;
    g_autofree char *uuid = NULL;

    connection = nm_simple_connection_new ();

    uuid = nm_utils_uuid_generate ();
    s_con = NM_SETTING_CONNECTION (nm_setting_connection_new ());
    g_object_set (s_con, NM_SETTING_CONNECTION_ID, "test-wireguard", NM_SETTING_CONNECTION_UUID, uuid,
                  NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIREGUARD_SETTING_NAME, NM_SETTING_CONNECTION_INTERFACE_NAME,
                  "wg-test", NULL);
    nm_connection_add_setting (connection, NM_SETTING (s_con));

    s_wg = NM_SETTING_WIREGUARD (nm_setting_wireguard_new ());
    peer = nm_wireguard_peer_new ();
    nm_wireguard_peer_set_public_key (peer, TEST_PUBKEY, FALSE);
    nm_setting_wireguard_append_peer (s_wg, peer);
    nm_wireguard_peer_unref (peer);
    nm_connection_add_setting (connection, NM_SETTING (s_wg));

    return connection;
}

/*
 * Creating and destroying the WireGuard editor page must not disturb the
 * reference counts of the connection it edits:
 *
 *  - dispose() must NOT unref the settings it borrows from the connection
 *    (nm_connection_get_setting*() is transfer-none), so they must still be
 *    alive right after the page is destroyed;
 *  - the page must release the reference it takes on the connection, so once
 *    the test drops its own reference the connection (and its settings) must
 *    be finalized rather than leaked.
 *
 * The peer loaded into the page also exercises the peer ref/unref balance;
 * any imbalance there is reported by the leak sanitizer.
 */
static void
test_page_lifecycle_refs (void)
{
    NMConnection *connection;
    CEPageWireguard *page;
    gpointer connection_weak, s_wg_weak, s_con_weak;

    connection = create_wireguard_connection ();

    connection_weak = connection;
    s_wg_weak = nm_connection_get_setting (connection, NM_TYPE_SETTING_WIREGUARD);
    s_con_weak = nm_connection_get_setting_connection (connection);

    g_object_add_weak_pointer (G_OBJECT (connection), &connection_weak);
    g_object_add_weak_pointer (G_OBJECT (s_wg_weak), &s_wg_weak);
    g_object_add_weak_pointer (G_OBJECT (s_con_weak), &s_con_weak);

    page = ce_page_wireguard_new (connection);
    g_object_ref_sink (page);
    ce_page_complete_init (CE_PAGE (page), connection, NULL, NULL, NULL);

    g_object_unref (page);

    /* Borrowed settings must survive the page being destroyed. */
    g_assert_nonnull (s_wg_weak);
    g_assert_nonnull (s_con_weak);

    /* The page must not have leaked a reference to the connection. */
    g_object_unref (connection);
    g_assert_null (connection_weak);
    g_assert_null (s_wg_weak);
    g_assert_null (s_con_weak);
}

int
main (int argc, char *argv[])
{
    g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
    g_setenv ("LC_ALL", "C", TRUE);

    gtk_test_init (&argc, &argv, NULL);
    adw_init ();
    g_assert_nonnull (net_connection_editor_get_resource ());

    g_test_add_func ("/network/wireguard/page-lifecycle-refs", test_page_lifecycle_refs);

    return g_test_run ();
}
