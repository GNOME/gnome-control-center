#include <gnome.h>


void
on_radiodefeditor_toggled              (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_combo_editor_changed                (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_radiocusteditor_toggled             (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_editorterminal_toggled              (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_editorlineno_toggled                (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_editorcommand_changed               (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_seldefbrowser_toggled               (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_combo_browser_changed               (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_selcustbrowser_toggled              (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_browserterminal_toggled             (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_browserremote_toggled               (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_browsercommand_changed              (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_seldefview_toggled                  (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_combo_help_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_selcustview_toggled                 (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_helpterminal_toggled                (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_helpurls_toggled                    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_helpcommand_changed                 (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_seldefterm_toggled                  (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_combo_term_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_selcustterm_toggled                 (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_termexec_changed                    (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_termcommand_changed                 (GtkEditable     *editable,
                                        gpointer         user_data);
