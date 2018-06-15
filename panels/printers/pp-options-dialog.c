/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
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
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <cups/cups.h>
#include <cups/ppd.h>

#include "pp-options-dialog.h"
#include "pp-maintenance-command.h"
#include "pp-ppd-option-widget.h"
#include "pp-ipp-option-widget.h"
#include "pp-utils.h"
#include "pp-printer.h"

struct _PpOptionsDialog {
  GtkBuilder *builder;
  GtkWidget  *parent;

  GtkWidget  *dialog;

  UserResponseCallback user_callback;
  gpointer             user_data;

  gchar       *printer_name;

  gchar       *ppd_filename;
  gboolean     ppd_filename_set;

  cups_dest_t *destination;
  gboolean     destination_set;

  GHashTable  *ipp_attributes;
  gboolean     ipp_attributes_set;

  gboolean     populating_dialog;

  GtkResponseType response;

  gboolean sensitive;
};

static void pp_options_dialog_hide (PpOptionsDialog *dialog);

enum
{
  CATEGORY_IDS_COLUMN = 0,
  CATEGORY_NAMES_COLUMN
};

/* These lists come from Gtk+ */
static const struct {
  const char *keyword;
  const char *translation;
} ppd_option_translations[] = {
  { "Duplex", N_("Two Sided") },
  { "MediaType", N_("Paper Type") },
  { "InputSlot", N_("Paper Source") },
  { "OutputBin", N_("Output Tray") },
  { "Resolution", N_("Resolution") },
  { "PreFilter", N_("GhostScript pre-filtering") },
};

/* keep sorted when changing */
static const char *page_setup_option_whitelist[] = {
  "InputSlot",
  "MediaType",
  "OutputBin",
  "PageSize",
};

/* keep sorted when changing */
static const char *color_option_whitelist[] = {
  "BRColorEnhancement",
  "BRColorMatching",
  "BRColorMatching",
  "BRColorMode",
  "BRGammaValue",
  "BRImprovedGray",
  "BlackSubstitution",
  "ColorModel",
  "HPCMYKInks",
  "HPCSGraphics",
  "HPCSImages",
  "HPCSText",
  "HPColorSmart",
  "RPSBlackMode",
  "RPSBlackOverPrint",
  "Rcmyksimulation",
};

/* keep sorted when changing */
static const char *color_group_whitelist[] = {
  "Color",
  "Color1",
  "Color2",
  "ColorBalance",
  "ColorPage",
  "ColorSettings1",
  "ColorSettings2",
  "ColorSettings3",
  "ColorSettings4",
  "EPColorSettings",
  "FPColorWise1",
  "FPColorWise2",
  "FPColorWise3",
  "FPColorWise4",
  "FPColorWise5",
  "HPCMYKInksPanel",
  "HPColorOptions",
  "HPColorOptionsPanel",
  "HPColorQualityOptionsPanel",
  "ManualColor",
};

/* keep sorted when changing */
static const char *image_quality_option_whitelist[] = {
  "BRDocument",
  "BRHalfTonePattern",
  "BRNormalPrt",
  "BRPrintQuality",
  "BitsPerPixel",
  "Darkness",
  "Dithering",
  "EconoMode",
  "Economode",
  "HPEconoMode",
  "HPEdgeControl",
  "HPGraphicsHalftone",
  "HPHalftone",
  "HPImagingOptions",
  "HPLJDensity",
  "HPPhotoHalftone",
  "HPPrintQualityOptions",
  "HPResolutionOptions",
  "OutputMode",
  "REt",
  "RPSBitsPerPixel",
  "RPSDitherType",
  "Resolution",
  "ScreenLock",
  "Smoothing",
  "TonerSaveMode",
  "UCRGCRForImage",
};

/* keep sorted when changing */
static const char *image_quality_group_whitelist[] = {
  "EPQualitySettings",
  "FPImageQuality1",
  "FPImageQuality2",
  "FPImageQuality3",
  "ImageQualityPage",
  "Quality",
};

/* keep sorted when changing */
static const char * finishing_option_whitelist[] = {
  "BindColor",
  "BindEdge",
  "BindType",
  "BindWhen",
  "Booklet",
  "FoldType",
  "FoldWhen",
  "HPStaplerOptions",
  "Jog",
  "Slipsheet",
  "Sorter",
  "StapleLocation",
  "StapleOrientation",
  "StapleWhen",
  "StapleX",
  "StapleY",
};

/* keep sorted when changing */
static const char *job_group_whitelist[] = {
  "JobHandling",
  "JobLog",
};

/* keep sorted when changing */
static const char *finishing_group_whitelist[] = {
  "Booklet",
  "BookletCover",
  "BookletModeOptions",
  "FPFinishing1",
  "FPFinishing2",
  "FPFinishing3",
  "FPFinishing4",
  "Finishing",
  "FinishingOptions",
  "FinishingPage",
  "HPBookletPanel",
  "HPFinishing",
  "HPFinishingOptions",
  "HPFinishingPanel",
};

/* keep sorted when changing */
static const char *installable_options_group_whitelist[] = {
  "InstallableOptions",
};

/* keep sorted when changing */
static const char *page_setup_group_whitelist[] = {
  "HPMarginAndLayout",
  "OutputControl",
  "PaperHandling",
  "Paper",
  "Source",
};

/* keep sorted when changing */
static const char *ppd_option_blacklist[] = {
  "Collate",
  "Copies",
  "Duplex",
  "HPManualDuplexOrientation",
  "HPManualDuplexSwitch",
  "OutputOrder",
  "PageRegion"
};

static int
strptr_cmp (const void *a,
	    const void *b)
{
  char **aa = (char **)a;
  char **bb = (char **)b;
  return strcmp (*aa, *bb);
}

static gboolean
string_in_table (gchar       *str,
		 const gchar *table[],
		 gint         table_len)
{
  return bsearch (&str, table, table_len, sizeof (char *), (void *)strptr_cmp) != NULL;
}

#define STRING_IN_TABLE(_str, _table) (string_in_table (_str, _table, G_N_ELEMENTS (_table)))

static const gchar *
ppd_option_name_translate (ppd_option_t *option)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (ppd_option_translations); i++)
    {
      if (g_strcmp0 (ppd_option_translations[i].keyword, option->keyword) == 0)
	return _(ppd_option_translations[i].translation);
    }

  return option->text;
}

static gint
grid_get_height (GtkWidget *grid)
{
  GList *children;
  GList *child;
  gint   height = 0;
  gint   top_attach = 0;
  gint   max = 0;

  children = gtk_container_get_children (GTK_CONTAINER (grid));
  for (child = children; child; child = g_list_next (child))
    {
      gtk_container_child_get (GTK_CONTAINER (grid), child->data,
                               "top-attach", &top_attach,
                               "height", &height,
                               NULL);

      if (height + top_attach > max)
        max = height + top_attach;
    }

  g_list_free (children);

  return max;
}

static gboolean
grid_is_empty (GtkWidget *grid)
{
  GList *children;

  children = gtk_container_get_children (GTK_CONTAINER (grid));
  if (children)
    {
      g_list_free (children);
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}

static GtkWidget *
ipp_option_add (IPPAttribute *attr_supported,
                IPPAttribute *attr_default,
                const gchar  *option_name,
                const gchar  *option_display_name,
                const gchar  *printer_name,
                GtkWidget    *grid,
                gboolean      sensitive)
{
  GtkStyleContext *context;
  GtkWidget       *widget;
  GtkWidget       *label;
  gint             position;

  widget = (GtkWidget *) pp_ipp_option_widget_new (attr_supported,
                                                   attr_default,
                                                   option_name,
                                                   printer_name);
  if (widget)
    {
      gtk_widget_set_sensitive (widget, sensitive);
      position = grid_get_height (grid);

      label = gtk_label_new (option_display_name);
      context = gtk_widget_get_style_context (label);
      gtk_style_context_add_class (context, "dim-label");
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_widget_set_margin_start (label, 10);
      gtk_grid_attach (GTK_GRID (grid), label, 0, position, 1, 1);

      gtk_widget_set_margin_start (widget, 20);
      gtk_grid_attach (GTK_GRID (grid), widget, 1, position, 1, 1);
    }

  return widget;
}

static GtkWidget *
ppd_option_add (ppd_option_t  option,
                const gchar  *printer_name,
                GtkWidget    *grid,
                gboolean      sensitive)
{
  GtkStyleContext *context;
  GtkWidget       *widget;
  GtkWidget       *label;
  gint             position;

  widget = (GtkWidget *) pp_ppd_option_widget_new (&option, printer_name);
  if (widget)
    {
      gtk_widget_set_sensitive (widget, sensitive);
      position = grid_get_height (grid);

      label = gtk_label_new (ppd_option_name_translate (&option));
      context = gtk_widget_get_style_context (label);
      gtk_style_context_add_class (context, "dim-label");
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_widget_set_margin_start (label, 10);
      gtk_grid_attach (GTK_GRID (grid), label, 0, position, 1, 1);

      gtk_widget_set_margin_start (widget, 20);
      gtk_grid_attach (GTK_GRID (grid), widget, 1, position, 1, 1);
    }

  return widget;
}

static GtkWidget *
tab_grid_new ()
{
  GtkWidget *grid;

  grid = gtk_grid_new ();
  gtk_container_set_border_width (GTK_CONTAINER (grid), 20);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 15);

  return grid;
}

static void
tab_add (const gchar *tab_name,
         GtkWidget   *options_notebook,
         GtkTreeView *treeview,
         GtkWidget   *grid)
{
  GtkListStore *store;
  GtkTreeIter   iter;
  GtkWidget    *scrolled_window;
  gboolean      unref_store = FALSE;
  gint          id;

  if (!grid_is_empty (grid))
    {
      scrolled_window = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_AUTOMATIC);
      gtk_container_add (GTK_CONTAINER (scrolled_window), grid);

      id = gtk_notebook_append_page (GTK_NOTEBOOK (options_notebook),
                                     scrolled_window,
                                     NULL);

      if (id >= 0)
        {
          store = GTK_LIST_STORE (gtk_tree_view_get_model (treeview));
          if (!store)
            {
              store = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
              unref_store = TRUE;
            }

          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter,
                              CATEGORY_IDS_COLUMN, id,
                              CATEGORY_NAMES_COLUMN, tab_name,
                              -1);

          if (unref_store)
            {
              gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (store));
              g_object_unref (store);
            }
        }
    }
  else
    {
      g_object_ref_sink (grid);
      g_object_unref (grid);
    }
}

static void
category_selection_changed_cb (GtkTreeSelection *selection,
                               gpointer          user_data)
{
  PpOptionsDialog *dialog = (PpOptionsDialog *) user_data;
  GtkTreeModel    *model;
  GtkTreeIter      iter;
  GtkWidget       *options_notebook;
  gint             id = -1;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
			  CATEGORY_IDS_COLUMN, &id,
			  -1);
    }

  if (id >= 0)
    {
      options_notebook = (GtkWidget*)
        gtk_builder_get_object (dialog->builder, "options-notebook");

      gtk_notebook_set_current_page (GTK_NOTEBOOK (options_notebook), id);
    }
}

static void
populate_options_real (PpOptionsDialog *dialog)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeView      *treeview;
  GtkTreeIter       iter;
  ppd_file_t       *ppd_file;
  GtkWidget        *notebook;
  GtkWidget        *grid;
  GtkWidget        *general_tab_grid = tab_grid_new ();
  GtkWidget        *page_setup_tab_grid = tab_grid_new ();
  GtkWidget        *installable_options_tab_grid = tab_grid_new ();
  GtkWidget        *job_tab_grid = tab_grid_new ();
  GtkWidget        *image_quality_tab_grid = tab_grid_new ();
  GtkWidget        *color_tab_grid = tab_grid_new ();
  GtkWidget        *finishing_tab_grid = tab_grid_new ();
  GtkWidget        *advanced_tab_grid = tab_grid_new ();
  GtkWidget        *widget;
  gint              i, j;

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "options-spinner");
  gtk_spinner_stop (GTK_SPINNER (widget));

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "stack");
  gtk_stack_set_visible_child_name (GTK_STACK (widget), "main-box");

  treeview = (GtkTreeView *)
    gtk_builder_get_object (dialog->builder, "options-categories-treeview");

  notebook = (GtkWidget *)
    gtk_builder_get_object (dialog->builder, "options-notebook");

  if (dialog->ipp_attributes)
    {
      /* Add number-up option to Page Setup tab */
      ipp_option_add (g_hash_table_lookup (dialog->ipp_attributes,
                                           "number-up-supported"),
                      g_hash_table_lookup (dialog->ipp_attributes,
                                           "number-up-default"),
                      "number-up",
                      /* Translators: This option sets number of pages printed on one sheet */
                      _("Pages per side"),
                      dialog->printer_name,
                      page_setup_tab_grid,
                      dialog->sensitive);

      /* Add sides option to Page Setup tab */
      ipp_option_add (g_hash_table_lookup (dialog->ipp_attributes,
                                           "sides-supported"),
                      g_hash_table_lookup (dialog->ipp_attributes,
                                           "sides-default"),
                      "sides",
                      /* Translators: This option sets whether to print on both sides of paper */
                      _("Two-sided"),
                      dialog->printer_name,
                      page_setup_tab_grid,
                      dialog->sensitive);

      /* Add orientation-requested option to Page Setup tab */
      ipp_option_add (g_hash_table_lookup (dialog->ipp_attributes,
                                           "orientation-requested-supported"),
                      g_hash_table_lookup (dialog->ipp_attributes,
                                           "orientation-requested-default"),
                      "orientation-requested",
                      /* Translators: This option sets orientation of print (portrait, landscape...) */
                      _("Orientation"),
                      dialog->printer_name,
                      page_setup_tab_grid,
                      dialog->sensitive);
    }

  if (dialog->destination && dialog->ppd_filename)
    {
      ppd_file = ppdOpenFile (dialog->ppd_filename);
      ppdLocalize (ppd_file);

      if (ppd_file)
        {
          ppdMarkDefaults (ppd_file);
          cupsMarkOptions (ppd_file,
                           dialog->destination->num_options,
                           dialog->destination->options);

          for (i = 0; i < ppd_file->num_groups; i++)
            {
              for (j = 0; j < ppd_file->groups[i].num_options; j++)
                {
                  grid = NULL;

                  if (STRING_IN_TABLE (ppd_file->groups[i].name,
                                       color_group_whitelist))
                    grid = color_tab_grid;
                  else if (STRING_IN_TABLE (ppd_file->groups[i].name,
                                            image_quality_group_whitelist))
                    grid = image_quality_tab_grid;
                  else if (STRING_IN_TABLE (ppd_file->groups[i].name,
                                            job_group_whitelist))
                    grid = job_tab_grid;
                  else if (STRING_IN_TABLE (ppd_file->groups[i].name,
                                            finishing_group_whitelist))
                    grid = finishing_tab_grid;
                  else if (STRING_IN_TABLE (ppd_file->groups[i].name,
                                            installable_options_group_whitelist))
                    grid = installable_options_tab_grid;
                  else if (STRING_IN_TABLE (ppd_file->groups[i].name,
                                            page_setup_group_whitelist))
                    grid = page_setup_tab_grid;

                  if (!STRING_IN_TABLE (ppd_file->groups[i].options[j].keyword,
                                        ppd_option_blacklist))
                    {
                      if (!grid && STRING_IN_TABLE (ppd_file->groups[i].options[j].keyword,
                                                    color_option_whitelist))
                        grid = color_tab_grid;
                      else if (!grid && STRING_IN_TABLE (ppd_file->groups[i].options[j].keyword,
                                                         image_quality_option_whitelist))
                        grid = image_quality_tab_grid;
                      else if (!grid && STRING_IN_TABLE (ppd_file->groups[i].options[j].keyword,
                                                         finishing_option_whitelist))
                        grid = finishing_tab_grid;
                      else if (!grid && STRING_IN_TABLE (ppd_file->groups[i].options[j].keyword,
                                                         page_setup_option_whitelist))
                        grid = page_setup_tab_grid;

                      if (!grid)
                        grid = advanced_tab_grid;

                      ppd_option_add (ppd_file->groups[i].options[j],
                                      dialog->printer_name,
                                      grid,
                                      dialog->sensitive);
                    }
                }
            }

          ppdClose (ppd_file);
        }
    }

  dialog->ppd_filename_set = FALSE;
  if (dialog->ppd_filename)
    {
      g_unlink (dialog->ppd_filename);
      g_free (dialog->ppd_filename);
      dialog->ppd_filename = NULL;
    }

  dialog->destination_set = FALSE;
  if (dialog->destination)
    {
      cupsFreeDests (1, dialog->destination);
      dialog->destination = NULL;
    }

  dialog->ipp_attributes_set = FALSE;
  if (dialog->ipp_attributes)
    {
      g_hash_table_unref (dialog->ipp_attributes);
      dialog->ipp_attributes = NULL;
    }

  /* Translators: "General" tab contains general printer options */
  tab_add (C_("Printer Option Group", "General"), notebook, treeview, general_tab_grid);

  /* Translators: "Page Setup" tab contains settings related to pages (page size, paper source, etc.) */
  tab_add (C_("Printer Option Group", "Page Setup"), notebook, treeview, page_setup_tab_grid);

  /* Translators: "Installable Options" tab contains settings of presence of installed options (amount of RAM, duplex unit, etc.) */
  tab_add (C_("Printer Option Group", "Installable Options"), notebook, treeview, installable_options_tab_grid);

  /* Translators: "Job" tab contains settings for jobs */
  tab_add (C_("Printer Option Group", "Job"), notebook, treeview, job_tab_grid);

  /* Translators: "Image Quality" tab contains settings for quality of output print (e.g. resolution) */
  tab_add (C_("Printer Option Group", "Image Quality"), notebook, treeview, image_quality_tab_grid);

  /* Translators: "Color" tab contains color settings (e.g. color printing) */
  tab_add (C_("Printer Option Group", "Color"), notebook, treeview, color_tab_grid);

  /* Translators: "Finishing" tab contains finishing settings (e.g. booklet printing) */
  tab_add (C_("Printer Option Group", "Finishing"), notebook, treeview, finishing_tab_grid);

  /* Translators: "Advanced" tab contains all others settings */
  tab_add (C_("Printer Option Group", "Advanced"), notebook, treeview, advanced_tab_grid);

  gtk_widget_show_all (GTK_WIDGET (notebook));

  /* Select the first option group */
  if ((selection = gtk_tree_view_get_selection (treeview)) != NULL)
    {
      g_signal_connect (selection,
                        "changed",
                        G_CALLBACK (category_selection_changed_cb), dialog);

      if ((model = gtk_tree_view_get_model (treeview)) != NULL &&
          gtk_tree_model_get_iter_first (model, &iter))
        gtk_tree_selection_select_iter (selection, &iter);
    }

  dialog->populating_dialog = FALSE;
  if (dialog->response != GTK_RESPONSE_NONE)
    {
      dialog->user_callback (GTK_DIALOG (dialog->dialog), dialog->response, dialog->user_data);
    }
}

static void
printer_get_ppd_cb (const gchar *ppd_filename,
                    gpointer     user_data)
{
  PpOptionsDialog *dialog = (PpOptionsDialog *) user_data;

  if (dialog->ppd_filename)
    {
      g_unlink (dialog->ppd_filename);
      g_free (dialog->ppd_filename);
    }

  dialog->ppd_filename = g_strdup (ppd_filename);
  dialog->ppd_filename_set = TRUE;

  if (dialog->destination_set &&
      dialog->ipp_attributes_set)
    {
      populate_options_real (dialog);
    }
}

static void
get_named_dest_cb (cups_dest_t *dest,
                   gpointer     user_data)
{
  PpOptionsDialog *dialog = (PpOptionsDialog *) user_data;

  if (dialog->destination)
    cupsFreeDests (1, dialog->destination);

  dialog->destination = dest;
  dialog->destination_set = TRUE;

  if (dialog->ppd_filename_set &&
      dialog->ipp_attributes_set)
    {
      populate_options_real (dialog);
    }
}

static void
get_ipp_attributes_cb (GHashTable *table,
                       gpointer    user_data)
{
  PpOptionsDialog *dialog = (PpOptionsDialog *) user_data;

  if (dialog->ipp_attributes)
    g_hash_table_unref (dialog->ipp_attributes);

  dialog->ipp_attributes = table;
  dialog->ipp_attributes_set = TRUE;

  if (dialog->ppd_filename_set &&
      dialog->destination_set)
    {
      populate_options_real (dialog);
    }
}

static void
populate_options (PpOptionsDialog *dialog)
{
  GtkTreeViewColumn  *column;
  GtkCellRenderer    *renderer;
  GtkTreeView        *treeview;
  GtkWidget          *widget;
  /*
   * Options which we need to obtain through an IPP request
   * to be able to fill the options dialog.
   * *-supported - possible values of the option
   * *-default - actual value of the option
   */
  const gchar        *attributes[] =
    { "number-up-supported",
      "number-up-default",
      "sides-supported",
      "sides-default",
      "orientation-requested-supported",
      "orientation-requested-default",
      NULL};

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "stack");
  gtk_stack_set_visible_child_name (GTK_STACK (widget), "progress-box");

  treeview = (GtkTreeView *)
    gtk_builder_get_object (dialog->builder, "options-categories-treeview");

  renderer = gtk_cell_renderer_text_new ();

  column = gtk_tree_view_column_new_with_attributes ("Categories", renderer,
                                                     "text", CATEGORY_NAMES_COLUMN, NULL);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (treeview, column);

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "options-spinner");
  gtk_spinner_start (GTK_SPINNER (widget));

  printer_get_ppd_async (dialog->printer_name,
                         NULL,
                         0,
                         printer_get_ppd_cb,
                         dialog);

  get_named_dest_async (dialog->printer_name,
                        get_named_dest_cb,
                        dialog);

  get_ipp_attributes_async (dialog->printer_name,
                            (gchar **) attributes,
                            get_ipp_attributes_cb,
                            dialog);
}

static void
pp_maintenance_command_execute_cb (GObject      *source_object,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
  PpMaintenanceCommand *command = (PpMaintenanceCommand *) source_object;

  pp_maintenance_command_execute_finish (command, res, NULL);

  g_object_unref (command);
}

static gchar *
get_testprint_filename (const gchar *datadir)
{
  const gchar *testprint[] = { "/data/testprint",
                               "/data/testprint.ps",
                               NULL };
  gchar       *filename = NULL;
  gint         i;

  for (i = 0; testprint[i] != NULL; i++)
    {
      filename = g_strconcat (datadir, testprint[i], NULL);
      if (g_access (filename, R_OK) == 0)
        break;

      g_clear_pointer (&filename, g_free);
    }

  return filename;
}

static void
print_test_page_cb (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  pp_printer_print_file_finish (PP_PRINTER (source_object),
                                result, NULL);

  g_object_unref (source_object);
}

static void
test_page_cb (GtkButton *button,
              gpointer   user_data)
{
  PpOptionsDialog *dialog = (PpOptionsDialog*) user_data;
  gint             i;

  if (dialog->printer_name)
    {
      const gchar  *const dirs[] = { "/usr/share/cups",
                                     "/usr/local/share/cups",
                                     NULL };
      const gchar  *datadir = NULL;
      gchar        *filename = NULL;

      datadir = getenv ("CUPS_DATADIR");
      if (datadir != NULL)
        {
          filename = get_testprint_filename (datadir);
        }
      else
        {
          for (i = 0; dirs[i] != NULL && filename == NULL; i++)
            filename = get_testprint_filename (dirs[i]);
        }

      if (filename != NULL)
        {
          PpPrinter *printer;

          printer = pp_printer_new (dialog->printer_name);
          pp_printer_print_file_async (printer,
                                       filename,
          /* Translators: Name of job which makes printer to print test page */
                                       _("Test Page"),
                                       NULL,
                                       print_test_page_cb,
                                       NULL);

          g_free (filename);
        }
      else
        {
          PpMaintenanceCommand *command;

          command = pp_maintenance_command_new (dialog->printer_name,
                                                "PrintSelfTestPage",
                                                NULL,
          /* Translators: Name of job which makes printer to print test page */
                                                _("Test page"));

          pp_maintenance_command_execute_async (command, NULL, pp_maintenance_command_execute_cb, NULL);
        }
    }
}

static void
options_dialog_response_cb (GtkDialog *_dialog,
                            gint       response_id,
                            gpointer   user_data)
{
  PpOptionsDialog *dialog = (PpOptionsDialog*) user_data;

  pp_options_dialog_hide (dialog);
  dialog->response = response_id;

  if (!dialog->populating_dialog)
    dialog->user_callback (GTK_DIALOG (dialog->dialog), response_id, dialog->user_data);
}

PpOptionsDialog *
pp_options_dialog_new (GtkWindow            *parent,
                       UserResponseCallback  user_callback,
                       gpointer              user_data,
                       gchar                *printer_name,
                       gboolean              sensitive)
{
  PpOptionsDialog *dialog;
  GtkWidget       *test_page_button;
  GError          *error = NULL;
  gchar           *objects[] = { "options-dialog", NULL };
  guint            builder_result;

  dialog = g_new0 (PpOptionsDialog, 1);

  dialog->builder = gtk_builder_new ();
  dialog->parent = GTK_WIDGET (parent);

  builder_result = gtk_builder_add_objects_from_resource (dialog->builder,
                                                          "/org/gnome/control-center/printers/options-dialog.ui",
                                                          objects, &error);

  if (builder_result == 0)
    {
      g_warning ("Could not load ui: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  dialog->dialog = (GtkWidget *) gtk_builder_get_object (dialog->builder, "options-dialog");
  gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), GTK_WINDOW (parent));

  dialog->user_callback = user_callback;
  dialog->user_data = user_data;

  dialog->printer_name = g_strdup (printer_name);

  dialog->ppd_filename = NULL;
  dialog->ppd_filename_set = FALSE;

  dialog->destination = NULL;
  dialog->destination_set = FALSE;

  dialog->ipp_attributes = NULL;
  dialog->ipp_attributes_set = FALSE;

  dialog->response = GTK_RESPONSE_NONE;

  dialog->sensitive = sensitive;

  /* connect signals */
  g_signal_connect (dialog->dialog, "response", G_CALLBACK (options_dialog_response_cb), dialog);
  test_page_button = (GtkWidget*) gtk_builder_get_object (dialog->builder, "print-test-page");
  g_signal_connect (test_page_button, "clicked", G_CALLBACK (test_page_cb), dialog);

  gtk_window_set_title (GTK_WINDOW (dialog->dialog), printer_name);

  gtk_widget_show_all (GTK_WIDGET (dialog->dialog));

  dialog->populating_dialog = TRUE;
  populate_options (dialog);

  return dialog;
}

void
pp_options_dialog_set_callback (PpOptionsDialog      *dialog,
                                UserResponseCallback  user_callback,
                                gpointer              user_data)
{
  if (dialog != NULL)
    {
      dialog->user_callback = user_callback;
      dialog->user_data = user_data;
    }
}

void
pp_options_dialog_free (PpOptionsDialog *dialog)
{
  gtk_widget_destroy (GTK_WIDGET (dialog->dialog));
  dialog->dialog = NULL;

  g_object_unref (dialog->builder);
  dialog->builder = NULL;

  g_free (dialog->printer_name);
  dialog->printer_name = NULL;

  if (dialog->ppd_filename)
    {
      g_unlink (dialog->ppd_filename);
      g_free (dialog->ppd_filename);
      dialog->ppd_filename = NULL;
    }

  if (dialog->destination)
    {
      cupsFreeDests (1, dialog->destination);
      dialog->destination = NULL;
    }

  if (dialog->ipp_attributes)
    {
      g_hash_table_unref (dialog->ipp_attributes);
      dialog->ipp_attributes = NULL;
    }

  g_free (dialog);
}

static void
pp_options_dialog_hide (PpOptionsDialog *dialog)
{
  gtk_widget_hide (GTK_WIDGET (dialog->dialog));
}
