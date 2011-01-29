/*
 * Copyright (C) 2010 Red Hat, Inc.
 * Author: Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "dt-lockbutton.h"

#include <glib.h>
#include <glib/gi18n.h>

#define P_(s) s

struct _DtLockButtonPrivate
{
  GPermission *permission;

  gchar *text_lock;
  gchar *text_unlock;
  gchar *text_not_authorized;

  gchar *tooltip_lock;
  gchar *tooltip_unlock;
  gchar *tooltip_not_authorized;

  GtkWidget *box;
  GtkWidget *eventbox;
  GtkWidget *image;
  GtkWidget *button;
  GtkWidget *notebook;

  GtkWidget *label_lock;
  GtkWidget *label_unlock;
  GtkWidget *label_not_authorized;

  GCancellable *cancellable;

  gboolean constructed;
};

enum
{
  PROP_0,
  PROP_PERMISSION,
  PROP_TEXT_LOCK,
  PROP_TEXT_UNLOCK,
  PROP_TEXT_NOT_AUTHORIZED,
  PROP_TOOLTIP_LOCK,
  PROP_TOOLTIP_UNLOCK,
  PROP_TOOLTIP_NOT_AUTHORIZED
};

static void update_state (DtLockButton *button);

static void on_permission_changed (GPermission *permission,
                                   GParamSpec  *pspec,
                                   gpointer     user_data);

static void on_clicked (GtkButton *button,
                        gpointer   user_data);

static void on_button_press (GtkWidget      *widget,
                             GdkEventButton *event,
                             gpointer        user_data);

G_DEFINE_TYPE (DtLockButton, dt_lock_button, GTK_TYPE_BIN);

static void
dt_lock_button_finalize (GObject *object)
{
  DtLockButton *button = DT_LOCK_BUTTON (object);
  DtLockButtonPrivate *priv = button->priv;

  g_free (priv->text_lock);
  g_free (priv->text_unlock);
  g_free (priv->text_not_authorized);

  g_free (priv->tooltip_lock);
  g_free (priv->tooltip_unlock);
  g_free (priv->tooltip_not_authorized);

  if (priv->cancellable != NULL)
    {
      g_cancellable_cancel (priv->cancellable);
      g_object_unref (priv->cancellable);
    }

  g_signal_handlers_disconnect_by_func (priv->permission,
                                        on_permission_changed,
                                        button);

  g_object_unref (priv->permission);

  G_OBJECT_CLASS (dt_lock_button_parent_class)->finalize (object);
}

static void
dt_lock_button_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  DtLockButton *button = DT_LOCK_BUTTON (object);
  DtLockButtonPrivate *priv = button->priv;

  switch (property_id)
    {
    case PROP_PERMISSION:
      g_value_set_object (value, priv->permission);
      break;

    case PROP_TEXT_LOCK:
      g_value_set_string (value, priv->text_lock);
      break;

    case PROP_TEXT_UNLOCK:
      g_value_set_string (value, priv->text_unlock);
      break;

    case PROP_TEXT_NOT_AUTHORIZED:
      g_value_set_string (value, priv->text_not_authorized);
      break;

    case PROP_TOOLTIP_LOCK:
      g_value_set_string (value, priv->tooltip_lock);
      break;

    case PROP_TOOLTIP_UNLOCK:
      g_value_set_string (value, priv->tooltip_unlock);
      break;

    case PROP_TOOLTIP_NOT_AUTHORIZED:
      g_value_set_string (value, priv->tooltip_not_authorized);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
dt_lock_button_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  DtLockButton *button = DT_LOCK_BUTTON (object);
  DtLockButtonPrivate *priv = button->priv;

  switch (property_id)
    {
    case PROP_PERMISSION:
      priv->permission = g_value_get_object (value);
      break;

    case PROP_TEXT_LOCK:
      g_free (priv->text_lock);
      priv->text_lock = g_value_dup_string (value);
      break;

    case PROP_TEXT_UNLOCK:
      g_free (priv->text_unlock);
      priv->text_unlock = g_value_dup_string (value);
      break;

    case PROP_TEXT_NOT_AUTHORIZED:
      g_free (priv->text_not_authorized);
      priv->text_not_authorized = g_value_dup_string (value);
      break;

    case PROP_TOOLTIP_LOCK:
      g_free (priv->tooltip_lock);
      priv->tooltip_lock = g_value_dup_string (value);
      break;

    case PROP_TOOLTIP_UNLOCK:
      g_free (priv->tooltip_unlock);
      priv->tooltip_unlock = g_value_dup_string (value);
      break;

    case PROP_TOOLTIP_NOT_AUTHORIZED:
      g_free (priv->tooltip_not_authorized);
      priv->tooltip_not_authorized = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }

  if (priv->constructed)
    update_state (button);
}

static void
dt_lock_button_init (DtLockButton *button)
{
  button->priv = G_TYPE_INSTANCE_GET_PRIVATE (button,
                                              DT_TYPE_LOCK_BUTTON,
                                              DtLockButtonPrivate);
}

static void
dt_lock_button_constructed (GObject *object)
{
  DtLockButton *button = DT_LOCK_BUTTON (object);
  DtLockButtonPrivate *priv = button->priv;

  priv->constructed = TRUE;

  g_signal_connect (priv->permission, "notify",
                    G_CALLBACK (on_permission_changed), button);

  priv->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (button), priv->box);

  priv->eventbox = gtk_event_box_new ();
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (priv->eventbox), FALSE);
  gtk_container_add (GTK_CONTAINER (priv->box), priv->eventbox);
  gtk_widget_show (priv->eventbox);

  priv->image = gtk_image_new (); /* image is set in update_state() */
  gtk_container_add (GTK_CONTAINER (priv->eventbox), priv->image);
  gtk_widget_show (priv->image);

  priv->notebook = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_widget_show (priv->notebook);

  priv->button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (priv->button), priv->notebook);
  gtk_widget_show (priv->button);

  priv->label_lock = gtk_label_new (""); /* text is set in update_state */
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->label_lock, NULL);
  gtk_widget_show (priv->label_lock);

  priv->label_unlock = gtk_label_new ("");
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->label_unlock, NULL);
  gtk_widget_show (priv->label_unlock);

  priv->label_not_authorized = gtk_label_new ("");
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->label_not_authorized, NULL);
  gtk_widget_show (priv->label_not_authorized);

  gtk_box_pack_start (GTK_BOX (priv->box), priv->button, FALSE, FALSE, 0);
  gtk_widget_show (priv->button);

  g_signal_connect (priv->eventbox, "button-press-event",
                    G_CALLBACK (on_button_press), button);
  g_signal_connect (priv->button, "clicked",
                    G_CALLBACK (on_clicked), button);

  gtk_widget_set_no_show_all (priv->box, TRUE);

  update_state (button);

  if (G_OBJECT_CLASS (dt_lock_button_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (dt_lock_button_parent_class)->constructed (object);
}

static void
dt_lock_button_get_preferred_width (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
  DtLockButtonPrivate *priv = DT_LOCK_BUTTON (widget)->priv;

  gtk_widget_get_preferred_width (priv->box, minimum, natural);
}

static void
dt_lock_button_get_preferred_height (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
  DtLockButtonPrivate *priv = DT_LOCK_BUTTON (widget)->priv;

  gtk_widget_get_preferred_height (priv->box, minimum, natural);
}

static void
dt_lock_button_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  DtLockButtonPrivate *priv = DT_LOCK_BUTTON (widget)->priv;
  GtkRequisition requisition;
  GtkAllocation child_allocation;

  gtk_widget_set_allocation (widget, allocation);
  gtk_widget_get_preferred_size (priv->box, &requisition, NULL);
  child_allocation.x = allocation->x;
  child_allocation.y = allocation->y;
  child_allocation.width = requisition.width;
  child_allocation.height = requisition.height;
  gtk_widget_size_allocate (priv->box, &child_allocation);
}

static void
dt_lock_button_class_init (DtLockButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->finalize     = dt_lock_button_finalize;
  gobject_class->get_property = dt_lock_button_get_property;
  gobject_class->set_property = dt_lock_button_set_property;
  gobject_class->constructed  = dt_lock_button_constructed;

  widget_class->get_preferred_width = dt_lock_button_get_preferred_width;
  widget_class->get_preferred_height = dt_lock_button_get_preferred_height;
  widget_class->size_allocate = dt_lock_button_size_allocate;

  g_type_class_add_private (klass, sizeof (DtLockButtonPrivate));

  g_object_class_install_property (gobject_class, PROP_PERMISSION,
    g_param_spec_object ("permission",
                         P_("Permission"),
                         P_("The GPermission object controlling this button"),
                         G_TYPE_PERMISSION,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TEXT_LOCK,
    g_param_spec_string ("text-lock",
                         P_("Lock Text"),
                         P_("The text to display when prompting the user to lock"),
                         _("Lock"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TEXT_UNLOCK,
    g_param_spec_string ("text-unlock",
                         P_("Unlock Text"),
                         P_("The text to display when prompting the user to unlock"),
                         _("Unlock"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TEXT_NOT_AUTHORIZED,
    g_param_spec_string ("text-not-authorized",
                         P_("Not Authorized Text"),
                         P_("The text to display when prompting the user cannot obtain authorization"),
                         _("Locked"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TOOLTIP_LOCK,
    g_param_spec_string ("tooltip-lock",
                         P_("Lock Tooltip"),
                         P_("The tooltip to display when prompting the user to lock"),
                         _("Dialog is unlocked.\nClick to prevent further changes"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TOOLTIP_UNLOCK,
    g_param_spec_string ("tooltip-unlock",
                         P_("Unlock Tooltip"),
                         P_("The tooltip to display when prompting the user to unlock"),
                         _("Dialog is locked.\nClick to make changes"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TOOLTIP_NOT_AUTHORIZED,
    g_param_spec_string ("tooltip-not-authorized",
                         P_("Not Authorized Tooltip"),
                         P_("The tooltip to display when prompting the user cannot obtain authorization"),
                         _("System policy prevents changes.\nContact your system administrator"),
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));
}

/**
 * dt_lock_button_new:
 * @permission: a #GPermission
 *
 * Creates a new lock button which reflects the @permission.
 *
 * Returns: a new #DtLockButton
 *
 * Since: 3.0
 */
GtkWidget *
dt_lock_button_new (GPermission *permission)
{
  g_return_val_if_fail (permission != NULL, NULL);

  return GTK_WIDGET (g_object_new (DT_TYPE_LOCK_BUTTON,
                                   "permission", permission,
                                   NULL));
}

static void
update_state (DtLockButton *button)
{
  DtLockButtonPrivate *priv = button->priv;
  gint page;
  const gchar *tooltip;
  gboolean sensitive;
  gboolean visible;
  GIcon *icon;

  visible = TRUE;
  sensitive = TRUE;

  gtk_label_set_text (GTK_LABEL (priv->label_lock), priv->text_lock);
  gtk_label_set_text (GTK_LABEL (priv->label_unlock), priv->text_unlock);
  gtk_label_set_text (GTK_LABEL (priv->label_not_authorized), priv->text_not_authorized);

  if (g_permission_get_allowed (priv->permission))
    {
      if (g_permission_get_can_release (priv->permission))
        {
          page = 0;
          tooltip = priv->tooltip_lock;
          sensitive = TRUE;
        }
      else
        {
          page = 0;
          tooltip = "";
          visible = FALSE;
        }
    }
  else
    {
      if (g_permission_get_can_acquire (priv->permission))
        {
          page = 1;
          tooltip = button->priv->tooltip_unlock;
          sensitive = TRUE;
        }
      else
        {
          page = 2;
          tooltip = button->priv->tooltip_not_authorized;
          sensitive = FALSE;
        }
    }

  if (g_permission_get_allowed (priv->permission))
    {
      gchar *names[3];

      names[0] = "changes-allow-symbolic";
      names[1] = "changes-allow";
      names[2] = NULL;
      icon = g_themed_icon_new_from_names (names, -1);
    }
  else
    {
      gchar *names[3];

      names[0] = "changes-prevent-symbolic";
      names[1] = "changes-prevent";
      names[2] = NULL;
      icon = g_themed_icon_new_from_names (names, -1);
    }

  gtk_image_set_from_gicon (GTK_IMAGE (priv->image), icon, GTK_ICON_SIZE_BUTTON);
  g_object_unref (icon);

  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), page);
  gtk_widget_set_tooltip_markup (priv->box, tooltip);

  gtk_widget_set_sensitive (priv->box, sensitive);

  if (visible)
    gtk_widget_show (priv->box);
  else
    gtk_widget_hide (priv->box);
}

static void
on_permission_changed (GPermission *permission,
                       GParamSpec  *pspec,
                       gpointer     user_data)
{
  DtLockButton *button = DT_LOCK_BUTTON (user_data);

  update_state (button);
}

static void
acquire_cb (GObject      *source,
            GAsyncResult *result,
            gpointer      user_data)
{
  DtLockButton *button = DT_LOCK_BUTTON (user_data);
  DtLockButtonPrivate *priv = button->priv;
  GError *error;

  error = NULL;
  g_permission_acquire_finish (priv->permission, result, &error);

  if (error)
    {
      g_warning ("Error acquiring permission: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (priv->cancellable);
  priv->cancellable = NULL;

  update_state (button);
}

static void
release_cb (GObject      *source,
            GAsyncResult *result,
            gpointer      user_data)
{
  DtLockButton *button = DT_LOCK_BUTTON (user_data);
  DtLockButtonPrivate *priv = button->priv;
  GError *error;

  error = NULL;
  g_permission_release_finish (priv->permission, result, &error);

  if (error)
    {
      g_warning ("Error releasing permission: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (priv->cancellable);
  priv->cancellable = NULL;

  update_state (button);
}

static void
handle_click (DtLockButton *button)
{
  DtLockButtonPrivate *priv = button->priv;

  if (!g_permission_get_allowed (priv->permission) &&
       g_permission_get_can_acquire (priv->permission))
    {
      /* if we already have a pending interactive check, then do nothing */
      if (priv->cancellable != NULL)
        goto out;

      priv->cancellable = g_cancellable_new ();

      g_permission_acquire_async (priv->permission,
                                  priv->cancellable,
                                  acquire_cb,
                                  button);
    }
  else if (g_permission_get_allowed (priv->permission) &&
           g_permission_get_can_release (priv->permission))
    {
      /* if we already have a pending interactive check, then do nothing */
      if (priv->cancellable != NULL)
        goto out;

      priv->cancellable = g_cancellable_new ();

      g_permission_release_async (priv->permission,
                                  priv->cancellable,
                                  release_cb,
                                  button);
    }

 out: ;
}

static void
on_clicked (GtkButton *_button,
            gpointer   user_data)

{
  handle_click (DT_LOCK_BUTTON (user_data));
}

static void
on_button_press (GtkWidget      *widget,
                 GdkEventButton *event,
                 gpointer        user_data)
{
  handle_click (DT_LOCK_BUTTON (user_data));
}

/**
 * dt_lock_button_get_permission:
 * @button: a #DtLockButton
 *
 * Obtains the #GPermission object that controls @button.
 *
 * Returns: the #GPermission of @button
 *
 * Since: 3.0
 */
GPermission *
dt_lock_button_get_permission (DtLockButton *button)
{
  g_return_val_if_fail (DT_IS_LOCK_BUTTON (button), NULL);

  return button->priv->permission;
}

