/* ACME
 * Copyright (C) 2001 Bastien Nocera <hadess@hadess.net>
 *
 * acme.h
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 * USA.
 */

#ifndef __ACME_H__
#define __ACME_H__

#define GCONF_BINDING_DIR "/apps/gnome_settings_daemon/keybindings"
#define GCONF_MISC_DIR "/apps/gnome_settings_daemon"

enum {
	MUTE_KEY,
	VOLUME_DOWN_KEY,
	VOLUME_UP_KEY,
	POWER_KEY,
	EJECT_KEY,
	HOME_KEY,
	SEARCH_KEY,
	EMAIL_KEY,
	SLEEP_KEY,
	SCREENSAVER_KEY,
	HELP_KEY,
	WWW_KEY,
#ifdef USE_FBLEVEL
	BRIGHT_DOWN_KEY,
	BRIGHT_UP_KEY,
#endif
	PLAY_KEY,
	PAUSE_KEY,
	STOP_KEY,
	PREVIOUS_KEY,
	NEXT_KEY,
	HANDLED_KEYS,
};

typedef struct {
  guint keysym;
  guint state;
  guint keycode;
} Key;

static struct {
	int key_type;
	const char *gconf_key;
	Key *key;
} keys[HANDLED_KEYS] = {
	{ MUTE_KEY, GCONF_BINDING_DIR "/volume_mute",NULL },
	{ VOLUME_DOWN_KEY, GCONF_BINDING_DIR "/volume_down", NULL },
	{ VOLUME_UP_KEY, GCONF_BINDING_DIR "/volume_up", NULL },
	{ POWER_KEY, GCONF_BINDING_DIR "/power", NULL },
	{ EJECT_KEY, GCONF_BINDING_DIR "/eject", NULL },
	{ HOME_KEY, GCONF_BINDING_DIR "/home", NULL },
	{ SEARCH_KEY, GCONF_BINDING_DIR "/search", NULL },
	{ EMAIL_KEY, GCONF_BINDING_DIR "/email", NULL },
	{ SLEEP_KEY, GCONF_BINDING_DIR "/sleep", NULL },
	{ SCREENSAVER_KEY, GCONF_BINDING_DIR "/screensaver", NULL },
	{ HELP_KEY, GCONF_BINDING_DIR "/help", NULL },
	{ WWW_KEY, GCONF_BINDING_DIR "/www_key_str", NULL },
#ifdef USE_FBLEVEL
	{ BRIGHT_DOWN_KEY, GCONF_BINDING_DIR "/brightness_down", NULL },
	{ BRIGHT_UP_KEY, GCONF_BINDING_DIR "/brightness_up", NULL },
#endif
	{ PLAY_KEY, GCONF_BINDING_DIR "/play", NULL },
	{ PAUSE_KEY, GCONF_BINDING_DIR "/pause", NULL },
	{ STOP_KEY, GCONF_BINDING_DIR "/stop", NULL },
	{ PREVIOUS_KEY, GCONF_BINDING_DIR "/previous", NULL },
	{ NEXT_KEY, GCONF_BINDING_DIR "/next", NULL },
};

#endif /* __ACME_H__ */

