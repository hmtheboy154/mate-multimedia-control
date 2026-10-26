/*************************************************************************/
/* Copyright (C) 2014 matias <mati86dl@gmail.com>                        */
/* Copyright (C) 2026 hmtheboy154 <buingoc67@gmail.com>                  */
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

#include "settings.h"

#include <glib/gi18n.h>

#include "../config.h"
#include "applet.h"

/*
 * Preferences dialog.
 *
 * The three switches are bound to the GSettings keys and apply
 * immediately; the dialog only needs a close button.
 */

static void
preferences_response (GtkWidget               *dialog,
                      gint                     response_id,
                      MultimediaControlApplet *applet)
{
	gtk_widget_destroy (dialog);
	applet->preferences_dialog = NULL;
}

static void
preferences_destroyed (GtkWidget               *widget,
                       MultimediaControlApplet *applet)
{
	applet->preferences_dialog = NULL;
}

static GtkWidget *
preferences_check_button (const gchar *label, GSettings *settings,
                          const gchar *key)
{
	GtkWidget *button;

	button = gtk_check_button_new_with_mnemonic (label);
	g_settings_bind (settings, key, G_OBJECT (button), "active",
	                 G_SETTINGS_BIND_DEFAULT);

	return button;
}

void
multimedia_control_preferences_show (MultimediaControlApplet *applet)
{
	GtkWidget *dialog, *content, *button;

	if (applet->preferences_dialog != NULL) {
		gtk_window_present (GTK_WINDOW (applet->preferences_dialog));
		return;
	}

	dialog = gtk_dialog_new_with_buttons (_("Multimedia Controls Preferences"),
	                                      NULL,
	                                      GTK_DIALOG_DESTROY_WITH_PARENT,
	                                      _("_Close"), GTK_RESPONSE_CLOSE,
	                                      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "multimedia-player");
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	g_signal_connect (G_OBJECT (dialog), "response",
	                  G_CALLBACK (preferences_response), applet);
	g_signal_connect (G_OBJECT (dialog), "destroy",
	                  G_CALLBACK (preferences_destroyed), applet);

	content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	button = preferences_check_button (_("Show what is playing on other players"),
	                                   applet->settings,
	                                   MC_SETTINGS_KEY_OTHER_PLAYERS);
	gtk_box_pack_start (GTK_BOX (content), button, FALSE, FALSE, 6);

	button = preferences_check_button (_("Show album art for the current player"),
	                                   applet->settings,
	                                   MC_SETTINGS_KEY_ART_NOW_PLAYING);
	gtk_box_pack_start (GTK_BOX (content), button, FALSE, FALSE, 6);

	button = preferences_check_button (_("Show album art for the other players"),
	                                   applet->settings,
	                                   MC_SETTINGS_KEY_ART_OTHER_PLAYERS);
	gtk_box_pack_start (GTK_BOX (content), button, FALSE, FALSE, 6);

	gtk_widget_show_all (content);

	applet->preferences_dialog = dialog;
	gtk_widget_show (dialog);
}
