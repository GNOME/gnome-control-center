/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <adwaita.h>

#include "cc-color-common.h"
#include "cc-color-profile.h"
#include "cc-list-row-info-button.h"

struct _CcColorProfile
{
  AdwActionRow parent_instance;

  CdDevice    *device;
  CdProfile   *profile;
  gboolean     is_default;
  gchar       *sortable;
  GtkWidget   *widget_image;
  GtkWidget   *widget_info;
  GSettings   *settings;
};

#define GCM_SETTINGS_RECALIBRATE_PRINTER_THRESHOLD      "recalibrate-printer-threshold"
#define GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD      "recalibrate-display-threshold"


#define IMAGE_WIDGET_PADDING 12

G_DEFINE_TYPE (CcColorProfile, cc_color_profile, ADW_TYPE_ACTION_ROW)

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_PROFILE,
  PROP_IS_DEFAULT,
  PROP_LAST
};

static gchar *
cc_color_profile_get_profile_date (CdProfile *profile)
{
  gint64 created;
  g_autoptr(GDateTime) dt = NULL;

  /* get profile age */
  created = cd_profile_get_created (profile);
  if (created == 0)
    return NULL;
  dt = g_date_time_new_from_unix_utc (created);
  return g_date_time_format (dt, "%x");
}

static gchar *
gcm_prefs_get_profile_title (CdProfile *profile)
{
  CdColorspace colorspace;
  const gchar *tmp;
  GString *str;

  str = g_string_new ("");

  /* add date only if it's a calibration profile or the profile has
   * not been tagged with this data */
  tmp = cd_profile_get_metadata_item (profile, CD_PROFILE_METADATA_DATA_SOURCE);
  if (tmp == NULL || g_strcmp0 (tmp, CD_PROFILE_METADATA_DATA_SOURCE_CALIB) == 0)
    {
      tmp = cc_color_profile_get_profile_date (profile);
      if (tmp != NULL)
        g_string_append_printf (str, "%s - ", tmp);
    }
  else if (g_strcmp0 (tmp, CD_PROFILE_METADATA_DATA_SOURCE_STANDARD) == 0)
    {
      /* TRANSLATORS: standard spaces are well known colorspaces like
       * sRGB, AdobeRGB and ProPhotoRGB */
      g_string_append_printf (str, "%s - ", _("Standard Space"));
    }
  else if (g_strcmp0 (tmp, CD_PROFILE_METADATA_DATA_SOURCE_TEST) == 0)
    {
      /* TRANSLATORS: test profiles do things like changing the screen
       * a different color, or swap the red and green channels */
      g_string_append_printf (str, "%s - ", _("Test Profile"));
    }
  else if (g_strcmp0 (tmp, CD_PROFILE_METADATA_DATA_SOURCE_EDID) == 0)
    {
      /* TRANSLATORS: automatic profiles are generated automatically
       * by the color management system based on manufacturing data,
       * for instance the default monitor profile is created from the
       * primaries specified in the monitor EDID */
      g_string_append_printf (str, "%s - ", C_("Automatically generated profile", "Automatic"));
    }

  /* add quality if it exists */
  tmp = cd_profile_get_metadata_item (profile, CD_PROFILE_METADATA_QUALITY);
  if (g_strcmp0 (tmp, CD_PROFILE_METADATA_QUALITY_LOW) == 0)
    {
      /* TRANSLATORS: the profile quality - low quality profiles take
       * much less time to generate but may be a poor reflection of the
       * device capability */
      g_string_append_printf (str, "%s - ", C_("Profile quality", "Low Quality"));
    }
  else if (g_strcmp0 (tmp, CD_PROFILE_METADATA_QUALITY_MEDIUM) == 0)
    {
      /* TRANSLATORS: the profile quality */
      g_string_append_printf (str, "%s - ", C_("Profile quality", "Medium Quality"));
    }
  else if (g_strcmp0 (tmp, CD_PROFILE_METADATA_QUALITY_HIGH) == 0)
    {
      /* TRANSLATORS: the profile quality - high quality profiles take
       * a *long* time, and have the best calibration and
       * characterisation data. */
      g_string_append_printf (str, "%s - ", C_("Profile quality", "High Quality"));
    }

  /* add profile description */
  tmp = cd_profile_get_title (profile);
  if (tmp != NULL)
    {
      g_string_append (str, tmp);
      goto out;
    }

  /* some meta profiles do not have ICC profiles */
  colorspace = cd_profile_get_colorspace (profile);
  if (colorspace == CD_COLORSPACE_RGB)
    {
      /* TRANSLATORS: this default RGB space is used for printers that
       * do not have additional printer profiles specified in the PPD */
      g_string_append (str, C_("Colorspace fallback", "Default RGB"));
      goto out;
    }
  if (colorspace == CD_COLORSPACE_CMYK)
    {
      /* TRANSLATORS: this default CMYK space is used for printers that
       * do not have additional printer profiles specified in the PPD */
      g_string_append (str, C_("Colorspace fallback", "Default CMYK"));
      goto out;
    }
  if (colorspace == CD_COLORSPACE_GRAY)
    {
      /* TRANSLATORS: this default gray space is used for printers that
       * do not have additional printer profiles specified in the PPD */
      g_string_append (str, C_("Colorspace fallback", "Default Gray"));
      goto out;
    }

  /* fall back to ID, ick */
  tmp = g_strdup (cd_profile_get_id (profile));
  g_string_append (str, tmp);
out:
  return g_string_free (str, FALSE);
}

static const gchar *
cc_color_profile_get_warnings (CcColorProfile *self)
{
  const gchar *tooltip = NULL;
  const guint seconds_in_one_day = 60 * 60 * 24;
  gint num_days;
  guint threshold_days = 0;

  /* autogenerated printer defaults */
  if (cd_device_get_kind (self->device) == CD_DEVICE_KIND_PRINTER &&
      cd_profile_get_filename (self->profile) == NULL)
    {
      tooltip = _("Vendor supplied factory calibration data");
      goto out;
    }

  /* autogenerated profiles are crap */
  if (cd_device_get_kind (self->device) == CD_DEVICE_KIND_DISPLAY &&
      cd_profile_get_kind (self->profile) == CD_PROFILE_KIND_DISPLAY_DEVICE &&
      !cd_profile_get_has_vcgt (self->profile))
    {
      tooltip = _("Full-screen display correction not possible with this profile");
      goto out;
    }

  /* greater than the calibration threshold for the device type */
  num_days = cd_profile_get_age (self->profile) / seconds_in_one_day;
  if (cd_device_get_kind (self->device) == CD_DEVICE_KIND_DISPLAY)
    {
      g_settings_get (self->settings,
                      GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD,
                      "u",
                      &threshold_days);
    }
  else if (cd_device_get_kind (self->device) == CD_DEVICE_KIND_DISPLAY)
    {
      g_settings_get (self->settings,
                      GCM_SETTINGS_RECALIBRATE_PRINTER_THRESHOLD,
                      "u",
                      &threshold_days);
    }
  if (threshold_days > 0 && num_days > threshold_days)
    {
      tooltip = _("This profile may no longer be accurate");
      goto out;
    }
out:
  return tooltip;
}

static void
cc_color_profile_refresh (CcColorProfile *self)
{
  const gchar *warnings;
  g_autofree gchar *title = NULL;

  /* show the image if the profile is default */

 if (self->is_default) {
   gtk_image_set_from_icon_name (GTK_IMAGE (self->widget_image), "object-select-symbolic");
 }
  else {
    gtk_image_set_from_icon_name (GTK_IMAGE (self->widget_image), NULL);
  }

  /* set the title */
  title = gcm_prefs_get_profile_title (self->profile);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), title);

  /* show any information */
  warnings = cc_color_profile_get_warnings (self);
  gtk_widget_set_visible (self->widget_info, warnings != NULL);
  cc_list_row_info_button_set_text (CC_LIST_ROW_INFO_BUTTON (self->widget_info), warnings);
}

CdDevice *
cc_color_profile_get_device (CcColorProfile *self)
{
  g_return_val_if_fail (CC_IS_COLOR_PROFILE (self), NULL);
  return self->device;
}

CdProfile *
cc_color_profile_get_profile (CcColorProfile *self)
{
  g_return_val_if_fail (CC_IS_COLOR_PROFILE (self), NULL);
  return self->profile;
}

const gchar *
cc_color_profile_get_sortable (CcColorProfile *self)
{
  g_return_val_if_fail (CC_IS_COLOR_PROFILE (self), NULL);
  return self->sortable;
}

gboolean
cc_color_profile_get_is_default (CcColorProfile *self)
{
  g_return_val_if_fail (CC_IS_COLOR_PROFILE (self), 0);
  return self->is_default;
}

void
cc_color_profile_set_is_default (CcColorProfile *self, gboolean is_default)
{
  g_return_if_fail (CC_IS_COLOR_PROFILE (self));
  self->is_default = is_default;
  cc_color_profile_refresh (self);
}

static void
cc_color_profile_get_property (GObject *object, guint param_id,
                               GValue *value, GParamSpec *pspec)
{
  CcColorProfile *self = CC_COLOR_PROFILE (object);
  switch (param_id)
    {
      case PROP_DEVICE:
        g_value_set_object (value, self->device);
        break;
      case PROP_PROFILE:
        g_value_set_object (value, self->profile);
        break;
      case PROP_IS_DEFAULT:
        g_value_set_boolean (value, self->is_default);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
cc_color_profile_set_property (GObject *object, guint param_id,
                               const GValue *value, GParamSpec *pspec)
{
  CcColorProfile *self = CC_COLOR_PROFILE (object);

  switch (param_id)
    {
      case PROP_DEVICE:
        self->device = g_value_dup_object (value);
        break;
      case PROP_PROFILE:
        self->profile = g_value_dup_object (value);
        break;
      case PROP_IS_DEFAULT:
        self->is_default = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
cc_color_profile_finalize (GObject *object)
{
  CcColorProfile *self = CC_COLOR_PROFILE (object);

  g_free (self->sortable);
  g_object_unref (self->device);
  g_object_unref (self->profile);
  g_object_unref (self->settings);

  G_OBJECT_CLASS (cc_color_profile_parent_class)->finalize (object);
}

static void
cc_color_profile_changed_cb (CcColorProfile *self)
{
  g_autoptr(CdProfile) profile = NULL;

  /* check to see if the default has changed */
  profile = cd_device_get_default_profile (self->device);
  if (profile != NULL)
    self->is_default = g_strcmp0 (cd_profile_get_object_path (profile),
                                           cd_profile_get_object_path (self->profile)) == 0;
  cc_color_profile_refresh (self);
}

static const gchar *
cc_color_profile_get_profile_sort_data_source (CdProfile *profile)
{
  const gchar *tmp;
  tmp = cd_profile_get_metadata_item (profile, CD_PROFILE_METADATA_DATA_SOURCE);
  if (tmp == NULL)
    return "9";
  if (g_strcmp0 (tmp, CD_PROFILE_METADATA_DATA_SOURCE_CALIB) == 0)
    return "3";
  if (g_strcmp0 (tmp, CD_PROFILE_METADATA_DATA_SOURCE_STANDARD) == 0)
    return "2";
  if (g_strcmp0 (tmp, CD_PROFILE_METADATA_DATA_SOURCE_TEST) == 0)
    return "1";
  return "0";
}

static void
cc_color_profile_constructed (GObject *object)
{
  CcColorProfile *self = CC_COLOR_PROFILE (object);
  const gchar *sortable_data_source;
  g_autofree gchar *sortable_device = NULL;
  g_autofree gchar *title = NULL;

  /* watch to see if the default changes */
  g_signal_connect_object (self->device, "changed",
                           G_CALLBACK (cc_color_profile_changed_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->profile, "changed",
                           G_CALLBACK (cc_color_profile_changed_cb), self, G_CONNECT_SWAPPED);

  /* sort the profiles in the list by:
   * 1. their device (required)
   * 2. the data source (so calibration profiles are listed before autogenerated ones)
   * 3. the date the profiles were created (newest first)
   * 4. the alpha sorting of the filename
   */
  title = gcm_prefs_get_profile_title (self->profile);
  sortable_device = cc_color_device_get_sortable_base (self->device);
  sortable_data_source = cc_color_profile_get_profile_sort_data_source (self->profile);
  self->sortable = g_strdup_printf ("%s-%s-%012" G_GINT64_FORMAT "-%s",
                                    sortable_device,
                                    sortable_data_source,
                                    cd_profile_get_created (self->profile),
                                    title);

  cc_color_profile_refresh (self);
}

static void
cc_color_profile_class_init (CcColorProfileClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = cc_color_profile_get_property;
  object_class->set_property = cc_color_profile_set_property;
  object_class->constructed = cc_color_profile_constructed;
  object_class->finalize = cc_color_profile_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/color/cc-color-profile.ui");

  gtk_widget_class_bind_template_child (widget_class, CcColorProfile, widget_image);
  gtk_widget_class_bind_template_child (widget_class, CcColorProfile, widget_info);

  g_object_class_install_property (object_class, PROP_DEVICE,
                                   g_param_spec_object ("device", NULL,
                                                        NULL,
                                                        CD_TYPE_DEVICE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PROFILE,
                                   g_param_spec_object ("profile", NULL,
                                                        NULL,
                                                        CD_TYPE_PROFILE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_IS_DEFAULT,
                                   g_param_spec_boolean ("is-default", NULL,
                                                         NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
cc_color_profile_init (CcColorProfile *self)
{
  self->settings = g_settings_new (GCM_SETTINGS_SCHEMA);

  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
cc_color_profile_new (CdDevice *device,
                      CdProfile *profile,
                      gboolean is_default)
{
  return g_object_new (CC_TYPE_COLOR_PROFILE,
                       "device", device,
                       "profile", profile,
                       "is-default", is_default,
                       NULL);
}
