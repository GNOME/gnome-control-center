/* -*- MODE: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Author: Benjamin Kahn <xkahn@zoned.net>
 * Based on capplets/gnome-edit-properties/gnome-edit-properties.c.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gnome.h>

#include "capplet-widget.h"
#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "defaults.h"

extern GtkWidget *capplet;
extern gboolean ignore_changes;

extern EditorDescription *possible_editors;
extern BrowserDescription *possible_browsers;
extern HelpViewDescription *possible_helpviewers;
extern TerminalDescription *possible_terminals;

void
edit_create (void)
{
  GtkWidget *notebook1;
  GtkWidget *frame1;
  GtkWidget *table16;
  GSList *seleditor_group = NULL;
  GtkWidget *radiodefeditor;
  GtkWidget *editorselect;
  GtkWidget *combo_editor;
  GtkWidget *radiocusteditor;
  GtkWidget *table17;
  GtkWidget *editorterminal;
  GtkWidget *editorlineno;
  GtkWidget *table18;
  GtkWidget *label7;
  GtkWidget *editorcommand;
  GtkWidget *hseparator5;
  GtkWidget *label4;
  GtkWidget *frame2;
  GtkWidget *table19;
  GSList *selbrowser_group = NULL;
  GtkWidget *seldefbrowser;
  GtkWidget *browserselect;
  GtkWidget *combo_browser;
  GtkWidget *selcustbrowser;
  GtkWidget *table20;
  GtkWidget *browserterminal;
  GtkWidget *browserremote;
  GtkWidget *table21;
  GtkWidget *label8;
  GtkWidget *browsercommand;
  GtkWidget *hseparator6;
  GtkWidget *label5;
  GtkWidget *frame3;
  GtkWidget *table22;
  GSList *selhelp_group = NULL;
  GtkWidget *seldefview;
  GtkWidget *helpselect;
  GtkWidget *combo_help;
  GtkWidget *selcustview;
  GtkWidget *table23;
  GtkWidget *helpterminal;
  GtkWidget *helpurls;
  GtkWidget *table24;
  GtkWidget *label9;
  GtkWidget *helpcommand;
  GtkWidget *hseparator7;
  GtkWidget *label6;
  GtkWidget *frame4;
  GtkWidget *table25;
  GSList *selterm_group = NULL;
  GtkWidget *seldefterm;
  GtkWidget *termselect;
  GtkWidget *combo_term;
  GtkWidget *selcustterm;
  GtkWidget *table26;
  GtkWidget *table27;
  GtkWidget *label11;
  GtkWidget *label12;
  GtkWidget *termexec;
  GtkWidget *termcommand;
  GtkWidget *hseparator8;
  GtkWidget *label10;
  GtkTooltips *tooltips;

  tooltips = gtk_tooltips_new ();

  capplet = capplet_widget_new();
  ignore_changes = TRUE;

  notebook1 = gtk_notebook_new ();
  gtk_widget_ref (notebook1);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "notebook1", notebook1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (notebook1);
  gtk_container_add (GTK_CONTAINER (capplet), notebook1);

  frame1 = gtk_frame_new (_("Gnome Default Editor"));
  gtk_widget_ref (frame1);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "frame1", frame1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame1);
  gtk_container_add (GTK_CONTAINER (notebook1), frame1);
  gtk_container_set_border_width (GTK_CONTAINER (frame1), 5);

  table16 = gtk_table_new (3, 2, FALSE);
  gtk_widget_ref (table16);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "table16", table16,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table16);
  gtk_container_add (GTK_CONTAINER (frame1), table16);
  gtk_table_set_row_spacings (GTK_TABLE (table16), 10);
  gtk_table_set_col_spacings (GTK_TABLE (table16), 10);

  radiodefeditor = gtk_radio_button_new_with_label (seleditor_group, _("Select an Editor"));
  seleditor_group = gtk_radio_button_group (GTK_RADIO_BUTTON (radiodefeditor));
  gtk_widget_ref (radiodefeditor);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "radiodefeditor", radiodefeditor,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (radiodefeditor);
  gtk_table_attach (GTK_TABLE (table16), radiodefeditor, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, radiodefeditor, _("With this option, you can select a predefined Editor as your default"), NULL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiodefeditor), TRUE);

  editorselect = gtk_combo_new ();
  gtk_widget_ref (editorselect);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "editorselect", editorselect,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (editorselect);
  gtk_table_attach (GTK_TABLE (table16), editorselect, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_combo_set_value_in_list (GTK_COMBO (editorselect), TRUE, TRUE);

  combo_editor = GTK_COMBO (editorselect)->entry;
  gtk_widget_ref (combo_editor);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "combo_editor", combo_editor,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo_editor);
  gtk_entry_set_editable (GTK_ENTRY (combo_editor), FALSE);

  radiocusteditor = gtk_radio_button_new_with_label (seleditor_group, _("Custom Editor"));
  seleditor_group = gtk_radio_button_group (GTK_RADIO_BUTTON (radiocusteditor));
  gtk_widget_ref (radiocusteditor);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "radiocusteditor", radiocusteditor,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (radiocusteditor);
  gtk_table_attach (GTK_TABLE (table16), radiocusteditor, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, radiocusteditor, _("With this option you can create your own default editor"), NULL);

  table17 = gtk_table_new (3, 1, FALSE);
  gtk_widget_ref (table17);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "table17", table17,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table17);
  gtk_table_attach (GTK_TABLE (table16), table17, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  editorterminal = gtk_check_button_new_with_label (_("Start in Terminal"));
  gtk_widget_ref (editorterminal);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "editorterminal", editorterminal,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (editorterminal);
  gtk_table_attach (GTK_TABLE (table17), editorterminal, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, editorterminal, _("Does this editor need to start in an xterm?"), NULL);

  editorlineno = gtk_check_button_new_with_label (_("Accepts Line Number"));
  gtk_widget_ref (editorlineno);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "editorlineno", editorlineno,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (editorlineno);
  gtk_table_attach (GTK_TABLE (table17), editorlineno, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, editorlineno, _("Does this editor accept line numbers from the command line?"), NULL);

  table18 = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (table18);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "table18", table18,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table18);
  gtk_table_attach (GTK_TABLE (table17), table18, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

  label7 = gtk_label_new (_("Command:"));
  gtk_widget_ref (label7);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "label7", label7,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label7);
  gtk_table_attach (GTK_TABLE (table18), label7, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label7), 0, 0.5);

  editorcommand = gtk_entry_new ();
  gtk_widget_ref (editorcommand);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "editorcommand", editorcommand,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (editorcommand);
  gtk_table_attach (GTK_TABLE (table18), editorcommand, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 5, 0);
  gtk_tooltips_set_tip (tooltips, editorcommand, _("Please enter the command line used to start this editor"), NULL);

  hseparator5 = gtk_hseparator_new ();
  gtk_widget_ref (hseparator5);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "hseparator5", hseparator5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hseparator5);
  gtk_table_attach (GTK_TABLE (table16), hseparator5, 0, 2, 1, 2,
                    (GtkAttachOptions) (GTK_SHRINK | GTK_FILL),
                    (GtkAttachOptions) (GTK_SHRINK | GTK_FILL), 0, 0);

  label4 = gtk_label_new (_("Text Editor"));
  gtk_widget_ref (label4);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "label4", label4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label4);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 0), label4);

  frame2 = gtk_frame_new (_("Gnome Default Web Browser"));
  gtk_widget_ref (frame2);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "frame2", frame2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame2);
  gtk_container_add (GTK_CONTAINER (notebook1), frame2);
  gtk_container_set_border_width (GTK_CONTAINER (frame2), 5);

  table19 = gtk_table_new (3, 2, FALSE);
  gtk_widget_ref (table19);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "table19", table19,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table19);
  gtk_container_add (GTK_CONTAINER (frame2), table19);
  gtk_table_set_row_spacings (GTK_TABLE (table19), 10);
  gtk_table_set_col_spacings (GTK_TABLE (table19), 10);

  seldefbrowser = gtk_radio_button_new_with_label (selbrowser_group, _("Select a Web Browser"));
  selbrowser_group = gtk_radio_button_group (GTK_RADIO_BUTTON (seldefbrowser));
  gtk_widget_ref (seldefbrowser);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "seldefbrowser", seldefbrowser,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (seldefbrowser);
  gtk_table_attach (GTK_TABLE (table19), seldefbrowser, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, seldefbrowser, _("With this option, you can select a predefined Web Browser as your default"), NULL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (seldefbrowser), TRUE);

  browserselect = gtk_combo_new ();
  gtk_widget_ref (browserselect);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "browserselect", browserselect,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (browserselect);
  gtk_table_attach (GTK_TABLE (table19), browserselect, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_combo_set_value_in_list (GTK_COMBO (browserselect), TRUE, TRUE);

  combo_browser = GTK_COMBO (browserselect)->entry;
  gtk_widget_ref (combo_browser);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "combo_browser", combo_browser,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo_browser);
  gtk_entry_set_editable (GTK_ENTRY (combo_browser), FALSE);

  selcustbrowser = gtk_radio_button_new_with_label (selbrowser_group, _("Custom Web Browser"));
  selbrowser_group = gtk_radio_button_group (GTK_RADIO_BUTTON (selcustbrowser));
  gtk_widget_ref (selcustbrowser);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "selcustbrowser", selcustbrowser,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (selcustbrowser);
  gtk_table_attach (GTK_TABLE (table19), selcustbrowser, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, selcustbrowser, _("With this option you can create your own default web browser"), NULL);

  table20 = gtk_table_new (3, 1, FALSE);
  gtk_widget_ref (table20);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "table20", table20,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table20);
  gtk_table_attach (GTK_TABLE (table19), table20, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  browserterminal = gtk_check_button_new_with_label (_("Start in Terminal"));
  gtk_widget_ref (browserterminal);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "browserterminal", browserterminal,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (browserterminal);
  gtk_table_attach (GTK_TABLE (table20), browserterminal, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, browserterminal, _("Does this web browser need to display in an xterm?"), NULL);

  browserremote = gtk_check_button_new_with_label (_("Understands Netscape Remote Control"));
  gtk_widget_ref (browserremote);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "browserremote", browserremote,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (browserremote);
  gtk_table_attach (GTK_TABLE (table20), browserremote, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, browserremote, _("Does this web browser support the netscape remote control protocol?  If in doubt, and this web browser isn't Netscape or Mozilla, it probably doesn't."), NULL);

  table21 = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (table21);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "table21", table21,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table21);
  gtk_table_attach (GTK_TABLE (table20), table21, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

  label8 = gtk_label_new (_("Command:"));
  gtk_widget_ref (label8);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "label8", label8,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label8);
  gtk_table_attach (GTK_TABLE (table21), label8, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label8), 0, 0.5);

  browsercommand = gtk_entry_new ();
  gtk_widget_ref (browsercommand);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "browsercommand", browsercommand,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (browsercommand);
  gtk_table_attach (GTK_TABLE (table21), browsercommand, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 5, 0);
  gtk_tooltips_set_tip (tooltips, browsercommand, _("Please enter the command line used to start this web browser"), NULL);

  hseparator6 = gtk_hseparator_new ();
  gtk_widget_ref (hseparator6);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "hseparator6", hseparator6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hseparator6);
  gtk_table_attach (GTK_TABLE (table19), hseparator6, 0, 2, 1, 2,
                    (GtkAttachOptions) (GTK_SHRINK | GTK_FILL),
                    (GtkAttachOptions) (GTK_SHRINK | GTK_FILL), 0, 0);

  label5 = gtk_label_new (_("Web Browser"));
  gtk_widget_ref (label5);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "label5", label5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label5);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 1), label5);

  frame3 = gtk_frame_new (_("Default Help Viewer"));
  gtk_widget_ref (frame3);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "frame3", frame3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame3);
  gtk_container_add (GTK_CONTAINER (notebook1), frame3);
  gtk_container_set_border_width (GTK_CONTAINER (frame3), 5);

  table22 = gtk_table_new (3, 2, FALSE);
  gtk_widget_ref (table22);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "table22", table22,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table22);
  gtk_container_add (GTK_CONTAINER (frame3), table22);
  gtk_table_set_row_spacings (GTK_TABLE (table22), 10);
  gtk_table_set_col_spacings (GTK_TABLE (table22), 10);

  seldefview = gtk_radio_button_new_with_label (selhelp_group, _("Select a Viewer"));
  selhelp_group = gtk_radio_button_group (GTK_RADIO_BUTTON (seldefview));
  gtk_widget_ref (seldefview);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "seldefview", seldefview,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (seldefview);
  gtk_table_attach (GTK_TABLE (table22), seldefview, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, seldefview, _("With this option you can select a predefined help viewer."), NULL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (seldefview), TRUE);

  helpselect = gtk_combo_new ();
  gtk_widget_ref (helpselect);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "helpselect", helpselect,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (helpselect);
  gtk_table_attach (GTK_TABLE (table22), helpselect, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_combo_set_value_in_list (GTK_COMBO (helpselect), TRUE, TRUE);

  combo_help = GTK_COMBO (helpselect)->entry;
  gtk_widget_ref (combo_help);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "combo_help", combo_help,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo_help);
  gtk_entry_set_editable (GTK_ENTRY (combo_help), FALSE);

  selcustview = gtk_radio_button_new_with_label (selhelp_group, _("Custom Help Viewer"));
  selhelp_group = gtk_radio_button_group (GTK_RADIO_BUTTON (selcustview));
  gtk_widget_ref (selcustview);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "selcustview", selcustview,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (selcustview);
  gtk_table_attach (GTK_TABLE (table22), selcustview, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, selcustview, _("With this option you can create your own help viewer"), NULL);

  table23 = gtk_table_new (3, 1, FALSE);
  gtk_widget_ref (table23);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "table23", table23,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table23);
  gtk_table_attach (GTK_TABLE (table22), table23, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  helpterminal = gtk_check_button_new_with_label (_("Start in Terminal"));
  gtk_widget_ref (helpterminal);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "helpterminal", helpterminal,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (helpterminal);
  gtk_table_attach (GTK_TABLE (table23), helpterminal, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, helpterminal, _("Does this help viewer need an xterm for display?"), NULL);

  helpurls = gtk_check_button_new_with_label (_("Accepts URLs"));
  gtk_widget_ref (helpurls);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "helpurls", helpurls,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (helpurls);
  gtk_table_attach (GTK_TABLE (table23), helpurls, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, helpurls, _("Does this help viewer allow URLs for help?"), NULL);

  table24 = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (table24);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "table24", table24,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table24);
  gtk_table_attach (GTK_TABLE (table23), table24, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

  label9 = gtk_label_new (_("Command:"));
  gtk_widget_ref (label9);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "label9", label9,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label9);
  gtk_table_attach (GTK_TABLE (table24), label9, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label9), 0, 0.5);

  helpcommand = gtk_entry_new ();
  gtk_widget_ref (helpcommand);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "helpcommand", helpcommand,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (helpcommand);
  gtk_table_attach (GTK_TABLE (table24), helpcommand, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 5, 0);
  gtk_tooltips_set_tip (tooltips, helpcommand, _("Please enter the command line used to start this help viewer"), NULL);

  hseparator7 = gtk_hseparator_new ();
  gtk_widget_ref (hseparator7);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "hseparator7", hseparator7,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hseparator7);
  gtk_table_attach (GTK_TABLE (table22), hseparator7, 0, 2, 1, 2,
                    (GtkAttachOptions) (GTK_SHRINK | GTK_FILL),
                    (GtkAttachOptions) (GTK_SHRINK | GTK_FILL), 0, 0);

  label6 = gtk_label_new (_("Help Viewer"));
  gtk_widget_ref (label6);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "label6", label6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label6);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 2), label6);

  frame4 = gtk_frame_new (_("Default Terminal"));
  gtk_widget_ref (frame4);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "frame4", frame4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame4);
  gtk_container_add (GTK_CONTAINER (notebook1), frame4);
  gtk_container_set_border_width (GTK_CONTAINER (frame4), 5);

  table25 = gtk_table_new (3, 2, FALSE);
  gtk_widget_ref (table25);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "table25", table25,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table25);
  gtk_container_add (GTK_CONTAINER (frame4), table25);
  gtk_table_set_row_spacings (GTK_TABLE (table25), 10);
  gtk_table_set_col_spacings (GTK_TABLE (table25), 10);

  seldefterm = gtk_radio_button_new_with_label (selterm_group, _("Select a Terminal"));
  selterm_group = gtk_radio_button_group (GTK_RADIO_BUTTON (seldefterm));
  gtk_widget_ref (seldefterm);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "seldefterm", seldefterm,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (seldefterm);
  gtk_table_attach (GTK_TABLE (table25), seldefterm, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, seldefterm, _("With this option you can select a predefined terminal."), NULL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (seldefterm), TRUE);

  termselect = gtk_combo_new ();
  gtk_widget_ref (termselect);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "termselect", termselect,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (termselect);
  gtk_table_attach (GTK_TABLE (table25), termselect, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_combo_set_value_in_list (GTK_COMBO (termselect), TRUE, TRUE);

  combo_term = GTK_COMBO (termselect)->entry;
  gtk_widget_ref (combo_term);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "combo_term", combo_term,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo_term);
  gtk_entry_set_editable (GTK_ENTRY (combo_term), FALSE);

  selcustterm = gtk_radio_button_new_with_label (selterm_group, _("Custom Terminal"));
  selterm_group = gtk_radio_button_group (GTK_RADIO_BUTTON (selcustterm));
  gtk_widget_ref (selcustterm);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "selcustterm", selcustterm,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (selcustterm);
  gtk_table_attach (GTK_TABLE (table25), selcustterm, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, selcustterm, _("With this option you can create your own terminal"), NULL);

  table26 = gtk_table_new (1, 1, FALSE);
  gtk_widget_ref (table26);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "table26", table26,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table26);
  gtk_table_attach (GTK_TABLE (table25), table26, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  table27 = gtk_table_new (2, 2, FALSE);
  gtk_widget_ref (table27);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "table27", table27,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table27);
  gtk_table_attach (GTK_TABLE (table26), table27, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

  label11 = gtk_label_new (_("Command:"));
  gtk_widget_ref (label11);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "label11", label11,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label11);
  gtk_table_attach (GTK_TABLE (table27), label11, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label11), 0, 0.5);

  label12 = gtk_label_new (_("Exec Flag:"));
  gtk_widget_ref (label12);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "label12", label12,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label12);
  gtk_table_attach (GTK_TABLE (table27), label12, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label12), 0, 0.5);

  termexec = gtk_entry_new_with_max_length (20);
  gtk_widget_ref (termexec);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "termexec", termexec,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (termexec);
  gtk_table_attach (GTK_TABLE (table27), termexec, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, termexec, _("Please enter the flag used by this terminal to specify the command to run on startup.  For example, in 'xterm' this would be '-e'."), NULL);

  termcommand = gtk_entry_new ();
  gtk_widget_ref (termcommand);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "termcommand", termcommand,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (termcommand);
  gtk_table_attach (GTK_TABLE (table27), termcommand, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (0), 5, 0);
  gtk_tooltips_set_tip (tooltips, termcommand, _("Please enter the command line used to start this terminal"), NULL);

  hseparator8 = gtk_hseparator_new ();
  gtk_widget_ref (hseparator8);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "hseparator8", hseparator8,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hseparator8);
  gtk_table_attach (GTK_TABLE (table25), hseparator8, 0, 2, 1, 2,
                    (GtkAttachOptions) (GTK_SHRINK | GTK_FILL),
                    (GtkAttachOptions) (GTK_SHRINK | GTK_FILL), 0, 0);

  label10 = gtk_label_new (_("Terminal"));
  gtk_widget_ref (label10);
  gtk_object_set_data_full (GTK_OBJECT (capplet), "label10", label10,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label10);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 3), label10);

  gtk_signal_connect (GTK_OBJECT (radiodefeditor), "toggled",
                      GTK_SIGNAL_FUNC (on_radiodefeditor_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (combo_editor), "changed",
                      GTK_SIGNAL_FUNC (on_combo_editor_changed),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (radiocusteditor), "toggled",
                      GTK_SIGNAL_FUNC (on_radiocusteditor_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (editorterminal), "toggled",
                      GTK_SIGNAL_FUNC (on_editorterminal_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (editorlineno), "toggled",
                      GTK_SIGNAL_FUNC (on_editorlineno_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (editorcommand), "changed",
                      GTK_SIGNAL_FUNC (on_editorcommand_changed),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (seldefbrowser), "toggled",
                      GTK_SIGNAL_FUNC (on_seldefbrowser_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (combo_browser), "changed",
                      GTK_SIGNAL_FUNC (on_combo_browser_changed),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (selcustbrowser), "toggled",
                      GTK_SIGNAL_FUNC (on_selcustbrowser_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (browserterminal), "toggled",
                      GTK_SIGNAL_FUNC (on_browserterminal_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (browserremote), "toggled",
                      GTK_SIGNAL_FUNC (on_browserremote_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (browsercommand), "changed",
                      GTK_SIGNAL_FUNC (on_browsercommand_changed),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (seldefview), "toggled",
                      GTK_SIGNAL_FUNC (on_seldefview_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (combo_help), "changed",
                      GTK_SIGNAL_FUNC (on_combo_help_changed),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (selcustview), "toggled",
                      GTK_SIGNAL_FUNC (on_selcustview_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (helpterminal), "toggled",
                      GTK_SIGNAL_FUNC (on_helpterminal_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (helpurls), "toggled",
                      GTK_SIGNAL_FUNC (on_helpurls_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (helpcommand), "changed",
                      GTK_SIGNAL_FUNC (on_helpcommand_changed),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (seldefterm), "toggled",
                      GTK_SIGNAL_FUNC (on_seldefterm_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (combo_term), "changed",
                      GTK_SIGNAL_FUNC (on_combo_term_changed),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (selcustterm), "toggled",
                      GTK_SIGNAL_FUNC (on_selcustterm_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (termexec), "changed",
                      GTK_SIGNAL_FUNC (on_termexec_changed),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (termcommand), "changed",
                      GTK_SIGNAL_FUNC (on_termcommand_changed),
                      NULL);

  gtk_object_set_data (GTK_OBJECT (capplet), "tooltips", tooltips);

  /* Turn off some sections for initialization. */
  gtk_widget_set_sensitive(table17, FALSE);
  gtk_widget_set_sensitive(table20, FALSE);
  gtk_widget_set_sensitive(table23, FALSE);
  gtk_widget_set_sensitive(table26, FALSE);
  

  gtk_widget_show_all (capplet);

}
