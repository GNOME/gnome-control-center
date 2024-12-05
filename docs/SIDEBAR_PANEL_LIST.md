# Reordering the sidebar panel list

Since 46.alpha the order of panels in the sidebar as well as the position of the separators is defined in this array https://gitlab.gnome.org/GNOME/gnome-control-center/-/blob/main/shell/cc-panel-list.c#L350

* Use the string "separator" to introduce a separator between two rows
* A "separator" can't be the last row
