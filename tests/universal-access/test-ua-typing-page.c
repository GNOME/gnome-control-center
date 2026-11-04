/*
 * Copyright (C) 2026 Owen Kent
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Tests for the accessible names of the sliders on the Typing page.
 *
 * The real CcUaTypingPage is instantiated and the widgets inside it are
 * located by their template ids. GTK_A11Y=test keeps the accessible state
 * in process, so no AT-SPI bus is needed.
 */

#include <adwaita.h>
#include <gtk/gtk.h>

#include "cc-ua-resources.h"
#include "cc-ua-typing-page.h"

typedef struct {
    const char *scale_id; /* Template id of the GtkScale */
    const char *label_id; /* Template id of the prefix label that names it */
    const char *name;     /* The name a screen reader announces for it */
} NamedScale;

/* The Repeat Keys rows show their titles in prefix labels, so that the two
 * sliders line up. The labels have to name the sliders and their rows as well. */
static const NamedScale named_scales[] = {
    { "repeat_speed_scale", "speed_label", "Speed" },
    { "repeat_delay_scale", "delay_label", "Delay" },
};

static GtkWidget *
find_by_id (GtkWidget *root, const char *id)
{
    GtkWidget *child;

    if (GTK_IS_BUILDABLE (root) && g_strcmp0 (gtk_buildable_get_buildable_id (GTK_BUILDABLE (root)), id) == 0)
        return root;

    for (child = gtk_widget_get_first_child (root); child != NULL; child = gtk_widget_get_next_sibling (child)) {
        GtkWidget *found = find_by_id (child, id);

        if (found != NULL)
            return found;
    }

    return NULL;
}

static void
test_accessible_name (gconstpointer user_data)
{
    const NamedScale *named = user_data;
    g_autoptr (GtkWidget) page = g_object_ref_sink (g_object_new (CC_TYPE_UA_TYPING_PAGE, NULL));
    GtkWidget *scale = find_by_id (page, named->scale_id);
    GtkWidget *label = find_by_id (page, named->label_id);
    GtkWidget *row;

    g_assert_true (GTK_IS_SCALE (scale));
    g_assert_true (GTK_IS_LABEL (label));
    g_assert_cmpstr (gtk_label_get_text (GTK_LABEL (label)), ==, named->name);

    gtk_test_accessible_assert_relation (scale, GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL);

    /* The row around the scale has no title of its own either. */
    row = gtk_widget_get_ancestor (scale, ADW_TYPE_ACTION_ROW);
    g_assert_nonnull (row);
    gtk_test_accessible_assert_relation (row, GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL);
}

int
main (int argc, char *argv[])
{
    guint i;

    g_test_init (&argc, &argv, NULL);

    if (!gtk_init_check ()) {
        g_print ("1..0 # SKIP No display available\n");
        return 0;
    }

    adw_init ();

    /* Normally done by CcUaPanel, which is not instantiated here. */
    g_resources_register (cc_universal_access_get_resource ());

    for (i = 0; i < G_N_ELEMENTS (named_scales); i++) {
        g_autofree char *path =
            g_strdup_printf ("/universal-access/typing-page/accessible-name/%s", named_scales[i].scale_id);

        g_test_add_data_func (path, &named_scales[i], test_accessible_name);
    }

    return g_test_run ();
}
