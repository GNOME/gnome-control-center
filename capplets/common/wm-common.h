#ifndef WM_COMMON_H
#define WM_COMMON_H

#define WM_COMMON_METACITY "Metacity"
#define WM_COMMON_SAWFISH  "Sawfish"
#define WM_COMMON_UNKNOWN  "Unknown"

gchar *wm_common_get_current_window_manager (void);
void   wm_common_register_window_manager_change (GFunc    func,
						 gpointer data);

#endif /* WM_COMMON_H */

