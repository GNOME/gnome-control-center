#ifndef __GNOME_THEME_DETAILS_H__
#define __GNOME_THEME_DETAILS_H__

void gnome_theme_details_init                    (void);
void gnome_theme_details_show                    (void);
void gnome_theme_details_reread_themes_from_disk (void);
void gnome_theme_details_update_from_gconf       (void);

#endif /* __GNOME_THEME_DETAILS_H__ */
