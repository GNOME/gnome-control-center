/* cc-keyboard-shortcut-editor.h
 *
 * Copyright (C) 2016 Endless, Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 */

#include <glib-object.h>
#include <glib/gi18n.h>

#include "cc-keyboard-shortcut-editor.h"
#include "keyboard-shortcuts.h"

struct _CcKeyboardShortcutEditor
{
  GtkDialog           parent;

  GtkButton          *add_button;
  GtkButton          *cancel_button;
  GtkButton          *change_custom_shortcut_button;
  GtkEntry           *command_entry;
  GtkGrid            *custom_grid;
  GtkShortcutLabel   *custom_shortcut_accel_label;
  GtkStack           *custom_shortcut_stack;
  GtkBox             *edit_box;
  GtkHeaderBar       *headerbar;
  GtkEntry           *name_entry;
  GtkLabel           *new_shortcut_conflict_label;
  GtkButton          *remove_button;
  GtkButton          *replace_button;
  GtkButton          *reset_button;
  GtkButton          *reset_custom_button;
  GtkButton          *set_button;
  GtkShortcutLabel   *shortcut_accel_label;
  GtkLabel           *shortcut_conflict_label;
  GtkBox             *standard_box;
  GtkStack           *stack;
  GtkLabel           *top_info_label;

  CcShortcutEditorMode mode;

  CcKeyboardManager  *manager;
  CcKeyboardItem     *item;
  GBinding           *reset_item_binding;

  CcKeyboardItem     *collision_item;

  /* Custom shortcuts */
  GdkDevice          *grab_pointer;
  guint               grab_idle_id;

  CcKeyCombo         *custom_combo;
  gboolean            custom_is_modifier;
  gboolean            edited : 1;
};

static void          command_entry_changed_cb                    (CcKeyboardShortcutEditor *self);
static void          name_entry_changed_cb                       (CcKeyboardShortcutEditor *self);
static void          set_button_clicked_cb                       (CcKeyboardShortcutEditor *self);

G_DEFINE_TYPE (CcKeyboardShortcutEditor, cc_keyboard_shortcut_editor, GTK_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_KEYBOARD_ITEM,
  PROP_MANAGER,
  N_PROPS
};

typedef enum
{
  HEADER_MODE_NONE,
  HEADER_MODE_ADD,
  HEADER_MODE_SET,
  HEADER_MODE_REPLACE,
  HEADER_MODE_CUSTOM_CANCEL,
  HEADER_MODE_CUSTOM_EDIT
} HeaderMode;

typedef enum
{
  PAGE_CUSTOM,
  PAGE_EDIT,
  PAGE_STANDARD,
} ShortcutEditorPage;

static GParamSpec *properties [N_PROPS] = { NULL, };

/* Getter and setter for ShortcutEditorPage */
static ShortcutEditorPage
get_shortcut_editor_page (CcKeyboardShortcutEditor *self)
{
  if (gtk_stack_get_visible_child (self->stack) == GTK_WIDGET (self->edit_box))
    return PAGE_EDIT;

  if (gtk_stack_get_visible_child (self->stack) == GTK_WIDGET (self->custom_grid))
    return PAGE_CUSTOM;

  return PAGE_STANDARD;
}

static void
set_shortcut_editor_page (CcKeyboardShortcutEditor *self,
                          ShortcutEditorPage        page)
{
  switch (page)
    {
    case PAGE_CUSTOM:
      gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->custom_grid));
      break;

    case PAGE_EDIT:
      gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->edit_box));
      break;

    case PAGE_STANDARD:
      gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->standard_box));
      break;

    default:
      g_assert_not_reached ();
    }

    gtk_widget_set_visible (GTK_WIDGET (self->top_info_label), page != PAGE_CUSTOM);
}

static void
apply_custom_item_fields (CcKeyboardShortcutEditor *self,
                          CcKeyboardItem           *item)
{
  /* Only setup the binding when it was actually edited */
  if (self->edited)
    {
      CcKeyCombo *combo = cc_keyboard_item_get_primary_combo (item);
      g_autofree gchar *binding = NULL;

      combo->keycode = self->custom_combo->keycode;
      combo->keyval = self->custom_combo->keyval;
      combo->mask = self->custom_combo->mask;

      if (combo->keycode == 0 && combo->keyval == 0 && combo->mask == 0)
        binding = g_strdup ("");
      else
        binding = gtk_accelerator_name_with_keycode (NULL,
                                                     combo->keyval,
                                                     combo->keycode,
                                                     combo->mask);

      g_object_set (G_OBJECT (item), "binding", binding, NULL);
    }

  /* Set the keyboard shortcut name and command for custom entries */
  if (cc_keyboard_item_get_item_type (item) == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH)
    {
      g_settings_set_string (cc_keyboard_item_get_settings (item), "name", gtk_entry_get_text (self->name_entry));
      g_settings_set_string (cc_keyboard_item_get_settings (item), "command", gtk_entry_get_text (self->command_entry));
    }
}

static void
clear_custom_entries (CcKeyboardShortcutEditor *self)
{
  g_signal_handlers_block_by_func (self->command_entry, command_entry_changed_cb, self);
  g_signal_handlers_block_by_func (self->name_entry, name_entry_changed_cb, self);

  gtk_entry_set_text (self->name_entry, "");
  gtk_entry_set_text (self->command_entry, "");

  gtk_shortcut_label_set_accelerator (GTK_SHORTCUT_LABEL (self->custom_shortcut_accel_label), "");
  gtk_label_set_label (self->new_shortcut_conflict_label, "");
  gtk_label_set_label (self->shortcut_conflict_label, "");

  memset (self->custom_combo, 0, sizeof (CcKeyCombo));
  self->custom_is_modifier = TRUE;
  self->edited = FALSE;

  self->collision_item = NULL;

  g_signal_handlers_unblock_by_func (self->command_entry, command_entry_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->name_entry, name_entry_changed_cb, self);
}

static void
cancel_editing (CcKeyboardShortcutEditor *self)
{
  cc_keyboard_shortcut_editor_set_item (self, NULL);
  clear_custom_entries (self);

  gtk_widget_hide (GTK_WIDGET (self));
}

static gboolean
is_custom_shortcut (CcKeyboardShortcutEditor *self) {
  return self->item == NULL || cc_keyboard_item_get_item_type (self->item) == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH;
}

static void
grab_seat (CcKeyboardShortcutEditor *self)
{
  GdkGrabStatus status;
  GdkWindow *window;
  GdkSeat *seat;

  window = gtk_widget_get_window (GTK_WIDGET (self));
  g_assert (window);

  seat = gdk_display_get_default_seat (gdk_window_get_display (window));

  status = gdk_seat_grab (seat,
                          window,
                          GDK_SEAT_CAPABILITY_KEYBOARD,
                          FALSE,
                          NULL,
                          NULL,
                          NULL,
                          NULL);

  if (status != GDK_GRAB_SUCCESS) {
    g_warning ("Grabbing keyboard failed");
    return;
  }

  self->grab_pointer = gdk_seat_get_keyboard (seat);
  if (!self->grab_pointer)
    self->grab_pointer = gdk_seat_get_pointer (seat);

  gtk_grab_add (GTK_WIDGET (self));
}

static void
release_grab (CcKeyboardShortcutEditor *self)
{
  if (self->grab_pointer)
    {
      gdk_seat_ungrab (gdk_device_get_seat (self->grab_pointer));
      self->grab_pointer = NULL;

      gtk_grab_remove (GTK_WIDGET (self));
    }
}

static void
update_shortcut (CcKeyboardShortcutEditor *self)
{
  if (!self->item)
    return;

  /* Setup the binding */
  apply_custom_item_fields (self, self->item);

  /* Eventually disable the conflict shortcut */
  if (self->collision_item)
    cc_keyboard_manager_disable_shortcut (self->manager, self->collision_item);

  /* Cleanup whatever was set before */
  clear_custom_entries (self);

  cc_keyboard_shortcut_editor_set_item (self, NULL);
}

static GtkShortcutLabel*
get_current_shortcut_label (CcKeyboardShortcutEditor *self)
{
  if (is_custom_shortcut (self))
    return GTK_SHORTCUT_LABEL (self->custom_shortcut_accel_label);

  return GTK_SHORTCUT_LABEL (self->shortcut_accel_label);
}

static void
set_header_mode (CcKeyboardShortcutEditor *self,
                 HeaderMode                mode)
{
  gtk_header_bar_set_show_close_button (self->headerbar, mode == HEADER_MODE_CUSTOM_EDIT);

  gtk_widget_set_visible (GTK_WIDGET (self->add_button), mode == HEADER_MODE_ADD);
  gtk_widget_set_visible (GTK_WIDGET (self->cancel_button), mode != HEADER_MODE_NONE &&
                                               mode != HEADER_MODE_CUSTOM_EDIT);
  gtk_widget_set_visible (GTK_WIDGET (self->replace_button), mode == HEADER_MODE_REPLACE);
  gtk_widget_set_visible (GTK_WIDGET (self->set_button), mode == HEADER_MODE_SET);
  gtk_widget_set_visible (GTK_WIDGET (self->remove_button), mode == HEADER_MODE_CUSTOM_EDIT);

  /* By setting the default response, the action button gets the 'suggested-action' applied */
  switch (mode)
    {
    case HEADER_MODE_SET:
      gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_APPLY);
      break;

    case HEADER_MODE_REPLACE:
      gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_ACCEPT);
      break;

    case HEADER_MODE_ADD:
      gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_OK);
      break;

    default:
      gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_NONE);
    }
}

static void
setup_custom_shortcut (CcKeyboardShortcutEditor *self)
{
  GtkShortcutLabel *shortcut_label;
  CcKeyboardItem *collision_item;
  HeaderMode mode;
  gboolean is_custom, is_accel_empty;
  gboolean valid, accel_valid;
  gchar *accel;

  is_custom = is_custom_shortcut (self);
  accel_valid = is_valid_binding (self->custom_combo) &&
                is_valid_accel (self->custom_combo) &&
                !self->custom_is_modifier;

  is_accel_empty = is_empty_binding (self->custom_combo);

  if (is_accel_empty)
    accel_valid = TRUE;
  valid = accel_valid;

  /* Additional checks for custom shortcuts */
  if (is_custom)
    {
      if (accel_valid)
        {
          set_shortcut_editor_page (self, PAGE_CUSTOM);

          /* We have to check if the current accelerator is empty in order to
           * decide if we show the "Set Shortcut" button or the accelerator label */
          gtk_stack_set_visible_child (self->custom_shortcut_stack,
                                       is_accel_empty ? GTK_WIDGET (self->change_custom_shortcut_button) : GTK_WIDGET (self->custom_shortcut_accel_label));
          gtk_widget_set_visible (GTK_WIDGET (self->reset_custom_button), !is_accel_empty);
        }

      valid = accel_valid &&
              gtk_entry_get_text_length (self->name_entry) > 0 &&
              gtk_entry_get_text_length (self->command_entry) > 0;
    }

  gtk_widget_set_sensitive (GTK_WIDGET (self->replace_button), valid);
  gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), valid);
  if (valid)
    set_header_mode (self, HEADER_MODE_ADD);
  else
    set_header_mode (self, is_custom ? HEADER_MODE_CUSTOM_CANCEL : HEADER_MODE_NONE);

  /* Nothing else to do if the shortcut is invalid */
  if (!accel_valid)
    return;

  /* Valid shortcut, show it in the standard page */
  if (!is_custom)
    set_shortcut_editor_page (self, PAGE_STANDARD);

  shortcut_label = get_current_shortcut_label (self);

  collision_item = cc_keyboard_manager_get_collision (self->manager,
                                                      self->item,
                                                      self->custom_combo);

  accel = gtk_accelerator_name (self->custom_combo->keyval, self->custom_combo->mask);


  /* Setup the accelerator label */
  gtk_shortcut_label_set_accelerator (shortcut_label, accel);

  self->edited = TRUE;

  release_grab (self);

  /*
   * Oops! Looks like the accelerator is already being used, so we
   * must warn the user and let it be very clear that adding this
   * shortcut will disable the other.
   */
  gtk_widget_set_visible (GTK_WIDGET (self->new_shortcut_conflict_label), collision_item != NULL);

  if (collision_item)
    {
      GtkLabel *label;
      g_autofree gchar *friendly_accelerator = NULL;
      g_autofree gchar *accelerator_text = NULL;
      g_autofree gchar *collision_text = NULL;

      friendly_accelerator = convert_keysym_state_to_string (self->custom_combo);

      accelerator_text = g_strdup_printf ("<b>%s</b>", friendly_accelerator);
      collision_text = g_strdup_printf (_("%s is already being used for %s. If you "
                                          "replace it, %s will be disabled"),
                                        accelerator_text,
                                        cc_keyboard_item_get_description (collision_item),
                                        cc_keyboard_item_get_description (collision_item));

      label = is_custom_shortcut (self) ? self->new_shortcut_conflict_label : self->shortcut_conflict_label;

      gtk_label_set_markup (label, collision_text);
    }

  /*
   * When there is a collision between the current shortcut and another shortcut,
   * and we're editing an existing shortcut (rather than creating a new one), setup
   * the headerbar to display "Cancel" and "Replace". Otherwise, make sure to set
   * only the close button again.
   */
  if (collision_item)
    {
      mode = HEADER_MODE_REPLACE;
    }
  else
    {
      if (self->mode == CC_SHORTCUT_EDITOR_EDIT)
        mode = is_custom ? HEADER_MODE_CUSTOM_EDIT : HEADER_MODE_SET;
      else
        mode = is_custom ? HEADER_MODE_ADD : HEADER_MODE_SET;
    }

  set_header_mode (self, mode);

  self->collision_item = collision_item;
}

static void
add_button_clicked_cb (CcKeyboardShortcutEditor *self)
{
  CcKeyboardItem *item;

  item = cc_keyboard_manager_create_custom_shortcut (self->manager);

  /* Apply the custom shortcut setup at the new item */
  apply_custom_item_fields (self, item);

  /* Eventually disable the conflict shortcut */
  if (self->collision_item)
    cc_keyboard_manager_disable_shortcut (self->manager, self->collision_item);

  /* Cleanup everything once we're done */
  clear_custom_entries (self);

  cc_keyboard_manager_add_custom_shortcut (self->manager, item);

  gtk_widget_hide (GTK_WIDGET (self));
}

static void
cancel_button_clicked_cb (GtkWidget                *button,
                          CcKeyboardShortcutEditor *self)
{
  cancel_editing (self);
}

static void
change_custom_shortcut_button_clicked_cb (CcKeyboardShortcutEditor *self)
{
  grab_seat (self);
  set_shortcut_editor_page (self, PAGE_EDIT);
  set_header_mode (self, HEADER_MODE_NONE);
}

static void
command_entry_changed_cb (CcKeyboardShortcutEditor *self)
{
  setup_custom_shortcut (self);
}

static void
name_entry_changed_cb (CcKeyboardShortcutEditor *self)
{
  setup_custom_shortcut (self);
}

static void
remove_button_clicked_cb (CcKeyboardShortcutEditor *self)
{
  gtk_widget_hide (GTK_WIDGET (self));

  cc_keyboard_manager_remove_custom_shortcut (self->manager, self->item);
}

static void
replace_button_clicked_cb (CcKeyboardShortcutEditor *self)
{
  if (self->mode == CC_SHORTCUT_EDITOR_CREATE)
    add_button_clicked_cb (self);
  else
    set_button_clicked_cb (self);
}

static void
reset_custom_clicked_cb (CcKeyboardShortcutEditor *self)
{
  if (self->item)
    cc_keyboard_manager_reset_shortcut (self->manager, self->item);

  gtk_stack_set_visible_child (self->custom_shortcut_stack, GTK_WIDGET (self->change_custom_shortcut_button));
  gtk_widget_hide (GTK_WIDGET (self->reset_custom_button));
}

static void
reset_item_clicked_cb (CcKeyboardShortcutEditor *self)
{
  CcKeyCombo *combo;
  gchar *accel;

  /* Reset first, then update the shortcut */
  cc_keyboard_manager_reset_shortcut (self->manager, self->item);

  combo = cc_keyboard_item_get_primary_combo (self->item);
  accel = gtk_accelerator_name (combo->keyval, combo->mask);
  gtk_shortcut_label_set_accelerator (GTK_SHORTCUT_LABEL (self->shortcut_accel_label), accel);

  g_free (accel);
}

static void
set_button_clicked_cb (CcKeyboardShortcutEditor *self)
{
  update_shortcut (self);
  gtk_widget_hide (GTK_WIDGET (self));
}

static void
setup_keyboard_item (CcKeyboardShortcutEditor *self,
                     CcKeyboardItem           *item)
{
  CcKeyCombo *combo;
  gboolean is_custom;
  g_autofree gchar *accel = NULL;
  g_autofree gchar *description_text = NULL;
  g_autofree gchar *text = NULL;

  if (!item) {
    gtk_label_set_text (self->top_info_label, _("Enter the new shortcut"));
    return;
  }

  combo = cc_keyboard_item_get_primary_combo (item);
  is_custom = cc_keyboard_item_get_item_type (item) == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH;
  accel = gtk_accelerator_name (combo->keyval, combo->mask);

  /* To avoid accidentally thinking we unset the current keybinding, set the values
   * of the keyboard item that is being edited */
  self->custom_is_modifier = FALSE;
  self->custom_combo->keycode = combo->keycode;
  self->custom_combo->keyval = combo->keyval;
  self->custom_combo->mask = combo->mask;

  /* Headerbar */
  gtk_header_bar_set_title (self->headerbar,
                            is_custom ? _("Set Custom Shortcut") : _("Set Shortcut"));

  set_header_mode (self, is_custom ? HEADER_MODE_CUSTOM_EDIT : HEADER_MODE_NONE);

  gtk_widget_hide (GTK_WIDGET (self->add_button));
  gtk_widget_hide (GTK_WIDGET (self->cancel_button));
  gtk_widget_hide (GTK_WIDGET (self->replace_button));

  /* Setup the top label */
  description_text = g_strdup_printf ("<b>%s</b>", cc_keyboard_item_get_description (item));
  /* TRANSLATORS: %s is replaced with a description of the keyboard shortcut */
  text = g_strdup_printf (_("Enter new shortcut to change %s."), description_text);

  gtk_label_set_markup (self->top_info_label, text);

  /* Accelerator labels */
  gtk_shortcut_label_set_accelerator (self->shortcut_accel_label, accel);
  gtk_shortcut_label_set_accelerator (self->custom_shortcut_accel_label, accel);

  g_clear_pointer (&self->reset_item_binding, g_binding_unbind);
  self->reset_item_binding = g_object_bind_property (item,
                                                     "is-value-default",
                                                     self->reset_button,
                                                     "visible",
                                                     G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);

  /* Setup the custom entries */
  if (is_custom)
    {
      gboolean is_accel_empty;

      g_signal_handlers_block_by_func (self->command_entry, command_entry_changed_cb, self);
      g_signal_handlers_block_by_func (self->name_entry, name_entry_changed_cb, self);

      /* Name entry */
      gtk_entry_set_text (self->name_entry, cc_keyboard_item_get_description (item));
      gtk_widget_set_sensitive (GTK_WIDGET (self->name_entry), cc_keyboard_item_get_desc_editable (item));

      /* Command entry */
      gtk_entry_set_text (self->command_entry, cc_keyboard_item_get_command (item));
      gtk_widget_set_sensitive (GTK_WIDGET (self->command_entry), cc_keyboard_item_get_cmd_editable (item));

      /* If there is no accelerator set for this custom shortcut, show the "Set Shortcut" button. */
      is_accel_empty = !accel || accel[0] == '\0';

      gtk_stack_set_visible_child (self->custom_shortcut_stack,
                                   is_accel_empty ? GTK_WIDGET (self->change_custom_shortcut_button) : GTK_WIDGET (self->custom_shortcut_accel_label));

      gtk_widget_set_visible (GTK_WIDGET (self->reset_custom_button), !is_accel_empty);

      g_signal_handlers_unblock_by_func (self->command_entry, command_entry_changed_cb, self);
      g_signal_handlers_unblock_by_func (self->name_entry, name_entry_changed_cb, self);

      release_grab (self);
    }

  /* Show the appropriate view */
  set_shortcut_editor_page (self, is_custom ? PAGE_CUSTOM : PAGE_EDIT);
}

static void
cc_keyboard_shortcut_editor_finalize (GObject *object)
{
  CcKeyboardShortcutEditor *self = (CcKeyboardShortcutEditor *)object;

  g_clear_object (&self->item);
  g_clear_object (&self->manager);

  g_clear_pointer (&self->custom_combo, g_free);
  g_clear_pointer (&self->reset_item_binding, g_binding_unbind);

  G_OBJECT_CLASS (cc_keyboard_shortcut_editor_parent_class)->finalize (object);
}

static void
cc_keyboard_shortcut_editor_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  CcKeyboardShortcutEditor *self = CC_KEYBOARD_SHORTCUT_EDITOR (object);

  switch (prop_id)
    {
    case PROP_KEYBOARD_ITEM:
      g_value_set_object (value, self->item);
      break;

    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_keyboard_shortcut_editor_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  CcKeyboardShortcutEditor *self = CC_KEYBOARD_SHORTCUT_EDITOR (object);

  switch (prop_id)
    {
    case PROP_KEYBOARD_ITEM:
      cc_keyboard_shortcut_editor_set_item (self, g_value_get_object (value));
      break;

    case PROP_MANAGER:
      g_set_object (&self->manager, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
cc_keyboard_shortcut_editor_key_press_event (GtkWidget   *widget,
                                             GdkEventKey *event)
{
  CcKeyboardShortcutEditor *self;
  GdkModifierType real_mask;
  gboolean editing;
  guint keyval_lower;

  self = CC_KEYBOARD_SHORTCUT_EDITOR (widget);

  /* Being in the "change-shortcut" page is the only check we must
   * perform to decide if we're editing a shortcut. */
  editing = get_shortcut_editor_page (self) == PAGE_EDIT;

  if (!editing)
    return GTK_WIDGET_CLASS (cc_keyboard_shortcut_editor_parent_class)->key_press_event (widget, event);

  real_mask = event->state & gtk_accelerator_get_default_mod_mask ();

  keyval_lower = gdk_keyval_to_lower (event->keyval);

  /* Normalise <Tab> */
  if (keyval_lower == GDK_KEY_ISO_Left_Tab)
    keyval_lower = GDK_KEY_Tab;

  /* Put shift back if it changed the case of the key, not otherwise. */
  if (keyval_lower != event->keyval)
    real_mask |= GDK_SHIFT_MASK;

  if (keyval_lower == GDK_KEY_Sys_Req &&
      (real_mask & GDK_MOD1_MASK) != 0)
    {
      /* HACK: we don't want to use SysRq as a keybinding (but we do
       * want Alt+Print), so we avoid translation from Alt+Print to SysRq */
      keyval_lower = GDK_KEY_Print;
    }

  /* A single Escape press cancels the editing */
  if (!event->is_modifier && real_mask == 0 && keyval_lower == GDK_KEY_Escape)
    {
      self->edited = FALSE;

      release_grab (self);
      cancel_editing (self);

      return GDK_EVENT_STOP;
    }

  /* Backspace disables the current shortcut */
  if (!event->is_modifier && real_mask == 0 && keyval_lower == GDK_KEY_BackSpace)
    {
      self->edited = TRUE;
      self->custom_is_modifier = FALSE;
      memset (self->custom_combo, 0, sizeof (CcKeyCombo));

      gtk_shortcut_label_set_accelerator (GTK_SHORTCUT_LABEL (self->custom_shortcut_accel_label), "");
      gtk_shortcut_label_set_accelerator (GTK_SHORTCUT_LABEL (self->shortcut_accel_label), "");

      release_grab (self);

      self->edited = FALSE;

      setup_custom_shortcut (self);

      return GDK_EVENT_STOP;
    }

  self->custom_is_modifier = event->is_modifier;
  self->custom_combo->keycode = event->hardware_keycode;
  self->custom_combo->keyval = keyval_lower;
  self->custom_combo->mask = real_mask;

  /* CapsLock isn't supported as a keybinding modifier, so keep it from confusing us */
  self->custom_combo->mask &= ~GDK_LOCK_MASK;

  setup_custom_shortcut (self);

  return GDK_EVENT_STOP;
}

static void
cc_keyboard_shortcut_editor_close (GtkDialog *dialog)
{
  CcKeyboardShortcutEditor *self = CC_KEYBOARD_SHORTCUT_EDITOR (dialog);

  if (self->mode == CC_SHORTCUT_EDITOR_EDIT)
    update_shortcut (self);

  GTK_DIALOG_CLASS (cc_keyboard_shortcut_editor_parent_class)->close (dialog);
}

static void
cc_keyboard_shortcut_editor_response (GtkDialog *dialog,
                                      gint       response_id)
{
  CcKeyboardShortcutEditor *self = CC_KEYBOARD_SHORTCUT_EDITOR (dialog);

  if (response_id == GTK_RESPONSE_DELETE_EVENT &&
      self->mode == CC_SHORTCUT_EDITOR_EDIT)
    {
      update_shortcut (self);
    }
}

static gboolean
grab_idle (gpointer data)
{
  CcKeyboardShortcutEditor *self = data;

  if (self->item && cc_keyboard_item_get_item_type (self->item) != CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH)
    grab_seat (self);

  self->grab_idle_id = 0;

  return G_SOURCE_REMOVE;
}

static void
cc_keyboard_shortcut_editor_show (GtkWidget *widget)
{
  CcKeyboardShortcutEditor *self = CC_KEYBOARD_SHORTCUT_EDITOR (widget);

  /* Map before grabbing, so that the window is visible */
  GTK_WIDGET_CLASS (cc_keyboard_shortcut_editor_parent_class)->show (widget);

  self->grab_idle_id = g_timeout_add (100, grab_idle, self);
}

static void
cc_keyboard_shortcut_editor_unrealize (GtkWidget *widget)
{
  CcKeyboardShortcutEditor *self = CC_KEYBOARD_SHORTCUT_EDITOR (widget);

  if (self->grab_idle_id) {
    g_source_remove (self->grab_idle_id);
    self->grab_idle_id = 0;
  }

  release_grab (self);

  GTK_WIDGET_CLASS (cc_keyboard_shortcut_editor_parent_class)->unrealize (widget);
}

static void
cc_keyboard_shortcut_editor_class_init (CcKeyboardShortcutEditorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_keyboard_shortcut_editor_finalize;
  object_class->get_property = cc_keyboard_shortcut_editor_get_property;
  object_class->set_property = cc_keyboard_shortcut_editor_set_property;

  widget_class->show = cc_keyboard_shortcut_editor_show;
  widget_class->unrealize = cc_keyboard_shortcut_editor_unrealize;
  widget_class->key_press_event = cc_keyboard_shortcut_editor_key_press_event;

  dialog_class->close = cc_keyboard_shortcut_editor_close;
  dialog_class->response = cc_keyboard_shortcut_editor_response;

  /**
   * CcKeyboardShortcutEditor:keyboard-item:
   *
   * The current keyboard shortcut being edited.
   */
  properties[PROP_KEYBOARD_ITEM] = g_param_spec_object ("keyboard-item",
                                                        "Keyboard item",
                                                        "The keyboard item being edited",
                                                        CC_TYPE_KEYBOARD_ITEM,
                                                        G_PARAM_READWRITE);

  /**
   * CcKeyboardShortcutEditor:panel:
   *
   * The current keyboard panel.
   */
  properties[PROP_MANAGER] = g_param_spec_object ("manager",
                                                  "Keyboard manager",
                                                  "The keyboard manager",
                                                  CC_TYPE_KEYBOARD_MANAGER,
                                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/keyboard/cc-keyboard-shortcut-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, add_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, change_custom_shortcut_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, command_entry);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, custom_grid);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, custom_shortcut_accel_label);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, custom_shortcut_stack);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, edit_box);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, name_entry);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, new_shortcut_conflict_label);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, remove_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, replace_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, reset_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, reset_custom_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, set_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, shortcut_accel_label);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, shortcut_conflict_label);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, standard_box);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, stack);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, top_info_label);

  gtk_widget_class_bind_template_callback (widget_class, add_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, cancel_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, change_custom_shortcut_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, command_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, name_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, remove_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, replace_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, reset_custom_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, reset_item_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, set_button_clicked_cb);
}

static void
cc_keyboard_shortcut_editor_init (CcKeyboardShortcutEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->mode = CC_SHORTCUT_EDITOR_EDIT;
  self->custom_is_modifier = TRUE;
  self->custom_combo = g_new0 (CcKeyCombo, 1);

  gtk_widget_set_direction (GTK_WIDGET (self->custom_shortcut_accel_label), GTK_TEXT_DIR_LTR);
  gtk_widget_set_direction (GTK_WIDGET (self->shortcut_accel_label), GTK_TEXT_DIR_LTR);
}

/**
 * cc_keyboard_shortcut_editor_new:
 *
 * Creates a new #CcKeyboardShortcutEditor.
 *
 * Returns: (transfer full): a newly created #CcKeyboardShortcutEditor.
 */
GtkWidget*
cc_keyboard_shortcut_editor_new (CcKeyboardManager *manager)
{
  return g_object_new (CC_TYPE_KEYBOARD_SHORTCUT_EDITOR,
                       "manager", manager,
                       "use-header-bar", 1,
                       NULL);
}

/**
 * cc_keyboard_shortcut_editor_get_item:
 * @self: a #CcKeyboardShortcutEditor
 *
 * Retrieves the current keyboard shortcut being edited.
 *
 * Returns: (transfer none)(nullable): a #CcKeyboardItem
 */
CcKeyboardItem*
cc_keyboard_shortcut_editor_get_item (CcKeyboardShortcutEditor *self)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_SHORTCUT_EDITOR (self), NULL);

  return self->item;
}

/**
 * cc_keyboard_shortcut_editor_set_item:
 * @self: a #CcKeyboardShortcutEditor
 * @item: a #CcKeyboardItem
 *
 * Sets the current keyboard shortcut to be edited.
 */
void
cc_keyboard_shortcut_editor_set_item (CcKeyboardShortcutEditor *self,
                                      CcKeyboardItem           *item)
{
  g_return_if_fail (CC_IS_KEYBOARD_SHORTCUT_EDITOR (self));

  setup_keyboard_item (self, item);

  if (!g_set_object (&self->item, item))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KEYBOARD_ITEM]);
}

CcShortcutEditorMode
cc_keyboard_shortcut_editor_get_mode (CcKeyboardShortcutEditor *self)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_SHORTCUT_EDITOR (self), 0);

  return self->mode;
}

void
cc_keyboard_shortcut_editor_set_mode (CcKeyboardShortcutEditor *self,
                                      CcShortcutEditorMode      mode)
{
  gboolean is_create_mode;

  g_return_if_fail (CC_IS_KEYBOARD_SHORTCUT_EDITOR (self));

  self->mode = mode;
  is_create_mode = mode == CC_SHORTCUT_EDITOR_CREATE;

  gtk_widget_set_visible (GTK_WIDGET (self->new_shortcut_conflict_label), is_create_mode);
  gtk_stack_set_visible_child (self->custom_shortcut_stack,
                               is_create_mode ? GTK_WIDGET (self->change_custom_shortcut_button) : GTK_WIDGET (self->custom_shortcut_accel_label));

  if (mode == CC_SHORTCUT_EDITOR_CREATE)
    {
      /* Cleanup whatever was set before */
      clear_custom_entries (self);

      set_header_mode (self, HEADER_MODE_ADD);
      set_shortcut_editor_page (self, PAGE_CUSTOM);
      gtk_header_bar_set_title (self->headerbar, _("Add Custom Shortcut"));

      gtk_widget_set_sensitive (GTK_WIDGET (self->command_entry), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->name_entry), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), FALSE);

      gtk_widget_hide (GTK_WIDGET (self->reset_custom_button));
    }
}
