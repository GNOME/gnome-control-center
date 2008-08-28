/* -*- mode: c; style: linux -*- */
/* -*- c-basic-offset: 2 -*- */

/* sound-theme-definition.h
 * Copyright (C) 2008 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

typedef enum {
	CATEGORY_INVALID,
	CATEGORY_BELL,
	CATEGORY_WINDOWS_BUTTONS,
	CATEGORY_DESKTOP,
	CATEGORY_ALERTS,
	NUM_CATEGORIES
} CategoryType;

typedef enum {
	SOUND_TYPE_NORMAL,
	SOUND_TYPE_AUDIO_BELL,
	SOUND_TYPE_VISUAL_BELL,
	SOUND_TYPE_FEEDBACK
} SoundType;

static struct {
	CategoryType category;
	SoundType type;
	const char *display_name;
	const char *names[6];
} sounds[20] = {
	/* Bell */
	{ CATEGORY_BELL, SOUND_TYPE_AUDIO_BELL, NC_("Sound event", "Alert sound"), { "bell-terminal", "bell-window-system", NULL } },
	{ CATEGORY_BELL, SOUND_TYPE_VISUAL_BELL, NC_("Sound event", "Visual alert"), { NULL } },
	/* Windows and buttons */
	{ CATEGORY_WINDOWS_BUTTONS, -1, NC_("Sound event", "Windows and Buttons"), { NULL } },
	{ CATEGORY_WINDOWS_BUTTONS, SOUND_TYPE_FEEDBACK, NC_("Sound event", "Button clicked"), { "button-pressed", "menu-click", "menu-popup", "menu-popdown", "menu-replace", NULL } },
	{ CATEGORY_WINDOWS_BUTTONS, SOUND_TYPE_FEEDBACK, NC_("Sound event", "Toggle button clicked"), { "button-toggle-off", "button-toggle-on", NULL } },
	{ CATEGORY_WINDOWS_BUTTONS, SOUND_TYPE_FEEDBACK, NC_("Sound event", "Window maximized"), { "window-maximized", NULL } },
	{ CATEGORY_WINDOWS_BUTTONS, SOUND_TYPE_FEEDBACK, NC_("Sound event", "Window unmaximized"), { "window-unmaximized", NULL } },
	{ CATEGORY_WINDOWS_BUTTONS, SOUND_TYPE_FEEDBACK, NC_("Sound event", "Window minimised"), { "window-minimized", NULL } },
	/* Desktop */
	{ CATEGORY_DESKTOP, -1, NC_("Sound event", "Desktop"), { NULL } },
	{ CATEGORY_DESKTOP, SOUND_TYPE_NORMAL, NC_("Sound event", "Login"), { "desktop-login", NULL } },
	{ CATEGORY_DESKTOP, SOUND_TYPE_NORMAL, NC_("Sound event", "Logout"), { "desktop-logout", NULL } },
	{ CATEGORY_DESKTOP, SOUND_TYPE_NORMAL, NC_("Sound event", "New e-mail"), { "message-new-email", NULL } },
	{ CATEGORY_DESKTOP, SOUND_TYPE_NORMAL, NC_("Sound event", "Empty trash"), { "trash-empty", NULL } },
	{ CATEGORY_DESKTOP, SOUND_TYPE_NORMAL, NC_("Sound event", "Long action completed (download, CD burning, etc.)"), { "complete-copy", "complete-download", "complete-media-burn", "complete-media-rip", "complete-scan", NULL } },
	/* Alerts? */
	{ CATEGORY_ALERTS, -1, NC_("Sound event", "Alerts"), { NULL } },
	{ CATEGORY_ALERTS, SOUND_TYPE_NORMAL, NC_("Sound event", "Information or question"), { "dialog-information", "dialog-question", NULL } },
	{ CATEGORY_ALERTS, SOUND_TYPE_NORMAL, NC_("Sound event", "Warning"), { "dialog-warning", NULL } },
	{ CATEGORY_ALERTS, SOUND_TYPE_NORMAL, NC_("Sound event", "Error"), { "dialog-error", NULL } },
	{ CATEGORY_ALERTS, SOUND_TYPE_NORMAL, NC_("Sound event", "Battery warning"), { "power-unplug-battery-low", "battery-low", "battery-caution", NULL } },
	/* Finish off */
	{ -1, -1, NULL, { NULL } }
};

