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

#include <mate-panel-applet.h>
#include <playerctl/playerctl.h>
#include <string.h>

#include "../config.h"
#include "applet.h"

/*
 * Mpris2: playerctl backend
 */
static void
multimedia_control_applet_update (MultimediaControlApplet *applet)
{
	gboolean active = FALSE;
	PlayerctlPlaybackStatus status = PLAYERCTL_PLAYBACK_STATUS_STOPPED;
	const gchar *icon_name = "media-playback-start";
	gchar *name = NULL;

	if (applet->player != NULL) {
		g_object_get (applet->player,
		              "playback-status", &status,
		              "player-name", &name,
		              NULL);

		if (status != PLAYERCTL_PLAYBACK_STATUS_STOPPED)
			active = TRUE;

		if (status == PLAYERCTL_PLAYBACK_STATUS_PLAYING)
			icon_name = "media-playback-pause";
	}

	gtk_widget_set_sensitive (applet->prev_button, active);
	gtk_button_set_image (GTK_BUTTON(applet->play_button),
	                      gtk_image_new_from_icon_name (icon_name,
	                                                    GTK_ICON_SIZE_BUTTON));
	gtk_widget_set_sensitive (applet->stop_button, active);
	gtk_widget_set_sensitive (applet->next_button, active);

	gtk_widget_set_tooltip_text (GTK_WIDGET(applet->applet),
	                             (name != NULL) ? name : "Click to detect the player");

	g_free (name);
}

static void
player_playback_status (PlayerctlPlayer         *player,
                        PlayerctlPlaybackStatus  playback_status,
                        MultimediaControlApplet *applet)
{
	multimedia_control_applet_update (applet);
}

static void
multimedia_control_applet_bind_player (MultimediaControlApplet *applet,
                                       PlayerctlPlayerName     *player_name)
{
	PlayerctlPlayer *player;
	GError *error = NULL;

	if (applet->player != NULL)
		return;

	player = playerctl_player_new_from_name (player_name, &error);
	if (player == NULL) {
		g_warning ("Could not connect to player '%s': %s",
		           player_name->name != NULL ? player_name->name : "?",
		           error->message);
		g_error_free (error);
		return;
	}

	playerctl_player_manager_manage_player (applet->manager, player);

	g_signal_connect (G_OBJECT (player), "playback-status",
	                  G_CALLBACK (player_playback_status), applet);

	applet->player = player;

	multimedia_control_applet_update (applet);
}

static void
multimedia_control_applet_unbind_player (MultimediaControlApplet *applet)
{
	if (applet->player == NULL)
		return;

	g_signal_handlers_disconnect_by_func (G_OBJECT (applet->player),
	                                      G_CALLBACK (player_playback_status),
	                                      applet);

	g_object_unref (applet->player);
	applet->player = NULL;
}

static void
multimedia_control_applet_bind_first_player (MultimediaControlApplet *applet)
{
	GList *names, *l;

	if (applet->player != NULL)
		return;

	g_object_get (applet->manager, "player-names", &names, NULL);

	for (l = names; l != NULL; l = l->next) {
		multimedia_control_applet_bind_player (applet, l->data);
		if (applet->player != NULL)
			break;
	}
}

static void
manager_name_appeared (PlayerctlPlayerManager  *manager,
                       PlayerctlPlayerName     *player_name,
                       MultimediaControlApplet *applet)
{
	if (applet->player == NULL)
		multimedia_control_applet_bind_player (applet, player_name);
}

static void
manager_player_vanished (PlayerctlPlayerManager  *manager,
                         PlayerctlPlayer         *player,
                         MultimediaControlApplet *applet)
{
	if (player != applet->player)
		return;

	multimedia_control_applet_unbind_player (applet);

	multimedia_control_applet_update (applet);
	multimedia_control_applet_bind_first_player (applet);
}

static void
player_button_prev (GtkWidget               *widget,
                    MultimediaControlApplet *applet)
{
	if (applet->player == NULL)
		return;

	playerctl_player_previous (applet->player, NULL);
}

static void
player_button_play_pause (GtkWidget               *widget,
                          MultimediaControlApplet *applet)
{
	if (applet->player == NULL)
		multimedia_control_applet_bind_first_player (applet);

	if (applet->player == NULL)
		return;

	playerctl_player_play_pause (applet->player, NULL);
}

static void
player_button_stop (GtkWidget               *widget,
                    MultimediaControlApplet *applet)
{
	if (applet->player == NULL)
		return;

	playerctl_player_stop (applet->player, NULL);
}

static void
player_button_next (GtkWidget               *widget,
                    MultimediaControlApplet *applet)
{
	if (applet->player == NULL)
		return;

	playerctl_player_next (applet->player, NULL);
}

/*
 * Applet construction and signals.
 */
static void
applet_destroy (MatePanelApplet         *applet_widget,
                MultimediaControlApplet *applet)
{
	g_assert(applet != NULL);

	multimedia_control_applet_unbind_player (applet);

	if (applet->manager != NULL) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (applet->manager),
		                                      G_CALLBACK (manager_name_appeared),
		                                      applet);
		g_signal_handlers_disconnect_by_func (G_OBJECT (applet->manager),
		                                      G_CALLBACK (manager_player_vanished),
		                                      applet);

		g_object_unref (applet->manager);
	}

	g_free(applet);

	return;
}

static gboolean
multimedia_control_applet_factory (MatePanelApplet *applet_widget,
                                   const gchar     *iid,
                                   gpointer         data)
{
	MultimediaControlApplet *applet;
	GtkWidget *box, *button;
	GError *error = NULL;

	if (strcmp (iid, APPLET_ID) != 0)
		return FALSE;

	applet = g_malloc0(sizeof(MultimediaControlApplet));

	applet->applet = applet_widget;

	/*
	 * Mpris2
	 */
	applet->manager = playerctl_player_manager_new (&error);
	if (applet->manager == NULL) {
		g_warning ("Could not create the player manager: %s", error->message);
		g_error_free (error);
		g_free (applet);
		return FALSE;
	}

	g_signal_connect (G_OBJECT (applet->manager), "name-appeared",
	                  G_CALLBACK(manager_name_appeared), applet);
	g_signal_connect (G_OBJECT (applet->manager), "player-vanished",
	                  G_CALLBACK(manager_player_vanished), applet);

	/*
	 * Widgets
	 */
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	button = GTK_WIDGET(gtk_button_new_from_icon_name ("media-skip-backward",
	                                                   GTK_ICON_SIZE_BUTTON));
	gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
	g_signal_connect (G_OBJECT(button), "clicked",
	                  G_CALLBACK(player_button_prev), applet);
	gtk_box_pack_start (GTK_BOX(box), GTK_WIDGET(button),
	                    FALSE, FALSE, 0);
	applet->prev_button = button;

	button = GTK_WIDGET(gtk_button_new_from_icon_name ("media-playback-start",
	                                                   GTK_ICON_SIZE_BUTTON));
	gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
	g_signal_connect (G_OBJECT(button), "clicked",
	                  G_CALLBACK(player_button_play_pause), applet);
	gtk_box_pack_start (GTK_BOX(box), GTK_WIDGET(button),
	                    FALSE, FALSE, 0);
	applet->play_button = button;

	button = GTK_WIDGET(gtk_button_new_from_icon_name ("media-playback-stop",
	                                                   GTK_ICON_SIZE_BUTTON));
	gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
	g_signal_connect (G_OBJECT(button), "clicked",
	                  G_CALLBACK(player_button_stop), applet);
	gtk_box_pack_start (GTK_BOX(box), GTK_WIDGET(button),
	                    FALSE, FALSE, 0);
	applet->stop_button = button;

	button = GTK_WIDGET(gtk_button_new_from_icon_name ("media-skip-forward",
	                                                   GTK_ICON_SIZE_BUTTON));
	gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
	g_signal_connect (G_OBJECT(button), "clicked",
	                  G_CALLBACK(player_button_next), applet);
	gtk_box_pack_start (GTK_BOX(box), GTK_WIDGET(button),
	                    FALSE, FALSE, 0);
	applet->next_button = button;

	g_signal_connect (G_OBJECT(applet_widget), "destroy",
	                  G_CALLBACK(applet_destroy), (gpointer)applet);

	gtk_container_add(GTK_CONTAINER(applet_widget), box);

	gtk_widget_show_all(GTK_WIDGET(applet_widget));

	/*
	 * Check if a player is running and update.
	 */
	multimedia_control_applet_bind_first_player (applet);

	multimedia_control_applet_update (applet);

	return TRUE;
}

MATE_PANEL_APPLET_OUT_PROCESS_FACTORY (APPLET_FACTORY, PANEL_TYPE_APPLET, APPLET_NAME, multimedia_control_applet_factory, NULL)
