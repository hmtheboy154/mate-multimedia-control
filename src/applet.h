/*************************************************************************/
/* Copyright (C) 2014 matias <mati86dl@gmail.com>                        */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */
/*************************************************************************/

#ifndef __MULTIMEDIA_CONTROL_APPLET_H__
#define __MULTIMEDIA_CONTROL_APPLET_H__

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <mate-panel-applet.h>
#include <playerctl/playerctl.h>

#define APPLET_FACTORY "MultimediaControlAppletFactory"
#define APPLET_ID "MultimediaControlApplet"
#define APPLET_NAME "multimedia-control"

/*
 * Size of the album art icons in the popup menu.
 */
#define MC_ART_SIZE 24

/*
 * GSettings schema and keys.
 */
#define MC_SETTINGS_SCHEMA "org.mate.panel.applets.multimedia-control"
#define MC_SETTINGS_KEY_OTHER_PLAYERS "show-other-players"
#define MC_SETTINGS_KEY_ART_NOW_PLAYING "album-art-now-playing"
#define MC_SETTINGS_KEY_ART_OTHER_PLAYERS "album-art-other-players"

/*
 * One discovered player and the metadata we cached from its signals.
 * The struct is defined in main.c; the GTask machinery keeps it alive
 * while an artwork load is in flight.
 */
#define MC_TYPE_PLAYER_ENTRY (mc_player_entry_get_type ())
G_DECLARE_FINAL_TYPE (McPlayerEntry, mc_player_entry, MC, PLAYER_ENTRY, GObject)

typedef struct {
	MatePanelApplet         *applet;

	PlayerctlPlayerManager  *manager;
	GList                   *players;   /* list of McPlayerEntry */
	McPlayerEntry           *selected;  /* NULL when nothing is playing */

	GSettings               *settings;
	gboolean                 show_other_players;
	gboolean                 art_now_playing;
	gboolean                 art_other_players;

	GtkWidget               *prev_button;
	GtkWidget               *play_button;
	GtkWidget               *stop_button;
	GtkWidget               *next_button;

	/*
	 * Popup menu. The menu itself is owned by the lib's ui manager;
	 * we only keep the proxies of our actions and the submenu that
	 * holds the player list.
	 */
	GtkActionGroup          *action_group;
	GtkAction               *now_playing_action;
	GtkAction               *players_action;
	GtkWidget               *now_playing_item;   /* weak, may be NULL */
	GtkWidget               *players_menu;       /* owned by the proxy item */

	/*
	 * Settings and about dialogs, kept around to present instead of
	 * duplicating.
	 */
	GtkWidget               *preferences_dialog;
	GtkWidget               *about_dialog;
} MultimediaControlApplet;

#endif /* __MULTIMEDIA_CONTROL_APPLET_H__ */
