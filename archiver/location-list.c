/* -*- mode: c; style: linux -*- */

/* location-list.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
 *
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "location-list.h"

typedef struct _pair_t pair_t;

struct _pair_t 
{
	gpointer a, b;
};

enum {
	ARG_0,
	ARG_USER_ARCHIVE,
	ARG_GLOBAL_ARCHIVE,
	ARG_SEPARATE_LOCATIONS
};

struct _LocationListPrivate 
{
	gchar      *selected_location_id;
	Location   *selected_location;

	gboolean    separate_locations;
	Archive    *user_archive;
	Archive    *global_archive;
};

static GtkCTreeClass *parent_class;

static void location_list_init        (LocationList *location_list);
static void location_list_class_init  (LocationListClass *class);

static void location_list_set_arg     (GtkObject *object, 
				       GtkArg *arg, 
				       guint arg_id);
static void location_list_get_arg     (GtkObject *object, 
				       GtkArg *arg, 
				       guint arg_id);

static void select_row_cb             (LocationList *list,
				       GList *node, gint column);

static void location_list_finalize    (GtkObject *object);

static gint populate_locations_cb     (Archive *archive,
				       Location *location,
				       pair_t *data);
static void populate_locations_list   (LocationList *list,
				       gboolean do_global);

guint
location_list_get_type (void)
{
	static guint location_list_type = 0;

	if (!location_list_type) {
		GtkTypeInfo location_list_info = {
			"LocationList",
			sizeof (LocationList),
			sizeof (LocationListClass),
			(GtkClassInitFunc) location_list_class_init,
			(GtkObjectInitFunc) location_list_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		location_list_type = 
			gtk_type_unique (gtk_ctree_get_type (), 
					 &location_list_info);
	}

	return location_list_type;
}

static void
location_list_init (LocationList *location_list)
{
	static char *titles = { "Location" };

	gtk_ctree_construct (GTK_CTREE (location_list),
			     1, 0, &titles);

	gtk_clist_column_titles_hide (GTK_CLIST (location_list));

	location_list->p = g_new0 (LocationListPrivate, 1);

	gtk_signal_connect (GTK_OBJECT (location_list),
			    "tree-select-row", GTK_SIGNAL_FUNC (select_row_cb),
			    NULL);
}

static void
location_list_class_init (LocationListClass *class) 
{
	GtkObjectClass *object_class;

	gtk_object_add_arg_type ("LocationList::user-archive",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_USER_ARCHIVE);

	gtk_object_add_arg_type ("LocationList::global-archive",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_GLOBAL_ARCHIVE);

	gtk_object_add_arg_type ("LocationList::separate-locations",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_SEPARATE_LOCATIONS);

	object_class = GTK_OBJECT_CLASS (class);
	object_class->finalize = location_list_finalize;
	object_class->set_arg = location_list_set_arg;
	object_class->get_arg = location_list_get_arg;

	parent_class = GTK_CTREE_CLASS
		(gtk_type_class (gtk_ctree_get_type ()));
}

static void
location_list_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	LocationList *location_list;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_LOCATION_LIST (object));

	location_list = LOCATION_LIST (object);

	switch (arg_id) {
	case ARG_USER_ARCHIVE:
		g_return_if_fail (GTK_VALUE_POINTER (*arg) == NULL || 
				  IS_ARCHIVE (GTK_VALUE_POINTER (*arg)));

		if (GTK_VALUE_POINTER (*arg) == NULL) return;

		location_list->p->user_archive =
			ARCHIVE (GTK_VALUE_POINTER (*arg));

		gtk_object_ref (GTK_OBJECT (location_list->p->user_archive));
		populate_locations_list (location_list, FALSE);

		break;

	case ARG_GLOBAL_ARCHIVE:
		g_return_if_fail (GTK_VALUE_POINTER (*arg) == NULL || 
				  IS_ARCHIVE (GTK_VALUE_POINTER (*arg)));

		if (GTK_VALUE_POINTER (*arg) == NULL) return;

		location_list->p->global_archive =
			ARCHIVE (GTK_VALUE_POINTER (*arg));

		gtk_object_ref (GTK_OBJECT (location_list->p->global_archive));
		populate_locations_list (location_list, TRUE);

		break;

	case ARG_SEPARATE_LOCATIONS:
		location_list->p->separate_locations =
			GTK_VALUE_INT (*arg);
		break;

	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void
location_list_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	LocationList *location_list;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_LOCATION_LIST (object));

	location_list = LOCATION_LIST (object);

	switch (arg_id) {
	case ARG_USER_ARCHIVE:
		GTK_VALUE_POINTER (*arg) = location_list->p->user_archive;
		break;

	case ARG_GLOBAL_ARCHIVE:
		GTK_VALUE_POINTER (*arg) = location_list->p->global_archive;
		break;

	case ARG_SEPARATE_LOCATIONS:
		GTK_VALUE_INT (*arg) = location_list->p->separate_locations;
		break;

	default:
		g_warning ("Bad argument get");
		break;
	}
}

static void
location_list_finalize (GtkObject *object) 
{
	LocationList *location_list;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_LOCATION_LIST (object));

	location_list = LOCATION_LIST (object);

	if (location_list->p->user_archive != NULL)
		gtk_object_unref
			(GTK_OBJECT (location_list->p->user_archive));

	if (location_list->p->global_archive != NULL)
		gtk_object_unref
			(GTK_OBJECT (location_list->p->global_archive));

	g_free (location_list->p);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
location_list_new (gboolean sep_locations, Archive *user_archive,
		   Archive *global_archive) 
{
	return gtk_widget_new (location_list_get_type (),
			       "separate-locations", sep_locations,
			       "user-archive", user_archive,
			       "global-archive", global_archive,
			       NULL);
}

gchar *
location_list_get_selected_location_id (LocationList *list)
{
	g_return_val_if_fail (list != NULL, NULL);
	g_return_val_if_fail (IS_LOCATION_LIST (list), NULL);

	return list->p->selected_location_id;
}

Location *
location_list_get_selected_location (LocationList *list)
{
	g_return_val_if_fail (list != NULL, NULL);
	g_return_val_if_fail (IS_LOCATION_LIST (list), NULL);

	return list->p->selected_location;
}

void
location_list_reread (LocationList *list)
{
	g_return_if_fail (list != NULL);
	g_return_if_fail (IS_LOCATION_LIST (list));

	gtk_clist_freeze (GTK_CLIST (list));
	gtk_clist_clear (GTK_CLIST (list));

	if (list->p->global_archive)
		populate_locations_list (list, TRUE);

	if (list->p->user_archive)
		populate_locations_list (list, FALSE);

	gtk_clist_thaw (GTK_CLIST (list));
}

static void
select_row_cb (LocationList *list, GList *node, gint column) 
{
	GtkCTreeRow *row;

	g_return_if_fail (list != NULL);
	g_return_if_fail (IS_LOCATION_LIST (list));

	row = GTK_CTREE_ROW (node);
	list->p->selected_location = row->row.data;
	list->p->selected_location_id =
		GTK_CELL_PIXTEXT (row->row.cell[0])->text;
}

static gint
populate_locations_cb (Archive *archive, Location *location, pair_t *data)
{
	pair_t new_pair;
	char *label;

	label = g_strdup (location_get_label (location));

	new_pair.b = gtk_ctree_insert_node (GTK_CTREE (data->a), 
					    (GtkCTreeNode *) data->b, NULL,
					    &label, GNOME_PAD_SMALL, NULL,
					    NULL, NULL, NULL, FALSE, TRUE);
	gtk_ctree_node_set_row_data (GTK_CTREE (data->a),
				     (GtkCTreeNode *) new_pair.b,
				     location);

	new_pair.a = data->a;

	archive_foreach_child_location (archive,
					(LocationCB) populate_locations_cb,
					location, &new_pair);

	return 0;
}

static void
populate_locations_list (LocationList *list, gboolean do_global) 
{
	pair_t pair;
	Archive *archive;
	char *label;

	if (do_global) {
		archive = list->p->global_archive;
		label = _("Global locations");
	} else {
		archive = list->p->user_archive;
		label = _("User locations");
	}

	pair.a = list;

	if (list->p->separate_locations)
		pair.b = gtk_ctree_insert_node (GTK_CTREE (list),
						NULL, NULL, &label,
						GNOME_PAD_SMALL, NULL,
						NULL, NULL, NULL, FALSE,
						TRUE);
	else	
		pair.b = NULL;

	archive_foreach_child_location (archive,
					(LocationCB) populate_locations_cb,
					NULL, &pair);
}
