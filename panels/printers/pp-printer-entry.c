/*
 * Copyright 2017 Red Hat, Inc
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
 * Author: Felipe Borges <felipeborges@gnome.org>
 */

#include <config.h>

#include "pp-printer-entry.h"
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "pp-details-dialog.h"
#include "pp-maintenance-command.h"
#include "pp-options-dialog.h"
#include "pp-jobs-dialog.h"
#include "pp-printer.h"
#include "pp-utils.h"

#define SUPPLY_BAR_HEIGHT 8

struct _PpPrinterEntry
{
  GtkListBoxRow parent;

  gchar    *printer_uri;
  gchar    *printer_name;
  gchar    *ppd_file_name;
  int       num_jobs;
  gboolean  is_accepting_jobs;
  gchar    *printer_make_and_model;
  gchar    *printer_location;
  gchar    *printer_hostname;
  gboolean  is_authorized;
  gint      printer_state;

  /* Maintenance commands */
  PpMaintenanceCommand *clean_command;
  GCancellable *check_clean_heads_cancellable;

  /* Widgets */
  GtkImage       *printer_icon;
  GtkLabel       *printer_status;
  GtkLabel       *printer_name_label;
  GtkLabel       *printer_model_label;
  GtkLabel       *printer_model;
  GtkLabel       *printer_location_label;
  GtkLabel       *printer_location_address_label;
  GtkDrawingArea *supply_drawing_area;
  GtkWidget      *show_jobs_dialog_button;
  GtkWidget      *clean_heads_menuitem;
  GtkCheckButton *printer_default_checkbutton;
  GtkModelButton *remove_printer_menuitem;
  GtkBox         *printer_error;
  GtkLabel       *error_status;

  /* Dialogs */
  PpDetailsDialog *pp_details_dialog;
  PpOptionsDialog *pp_options_dialog;
  PpJobsDialog    *pp_jobs_dialog;

  GCancellable *get_jobs_cancellable;
};

struct _PpPrinterEntryClass
{
  GtkListBoxRowClass parent_class;

  void (*printer_changed) (PpPrinterEntry *printer_entry);
};

G_DEFINE_TYPE (PpPrinterEntry, pp_printer_entry, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_PRINTER_NAME,
  PROP_PRINTER_LOCATION,
};

enum {
  IS_DEFAULT_PRINTER,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
pp_printer_entry_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  PpPrinterEntry *self = PP_PRINTER_ENTRY (object);

  switch (prop_id)
    {
      case PROP_PRINTER_NAME:
        g_value_set_string (value, self->printer_name);
        break;
      case PROP_PRINTER_LOCATION:
        g_value_set_string (value, self->printer_location);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pp_printer_entry_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  PpPrinterEntry *self = PP_PRINTER_ENTRY (object);

  switch (prop_id)
    {
      case PROP_PRINTER_NAME:
        self->printer_name = g_value_dup_string (value);
        break;
      case PROP_PRINTER_LOCATION:
        self->printer_location = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
pp_printer_entry_init (PpPrinterEntry *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

typedef struct {
  gchar *color;
  gchar *type;
  gchar *name;
  gint   level;
} MarkerItem;

static gint
markers_cmp (gconstpointer a,
             gconstpointer b)
{
  MarkerItem *x = (MarkerItem*) a;
  MarkerItem *y = (MarkerItem*) b;

  if (x->level < y->level)
    return 1;
  else if (x->level == y->level)
    return 0;
  else
    return -1;
}

static gchar *
sanitize_printer_model (gchar *printer_make_and_model)
{
  gchar  *breakpoint = NULL, *tmp = NULL, *tmp2 = NULL;
  gchar  *printer_model = NULL;
  gchar   backup;
  size_t  length = 0;
  gchar  *forbiden[] = {
    "foomatic",
    ",",
    "hpijs",
    "hpcups",
    "(recommended)",
    "postscript (recommended)",
    NULL };
  int     i;

  tmp = g_ascii_strdown (printer_make_and_model, -1);

  for (i = 0; i < g_strv_length (forbiden); i++)
    {
      tmp2 = g_strrstr (tmp, forbiden[i]);
      if (breakpoint == NULL ||
         (tmp2 != NULL && tmp2 < breakpoint))
           breakpoint = tmp2;
    }

  if (breakpoint)
    {
      backup = *breakpoint;
      *breakpoint = '\0';
      length = strlen (tmp);
      *breakpoint = backup;

      if (length > 0)
        printer_model = g_strndup (printer_make_and_model, length);
    }
  else
    printer_model = g_strdup (printer_make_and_model);

  g_free (tmp);

  return g_strdup (printer_model);
}

typedef struct
{
  gchar *marker_names;
  gchar *marker_levels;
  gchar *marker_colors;
  gchar *marker_types;
} InkLevelData;

/* To tone down the colors in the supply level bar
 * we shade them by darkening the hue.
 *
 * Obs.: we don't know whether the color is already
 * shaded.
 *
 */
static void
tone_down_color (GdkRGBA *color,
                 gdouble  hue_ratio,
                 gdouble  saturation_ratio,
                 gdouble  value_ratio)
{
  gdouble h, s, v;

  gtk_rgb_to_hsv (color->red, color->green, color->blue,
                  &h, &s, &v);
  gtk_hsv_to_rgb (h * hue_ratio, s * saturation_ratio, v * value_ratio,
                  &color->red, &color->green, &color->blue);
}

static gboolean
supply_levels_draw_cb (GtkWidget    *widget,
                       cairo_t      *cr,
                       InkLevelData *inklevel)
{
  GtkStyleContext        *context;
  gboolean                is_empty = TRUE;
  gchar                  *tooltip_text = NULL;
  gint                    width;
  gint                    height;
  int                     i;

  context = gtk_widget_get_style_context (widget);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_background (context, cr, 0, 0, width, height);

  if (inklevel->marker_levels && inklevel->marker_colors && inklevel->marker_names && inklevel->marker_types)
    {
      GSList   *markers = NULL;
      GSList   *tmp_list = NULL;
      gchar   **marker_levelsv = NULL;
      gchar   **marker_colorsv = NULL;
      gchar   **marker_namesv = NULL;
      gchar   **marker_typesv = NULL;
      gchar    *tmp = NULL;

      gtk_style_context_save (context);

      marker_levelsv = g_strsplit (inklevel->marker_levels, ",", -1);
      marker_colorsv = g_strsplit (inklevel->marker_colors, ",", -1);
      marker_namesv = g_strsplit (inklevel->marker_names, ",", -1);
      marker_typesv = g_strsplit (inklevel->marker_types, ",", -1);

      if (g_strv_length (marker_levelsv) == g_strv_length (marker_colorsv) &&
          g_strv_length (marker_colorsv) == g_strv_length (marker_namesv) &&
          g_strv_length (marker_namesv) == g_strv_length (marker_typesv))
        {
          for (i = 0; i < g_strv_length (marker_levelsv); i++)
            {
              MarkerItem *marker;

              if (g_strcmp0 (marker_typesv[i], "ink") == 0 ||
                  g_strcmp0 (marker_typesv[i], "toner") == 0 ||
                  g_strcmp0 (marker_typesv[i], "inkCartridge") == 0 ||
                  g_strcmp0 (marker_typesv[i], "tonerCartridge") == 0)
                {
                  marker = g_new0 (MarkerItem, 1);
                  marker->type = g_strdup (marker_typesv[i]);
                  marker->name = g_strdup (marker_namesv[i]);
                  marker->color = g_strdup (marker_colorsv[i]);
                  marker->level = atoi (marker_levelsv[i]);

                  markers = g_slist_prepend (markers, marker);
                }
            }

            markers = g_slist_sort (markers, markers_cmp);

            for (tmp_list = markers; tmp_list; tmp_list = tmp_list->next)
              {
                GdkRGBA color = {0.0, 0.0, 0.0, 1.0};
                double  display_value;
                int     value;

                value = ((MarkerItem*) tmp_list->data)->level;

                gdk_rgba_parse (&color, ((MarkerItem*) tmp_list->data)->color);
                tone_down_color (&color, 1.0, 0.6, 0.9);

                if (value > 0)
                  {
                    display_value = value / 100.0 * (width - 3.0);
                    gdk_cairo_set_source_rgba (cr, &color);
                    cairo_rectangle (cr, 2.0, 2.0, display_value, SUPPLY_BAR_HEIGHT);
                    cairo_fill (cr);

                    tone_down_color (&color, 1.0, 1.0, 0.85);
                    gdk_cairo_set_source_rgba (cr, &color);
                    cairo_set_line_width (cr, 1.0);
                    cairo_rectangle (cr, 1.5, 1.5, display_value, SUPPLY_BAR_HEIGHT + 1);
                    cairo_stroke (cr);

                    is_empty = FALSE;
                  }

                if (tooltip_text)
                  {
                    tmp = g_strdup_printf ("%s\n%s",
                                           tooltip_text,
                                           ((MarkerItem*) tmp_list->data)->name);
                    g_free (tooltip_text);
                    tooltip_text = tmp;
                    tmp = NULL;
                  }
                else
                  tooltip_text = g_strdup_printf ("%s",
                                                  ((MarkerItem*) tmp_list->data)->name);
              }

            gtk_render_frame (context, cr, 1, 1, width - 1, SUPPLY_BAR_HEIGHT);

            for (tmp_list = markers; tmp_list; tmp_list = tmp_list->next)
              {
                g_free (((MarkerItem*) tmp_list->data)->name);
                g_free (((MarkerItem*) tmp_list->data)->type);
                g_free (((MarkerItem*) tmp_list->data)->color);
              }
            g_slist_free_full (markers, g_free);
          }

        gtk_style_context_restore (context);

    if (tooltip_text)
      {
        gtk_widget_set_tooltip_text (widget, tooltip_text);
        g_free (tooltip_text);
      }
    else
      {
        gtk_widget_set_tooltip_text (widget, NULL);
        gtk_widget_set_has_tooltip (widget, FALSE);
      }
    }

  if (is_empty)
    {
      GtkWidget *frame = gtk_widget_get_parent (widget);
      gtk_style_context_add_class (gtk_widget_get_style_context (frame),
                                   "background");
    }
  return TRUE;
}

static void
details_dialog_cb (GtkDialog *dialog,
                   gint       response_id,
                   gpointer   user_data)
{
  PpPrinterEntry *self = PP_PRINTER_ENTRY (user_data);

  pp_details_dialog_free (self->pp_details_dialog);
  self->pp_details_dialog = NULL;

  g_signal_emit_by_name (self, "printer-changed");
}

static void
details_dialog_free_cb (GtkDialog *dialog,
                        gint       response_id,
                        gpointer   user_data)
{
  pp_details_dialog_free (PP_DETAILS_DIALOG (dialog));
}

static void
on_show_printer_details_dialog (GtkButton      *button,
                                PpPrinterEntry *self)
{
  self->pp_details_dialog = pp_details_dialog_new (
    GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
    self->printer_name,
    self->printer_location,
    self->printer_hostname,
    self->printer_make_and_model,
    self->is_authorized);

  g_signal_connect (self->pp_details_dialog, "response", G_CALLBACK (details_dialog_cb), self);
  gtk_widget_show_all (GTK_WIDGET (self->pp_details_dialog));
}

static void
printer_options_dialog_cb (GtkDialog *dialog,
                           gint       response_id,
                           gpointer   user_data)
{
  PpPrinterEntry *self = PP_PRINTER_ENTRY (user_data);

  pp_options_dialog_free (self->pp_options_dialog);
  self->pp_options_dialog = NULL;
}

static void
printer_options_dialog_free_cb (GtkDialog *dialog,
                                gint       response_id,
                                gpointer   user_data)
{
  pp_options_dialog_free ((PpOptionsDialog *) user_data);
}

static void
on_show_printer_options_dialog (GtkButton      *button,
                                PpPrinterEntry *self)
{
  self->pp_options_dialog = pp_options_dialog_new (
    GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
    printer_options_dialog_cb,
    self,
    self->printer_name,
    self->is_authorized);
}

static void
set_as_default_printer (GtkToggleButton *button,
                        PpPrinterEntry  *self)
{
  printer_set_default (self->printer_name);

  g_signal_emit_by_name (self, "printer-changed");
}

static void
check_clean_heads_maintenance_command_cb (GObject      *source_object,
                                          GAsyncResult *res,
                                          gpointer      user_data)
{
  PpMaintenanceCommand *command = (PpMaintenanceCommand *) source_object;
  gboolean              is_supported = FALSE;
  GError               *error = NULL;

  is_supported = pp_maintenance_command_is_supported_finish (command, res, &error);
  if (error != NULL)
    {
      g_debug ("Could not check 'Clean' maintenance command: %s", error->message);
      g_error_free (error);
      goto out;
    }

  if (is_supported)
    {
      PpPrinterEntry *self = PP_PRINTER_ENTRY (user_data);
      gtk_widget_show (GTK_WIDGET (self->clean_heads_menuitem));
    }

 out:
  g_object_unref (source_object);
}

static void
check_clean_heads_maintenance_command (PpPrinterEntry *self)
{
  if (self->clean_command == NULL)
    return;

  g_object_ref (self->clean_command);
  self->check_clean_heads_cancellable = g_cancellable_new ();

  pp_maintenance_command_is_supported_async (self->clean_command,
                                             self->check_clean_heads_cancellable,
                                             check_clean_heads_maintenance_command_cb,
                                             self);
}

static void
clean_heads_maintenance_command_cb (GObject      *source_object,
                                    GAsyncResult *res,
                                    gpointer      user_data)
{
  PpPrinterEntry       *self = PP_PRINTER_ENTRY (user_data);
  PpMaintenanceCommand *command = (PpMaintenanceCommand *) source_object;
  GError               *error = NULL;

  if (!pp_maintenance_command_execute_finish (command, res, &error))
    {
      g_warning ("Error cleaning print heads for %s: %s", self->printer_name, error->message);
      g_error_free (error);
    }
  g_object_unref (source_object);
}

static void
clean_heads (GtkButton *button,
             PpPrinterEntry *self)
{
  if (self->clean_command == NULL)
    return;

  g_object_ref (self->clean_command);
  pp_maintenance_command_execute_async (self->clean_command,
                                        NULL,
                                        clean_heads_maintenance_command_cb,
                                        self);
}

static void
remove_printer (GtkButton      *button,
                PpPrinterEntry *self)
{
  printer_delete (self->printer_name);
}

static void
get_jobs_cb (GObject      *source_object,
             GAsyncResult *result,
             gpointer      user_data)
{
  PpPrinterEntry *self;
  PpPrinter      *printer = PP_PRINTER (source_object);
  GError         *error = NULL;
  GList          *jobs;
  gchar          *button_label;
  gint            num_jobs;

  jobs = pp_printer_get_jobs_finish (printer, result, &error);
  num_jobs = g_list_length (jobs);

  g_object_unref (source_object);
  g_list_free_full (jobs, (GDestroyNotify) g_object_unref);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Could not get jobs: %s", error->message);
        }

      g_error_free (error);
      return;
    }

  if (num_jobs == 0)
    {
      /* Translators: This is the label of the button that opens the Jobs Dialog. */
      button_label = g_strdup (_("No Active Jobs"));
    }
  else
    {
      /* Translators: This is the label of the button that opens the Jobs Dialog. */
      button_label = g_strdup_printf (ngettext ("%u Job", "%u Jobs", num_jobs), num_jobs);
    }

  self = PP_PRINTER_ENTRY (user_data);

  gtk_button_set_label (GTK_BUTTON (self->show_jobs_dialog_button), button_label);
  gtk_widget_set_sensitive (self->show_jobs_dialog_button, num_jobs > 0);

  if (self->pp_jobs_dialog != NULL)
    {
      pp_jobs_dialog_update (self->pp_jobs_dialog);
    }

  g_free (button_label);

  g_clear_object (&self->get_jobs_cancellable);
}

void
pp_printer_entry_update_jobs_count (PpPrinterEntry *self)
{
  PpPrinter *printer;

  if (self->get_jobs_cancellable != NULL)
    {
      g_cancellable_cancel (self->get_jobs_cancellable);
      g_clear_object (&self->get_jobs_cancellable);
    }

  self->get_jobs_cancellable = g_cancellable_new ();

  printer = pp_printer_new (self->printer_name);
  pp_printer_get_jobs_async (printer,
                             TRUE,
                             CUPS_WHICHJOBS_ACTIVE,
                             self->get_jobs_cancellable,
                             get_jobs_cb,
                             self);
}

static void
jobs_dialog_response_cb (GtkDialog  *dialog,
                         gint        response_id,
                         gpointer    user_data)
{
  PpPrinterEntry *self = (PpPrinterEntry*) user_data;

  if (self->pp_jobs_dialog != NULL)
    {
      pp_jobs_dialog_free (self->pp_jobs_dialog);
      self->pp_jobs_dialog = NULL;
    }
}

static void
printer_jobs_dialog_free_cb (GtkDialog *dialog,
                             gint       response_id,
                             gpointer   user_data)
{
  pp_jobs_dialog_free ((PpJobsDialog *) user_data);
}

static void
show_jobs_dialog (GtkButton *button,
                  gpointer   user_data)
{
  PpPrinterEntry *self = PP_PRINTER_ENTRY (user_data);

  self->pp_jobs_dialog = pp_jobs_dialog_new (
    GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
    jobs_dialog_response_cb,
    self,
    self->printer_name);
}

enum
{
  PRINTER_READY = 3,
  PRINTER_PROCESSING,
  PRINTER_STOPPED
};

static void
restart_printer (GtkButton      *button,
                 PpPrinterEntry *self)
{
  if (self->printer_state == PRINTER_STOPPED)
    printer_set_enabled (self->printer_name, TRUE);

  if (!self->is_accepting_jobs)
    printer_set_accepting_jobs (self->printer_name, TRUE, NULL);

  g_signal_emit_by_name (self, "printer-changed");
}

PpPrinterEntry *
pp_printer_entry_new (cups_dest_t  printer,
                      gboolean     is_authorized)
{
  PpPrinterEntry *self;
  InkLevelData   *inklevel;
  cups_ptype_t    printer_type = 0;
  gboolean        is_accepting_jobs;
  gchar          *instance;
  gchar          *printer_uri = NULL;
  gchar          *location = NULL;
  gchar          *printer_icon_name = NULL;
  gchar          *default_icon_name = NULL;
  gchar          *printer_make_and_model = NULL;
  gchar          *reason = NULL;
  gchar         **printer_reasons = NULL;
  gchar          *status = NULL;
  gchar          *printer_status = NULL;
  int             i, j;
  static const char * const reasons[] =
    {
      "toner-low",
      "toner-empty",
      "developer-low",
      "developer-empty",
      "marker-supply-low",
      "marker-supply-empty",
      "cover-open",
      "door-open",
      "media-low",
      "media-empty",
      "offline",
      "paused",
      "marker-waste-almost-full",
      "marker-waste-full",
      "opc-near-eol",
      "opc-life-over"
    };
  static const char * statuses[] =
    {
      /* Translators: The printer is low on toner */
      N_("Low on toner"),
      /* Translators: The printer has no toner left */
      N_("Out of toner"),
      /* Translators: "Developer" is a chemical for photo development,
       * http://en.wikipedia.org/wiki/Photographic_developer */
      N_("Low on developer"),
      /* Translators: "Developer" is a chemical for photo development,
       * http://en.wikipedia.org/wiki/Photographic_developer */
      N_("Out of developer"),
      /* Translators: "marker" is one color bin of the printer */
      N_("Low on a marker supply"),
      /* Translators: "marker" is one color bin of the printer */
      N_("Out of a marker supply"),
      /* Translators: One or more covers on the printer are open */
      N_("Open cover"),
      /* Translators: One or more doors on the printer are open */
      N_("Open door"),
      /* Translators: At least one input tray is low on media */
      N_("Low on paper"),
      /* Translators: At least one input tray is empty */
      N_("Out of paper"),
      /* Translators: The printer is offline */
      NC_("printer state", "Offline"),
      /* Translators: Someone has stopped the Printer */
      NC_("printer state", "Stopped"),
      /* Translators: The printer marker supply waste receptacle is almost full */
      N_("Waste receptacle almost full"),
      /* Translators: The printer marker supply waste receptacle is full */
      N_("Waste receptacle full"),
      /* Translators: Optical photo conductors are used in laser printers */
      N_("The optical photo conductor is near end of life"),
      /* Translators: Optical photo conductors are used in laser printers */
      N_("The optical photo conductor is no longer functioning")
    };

  self = g_object_new (PP_PRINTER_ENTRY_TYPE, "printer-name", printer.name, NULL);

  inklevel = g_slice_new0 (InkLevelData);

  if (printer.instance)
    {
      instance = g_strdup_printf ("%s / %s", printer.name, printer.instance);
    }
  else
    {
      instance = g_strdup (printer.name);
    }

  self->printer_state = PRINTER_READY;

  for (i = 0; i < printer.num_options; i++)
    {
      if (g_strcmp0 (printer.options[i].name, "device-uri") == 0)
        self->printer_uri = printer.options[i].value;
      else if (g_strcmp0 (printer.options[i].name, "printer-uri-supported") == 0)
        printer_uri = printer.options[i].value;
      else if (g_strcmp0 (printer.options[i].name, "printer-type") == 0)
        printer_type = atoi (printer.options[i].value);
      else if (g_strcmp0 (printer.options[i].name, "printer-location") == 0)
        location = printer.options[i].value;
      else if (g_strcmp0 (printer.options[i].name, "printer-state-reasons") == 0)
        reason = printer.options[i].value;
      else if (g_strcmp0 (printer.options[i].name, "marker-names") == 0)
        inklevel->marker_names = g_strcompress (printer.options[i].value);
      else if (g_strcmp0 (printer.options[i].name, "marker-levels") == 0)
        inklevel->marker_levels = g_strdup (printer.options[i].value);
      else if (g_strcmp0 (printer.options[i].name, "marker-colors") == 0)
        inklevel->marker_colors = g_strdup (printer.options[i].value);
      else if (g_strcmp0 (printer.options[i].name, "marker-types") == 0)
        inklevel->marker_types = g_strdup (printer.options[i].value);
      else if (g_strcmp0 (printer.options[i].name, "printer-make-and-model") == 0)
        printer_make_and_model = printer.options[i].value;
      else if (g_strcmp0 (printer.options[i].name, "printer-state") == 0)
        self->printer_state = atoi (printer.options[i].value);
      else if (g_strcmp0 (printer.options[i].name, "printer-is-accepting-jobs") == 0)
        {
          if (g_strcmp0 (printer.options[i].value, "true") == 0)
            is_accepting_jobs = TRUE;
          else
            is_accepting_jobs = FALSE;
        }
    }

  /* Find the first of the most severe reasons
   * and show it in the status field
   */
  if (reason && !g_str_equal (reason, "none"))
    {
      int errors = 0, warnings = 0, reports = 0;
      int error_index = -1, warning_index = -1, report_index = -1;

      printer_reasons = g_strsplit (reason, ",", -1);
      for (i = 0; i < g_strv_length (printer_reasons); i++)
        {
          for (j = 0; j < G_N_ELEMENTS (reasons); j++)
            if (strncmp (printer_reasons[i], reasons[j], strlen (reasons[j])) == 0)
                {
                  if (g_str_has_suffix (printer_reasons[i], "-report"))
                    {
                      if (reports == 0)
                        report_index = j;
                      reports++;
                    }
                  else if (g_str_has_suffix (printer_reasons[i], "-warning"))
                    {
                      if (warnings == 0)
                        warning_index = j;
                      warnings++;
                    }
                  else
                    {
                      if (errors == 0)
                        error_index = j;
                      errors++;
                    }
                }
        }
      g_strfreev (printer_reasons);

      if (error_index >= 0)
        status = g_strdup (_(statuses[error_index]));
      else if (warning_index >= 0)
        status = g_strdup (_(statuses[warning_index]));
      else if (report_index >= 0)
        status = g_strdup (_(statuses[report_index]));
    }

  if ((self->printer_state == PRINTER_STOPPED || !is_accepting_jobs) &&
      status != NULL && status[0] != '\0')
    {
      gtk_label_set_label (self->error_status, status);
      gtk_widget_set_visible (GTK_WIDGET (self->printer_error), TRUE);
    }

  switch (self->printer_state)
    {
      case PRINTER_READY:
        if (is_accepting_jobs)
          {
            /* Translators: Printer's state (can start new job without waiting) */
            printer_status = g_strdup ( C_("printer state", "Ready"));
          }
        else
          {
            /* Translators: Printer's state (printer is ready but doesn't accept new jobs) */
            printer_status = g_strdup ( C_("printer state", "Does not accept jobs"));
          }
        break;
      case PRINTER_PROCESSING:
        /* Translators: Printer's state (jobs are processing) */
        printer_status = g_strdup ( C_("printer state", "Processing"));
        break;
      case PRINTER_STOPPED:
        /* Translators: Printer's state (no jobs can be processed) */
        printer_status = g_strdup ( C_("printer state", "Stopped"));
        break;
    }

  if (printer_is_local (printer_type, self->printer_uri))
    printer_icon_name = g_strdup ("printer");
  else
    printer_icon_name = g_strdup ("printer-network");

  g_object_set (self, "printer-location", location, NULL);

  self->is_accepting_jobs = is_accepting_jobs;
  self->is_authorized = is_authorized;

  self->printer_hostname = printer_get_hostname (printer_type, self->printer_uri, printer_uri);

  self->clean_command = pp_maintenance_command_new (self->printer_name,
                                                    "Clean",
                                                    "all",
                                                    /* Translators: Name of job which makes printer to clean its heads */
                                                    _("Clean print heads"));
  check_clean_heads_maintenance_command (self);

  gtk_image_set_from_icon_name (self->printer_icon, printer_icon_name, GTK_ICON_SIZE_DIALOG);
  gtk_label_set_text (self->printer_status, printer_status);
  g_free (printer_status);
  gtk_label_set_text (self->printer_name_label, instance);
  g_signal_handlers_block_by_func (self->printer_default_checkbutton, set_as_default_printer, self);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->printer_default_checkbutton), printer.is_default);
  g_signal_handlers_unblock_by_func (self->printer_default_checkbutton, set_as_default_printer, self);

  self->printer_make_and_model = sanitize_printer_model (printer_make_and_model);

  if (self->printer_make_and_model == NULL && self->printer_make_and_model[0] != '\0')
    {
      gtk_widget_hide (GTK_WIDGET (self->printer_model_label));
      gtk_widget_hide (GTK_WIDGET (self->printer_model));
    }
  else
    {
      gtk_label_set_text (self->printer_model, self->printer_make_and_model);
    }

  if (location != NULL && location[0] == '\0')
    {
      gtk_widget_hide (GTK_WIDGET (self->printer_location_label));
      gtk_widget_hide (GTK_WIDGET (self->printer_location_address_label));
    }
  else
    {
      gtk_label_set_text (self->printer_location_address_label, location);
    }

  g_signal_connect (self->supply_drawing_area, "draw", G_CALLBACK (supply_levels_draw_cb), inklevel);

  pp_printer_entry_update_jobs_count (self);

  gtk_widget_set_sensitive (GTK_WIDGET (self->printer_default_checkbutton), self->is_authorized);
  gtk_widget_set_sensitive (GTK_WIDGET (self->remove_printer_menuitem), self->is_authorized);

  g_free (instance);
  g_free (printer_icon_name);
  g_free (default_icon_name);

  return self;
}

static void
pp_printer_entry_dispose (GObject *object)
{
  PpPrinterEntry *self = PP_PRINTER_ENTRY (object);

  if (self->pp_details_dialog != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->pp_details_dialog, self);
      g_signal_connect (self->pp_details_dialog, "response", G_CALLBACK (details_dialog_free_cb), NULL);
    }

  if (self->pp_options_dialog != NULL)
    pp_options_dialog_set_callback (self->pp_options_dialog, printer_options_dialog_free_cb, self->pp_options_dialog);

  if (self->pp_jobs_dialog != NULL)
    pp_jobs_dialog_set_callback (self->pp_jobs_dialog, printer_jobs_dialog_free_cb, self->pp_jobs_dialog);

  g_clear_pointer (&self->printer_name, g_free);
  g_clear_pointer (&self->printer_location, g_free);
  g_clear_pointer (&self->printer_make_and_model, g_free);
  g_clear_pointer (&self->printer_hostname, g_free);

  if (self->get_jobs_cancellable != NULL)
    {
      g_cancellable_cancel (self->get_jobs_cancellable);
      g_clear_object (&self->get_jobs_cancellable);
    }

  if (self->check_clean_heads_cancellable)
    {
      g_cancellable_cancel (self->check_clean_heads_cancellable);
      g_clear_object (&self->check_clean_heads_cancellable);
    }
  g_clear_object (&self->clean_command);

  G_OBJECT_CLASS (pp_printer_entry_parent_class)->dispose (object);
}

static void
pp_printer_entry_class_init (PpPrinterEntryClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/printers/printer-entry.ui");

  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_icon);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_name_label);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_status);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_model_label);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_model);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_location_label);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_location_address_label);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, supply_drawing_area);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_default_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, show_jobs_dialog_button);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, clean_heads_menuitem);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, remove_printer_menuitem);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, error_status);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_error);

  gtk_widget_class_bind_template_callback (widget_class, on_show_printer_details_dialog);
  gtk_widget_class_bind_template_callback (widget_class, on_show_printer_options_dialog);
  gtk_widget_class_bind_template_callback (widget_class, set_as_default_printer);
  gtk_widget_class_bind_template_callback (widget_class, clean_heads);
  gtk_widget_class_bind_template_callback (widget_class, remove_printer);
  gtk_widget_class_bind_template_callback (widget_class, show_jobs_dialog);
  gtk_widget_class_bind_template_callback (widget_class, restart_printer);

  object_class->get_property = pp_printer_entry_get_property;
  object_class->set_property = pp_printer_entry_set_property;
  object_class->dispose = pp_printer_entry_dispose;

  g_object_class_install_property (object_class,
                                   PROP_PRINTER_NAME,
                                   g_param_spec_string ("printer-name",
                                                        "Printer Name",
                                                        "The Printer unique name",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_PRINTER_LOCATION,
                                   g_param_spec_string ("printer-location",
                                                        "Printer Location",
                                                        "Printer location string",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  signals[IS_DEFAULT_PRINTER] =
    g_signal_new ("printer-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (PpPrinterEntryClass, printer_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}
