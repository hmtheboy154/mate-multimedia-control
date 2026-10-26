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

#include <glib/gi18n.h>
#include <string.h>

#include "../config.h"
#include "applet.h"
#include "art.h"
#include "settings.h"

/*
 * Multimedia control applet.
 *
 * All MPRIS2 players are discovered through a PlayerctlPlayerManager.
 * Each one is remembered with its current metadata so the popup menu
 * can show what is playing and allow switching the player to control.
 */

/*
 * Popup menu definition. The panel appends the standard items
 * (Remove From Panel, Move, Lock) below our entries.
 */
static const char applet_menu_xml[] =
	"<menuitem name='NowPlayingItem' action='NowPlaying'/>"
	"<separator/>"
	"<menu name='PlayersItem' action='Players'/>"
	"<separator/>"
	"<menuitem name='PreferencesItem' action='Preferences'/>"
	"<menuitem name='AboutItem' action='About'/>";

static void applet_update_buttons       (MultimediaControlApplet *applet);
static void applet_menu_refresh         (MultimediaControlApplet *applet);
static void applet_menu_players_rebuild (MultimediaControlApplet *applet);
static void applet_select_player        (MultimediaControlApplet *applet,
                                         McPlayerEntry           *entry);
static void multimedia_control_about_show (GtkAction               *action,
                                           MultimediaControlApplet *applet);

static void
preferences_activated (GtkAction               *action,
                       MultimediaControlApplet *applet)
{
	multimedia_control_preferences_show (applet);
}

static const GtkActionEntry applet_menu_actions[] = {
	{ "Preferences", NULL, N_("_Preferences"), NULL, NULL,
	  G_CALLBACK (preferences_activated) },
	{ "About", NULL, N_("_About"), NULL, NULL,
	  G_CALLBACK (multimedia_control_about_show) },
};

/*
 * Player entry.
 */
struct _McPlayerEntry {
	GObject parent_instance;

	PlayerctlPlayer         *player;
	MultimediaControlApplet *applet;
	gchar                   *name;
	gchar                   *instance;
	PlayerctlPlaybackStatus  status;
	gchar                   *title;
	gchar                   *artist;
	gchar                   *art_url;
	GdkPixbuf               *art;
	GCancellable            *art_cancel;
	gboolean                 art_failed;
};

G_DEFINE_TYPE (McPlayerEntry, mc_player_entry, G_TYPE_OBJECT);

static gchar *
mc_truncate_text (const gchar *text, gsize max_chars)
{
	gchar *part, *truncated;

	if (text == NULL)
		return g_strdup ("");

	if (g_utf8_strlen (text, -1) <= max_chars)
		return g_strdup (text);

	part = g_utf8_substring (text, 0, max_chars);
	truncated = g_strconcat (part, "…", NULL);
	g_free (part);

	return truncated;
}

/*
 * Text of the "Now Playing" entry: artist and title of the current
 * track, falling back to the player name.
 */
static gchar *
applet_now_playing_text (MultimediaControlApplet *applet)
{
	McPlayerEntry *entry = applet->selected;
	gchar *text, *truncated;

	if (entry == NULL)
		return g_strdup (_("no player detected"));

	if (entry->artist != NULL && entry->title != NULL)
		text = g_strdup_printf ("%s — %s", entry->artist, entry->title);
	else if (entry->title != NULL)
		text = g_strdup (entry->title);
	else
		text = g_strdup (entry->name);

	truncated = mc_truncate_text (text, 48);
	g_free (text);

	return truncated;
}

static void
art_loaded (GObject      *source_object,
            GAsyncResult *result,
            gpointer      user_data)
{
	McPlayerEntry *entry = MC_PLAYER_ENTRY (source_object);
	GdkPixbuf *pixbuf;
	GError *error = NULL;

	if (g_cancellable_is_cancelled (entry->art_cancel))
		return;

	pixbuf = multimedia_art_get_finish (result, &error);
	if (pixbuf == NULL) {
		if (error != NULL) {
			g_debug ("Could not load artwork: %s", error->message);
			g_error_free (error);
		}
		entry->art_failed = TRUE;
		return;
	}

	if (entry->art != NULL)
		g_object_unref (entry->art);
	entry->art = pixbuf;

	applet_menu_refresh (entry->applet);
	applet_menu_players_rebuild (entry->applet);
}

static void
entry_request_art (McPlayerEntry *entry)
{
	if (entry->art_url == NULL ||
	    entry->art != NULL ||
	    entry->art_failed ||
	    g_cancellable_is_cancelled (entry->art_cancel))
		return;

	multimedia_art_get_async (entry, entry->art_url, MC_ART_SIZE,
	                          art_loaded, NULL);
}

/*
 * Read the metadata of a player into the entry cache.
 */
static void
player_entry_refresh_metadata (McPlayerEntry *entry)
{
	GVariant *metadata;
	gchar *title, *artist, *art_url;
	GError *error = NULL;

	title = playerctl_player_get_title (entry->player, &error);
	if (error != NULL) {
		g_debug ("Could not read the track title: %s", error->message);
		g_error_free (error);
	}

	artist = playerctl_player_get_artist (entry->player, &error);
	if (error != NULL) {
		g_debug ("Could not read the track artist: %s", error->message);
		g_error_free (error);
	}

	art_url = NULL;
	g_object_get (entry->player, "metadata", &metadata, NULL);
	if (metadata != NULL) {
		g_variant_lookup (metadata, "mpris:artUrl", "s", &art_url);
		g_variant_unref (metadata);
	}

	if (g_strcmp0 (art_url, entry->art_url) != 0) {
		g_free (entry->art_url);
		entry->art_url = art_url;

		if (entry->art != NULL) {
			g_object_unref (entry->art);
			entry->art = NULL;
		}
	}
	else {
		g_free (art_url);
	}

	g_free (entry->title);
	entry->title = title;

	g_free (entry->artist);
	entry->artist = artist;
}

static void
mc_player_entry_dispose (GObject *object)
{
	McPlayerEntry *entry = MC_PLAYER_ENTRY (object);

	if (entry->art_cancel != NULL) {
		g_cancellable_cancel (entry->art_cancel);
		g_object_unref (entry->art_cancel);
		entry->art_cancel = NULL;
	}

	if (entry->player != NULL) {
		g_signal_handlers_disconnect_by_data (G_OBJECT (entry->player),
		                                      entry);
		g_object_unref (entry->player);
		entry->player = NULL;
	}

	G_OBJECT_CLASS (mc_player_entry_parent_class)->dispose (object);
}

static void
mc_player_entry_finalize (GObject *object)
{
	McPlayerEntry *entry = MC_PLAYER_ENTRY (object);

	g_free (entry->name);
	g_free (entry->instance);
	g_free (entry->title);
	g_free (entry->artist);
	g_free (entry->art_url);

	if (entry->art != NULL)
		g_object_unref (entry->art);

	G_OBJECT_CLASS (mc_player_entry_parent_class)->finalize (object);
}

static void
mc_player_entry_class_init (McPlayerEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = mc_player_entry_dispose;
	object_class->finalize = mc_player_entry_finalize;
}

static void
mc_player_entry_init (McPlayerEntry *entry)
{
	entry->art_cancel = g_cancellable_new ();
}

/*
 * Applet widgets.
 */
static void
applet_update_buttons (MultimediaControlApplet *applet)
{
	gboolean active = FALSE;
	const gchar *icon_name = "media-playback-start";
	const gchar *name = NULL;

	if (applet->selected != NULL) {
		name = applet->selected->name;

		if (applet->selected->status != PLAYERCTL_PLAYBACK_STATUS_STOPPED)
			active = TRUE;

		if (applet->selected->status == PLAYERCTL_PLAYBACK_STATUS_PLAYING)
			icon_name = "media-playback-pause";
	}

	gtk_widget_set_sensitive (applet->prev_button, active);
	gtk_button_set_image (GTK_BUTTON (applet->play_button),
	                      gtk_image_new_from_icon_name (icon_name,
	                                                    GTK_ICON_SIZE_BUTTON));
	gtk_widget_set_sensitive (applet->stop_button, active);
	gtk_widget_set_sensitive (applet->next_button, active);

	gtk_widget_set_tooltip_text (GTK_WIDGET (applet->applet),
	                             (name != NULL) ? name : _("Click to detect the player"));
}

/*
 * Popup menu.
 */
static void
applet_menu_refresh (MultimediaControlApplet *applet)
{
	McPlayerEntry *entry = applet->selected;
	gchar *text, *label;
	GtkWidget *image;

	if (applet->now_playing_action == NULL)
		return;

	text = applet_now_playing_text (applet);
	label = g_strdup_printf (_("Now Playing: %s"), text);
	gtk_action_set_label (applet->now_playing_action, label);
	g_free (label);
	g_free (text);

	/*
	 * Album art of the current track.
	 */
	if (applet->now_playing_item != NULL) {
		image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (applet->now_playing_item));

		if (image != NULL) {
			if (applet->art_now_playing && entry != NULL && entry->art != NULL)
				gtk_image_set_from_pixbuf (GTK_IMAGE (image), entry->art);
			else
				gtk_image_set_from_pixbuf (GTK_IMAGE (image), NULL);
		}
	}

	if (applet->art_now_playing && entry != NULL)
		entry_request_art (entry);
}

static void
player_item_toggled (GtkCheckMenuItem *item, McPlayerEntry *entry)
{
	if (gtk_check_menu_item_get_active (item))
		applet_select_player (entry->applet, entry);
}

static void
applet_menu_players_rebuild (MultimediaControlApplet *applet)
{
	GtkWidget *item, *hbox, *image, *label;
	McPlayerEntry *entry;
	GList *l, *children;
	gchar *text;
	GSList *group = NULL;

	if (applet->players_menu == NULL)
		return;

	children = gtk_container_get_children (GTK_CONTAINER (applet->players_menu));
	for (l = children; l != NULL; l = l->next)
		gtk_widget_destroy (GTK_WIDGET (l->data));
	g_list_free (children);

	if (applet->players == NULL) {
		item = gtk_menu_item_new_with_label (_("No players detected"));
		gtk_widget_set_sensitive (item, FALSE);
		gtk_menu_shell_append (GTK_MENU_SHELL (applet->players_menu), item);
		gtk_widget_show (item);

		return;
	}

	for (l = applet->players; l != NULL; l = l->next) {
		entry = l->data;

		item = gtk_radio_menu_item_new (group);
		group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));

		if (entry == applet->selected)
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);

		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

		image = gtk_image_new ();
		gtk_image_set_pixel_size (GTK_IMAGE (image), MC_ART_SIZE);
		gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

		text = mc_truncate_text (entry->name, 48);
		label = gtk_label_new (text);
		gtk_label_set_xalign (GTK_LABEL (label), 0.0);
		gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars (GTK_LABEL (label), 40);
		g_free (text);
		gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

		gtk_container_add (GTK_CONTAINER (item), hbox);
		gtk_widget_show_all (hbox);

		/*
		 * What is playing on the other players.
		 */
		if (applet->show_other_players && entry != applet->selected) {
			gchar *playing;

			if (entry->artist != NULL && entry->title != NULL)
				playing = g_strdup_printf ("%s — %s", entry->artist, entry->title);
			else if (entry->title != NULL)
				playing = g_strdup (entry->title);
			else
				playing = g_strdup (entry->name);

			text = mc_truncate_text (playing, 48);
			g_free (playing);
			gtk_label_set_text (GTK_LABEL (label), text);
			g_free (text);
		}

		/*
		 * Album art of the other players.
		 */
		if (applet->art_other_players) {
			if (entry->art != NULL)
				gtk_image_set_from_pixbuf (GTK_IMAGE (image), entry->art);
			else
				entry_request_art (entry);
		}

		g_signal_connect (G_OBJECT (item), "toggled",
		                  G_CALLBACK (player_item_toggled), entry);

		gtk_menu_shell_append (GTK_MENU_SHELL (applet->players_menu), item);
		gtk_widget_show (item);
	}
}

/*
 * Player selection.
 */
static void
applet_select_player (MultimediaControlApplet *applet, McPlayerEntry *entry)
{
	if (applet->selected == entry)
		return;

	applet->selected = entry;

	applet_update_buttons (applet);
	applet_menu_refresh (applet);
}

/*
 * Playerctl signals.
 */
static void
player_playback_status (PlayerctlPlayer          *player,
                        PlayerctlPlaybackStatus   playback_status,
                        McPlayerEntry            *entry)
{
	entry->status = playback_status;

	if (entry == entry->applet->selected)
		applet_update_buttons (entry->applet);
}

static void
player_metadata (PlayerctlPlayer *player,
                 GVariant        *metadata,
                 McPlayerEntry   *entry)
{
	player_entry_refresh_metadata (entry);

	entry->art_failed = FALSE;

	if (entry == entry->applet->selected) {
		applet_update_buttons (entry->applet);
		applet_menu_refresh (entry->applet);
	}

	applet_menu_players_rebuild (entry->applet);
}

/*
 * Player management.
 */
static McPlayerEntry *
applet_find_entry (MultimediaControlApplet *applet, PlayerctlPlayer *player)
{
	GList *l;

	for (l = applet->players; l != NULL; l = l->next) {
		McPlayerEntry *entry = l->data;

		if (entry->player == player)
			return entry;
	}

	return NULL;
}

static void
applet_add_player (MultimediaControlApplet *applet,
                   PlayerctlPlayerName     *player_name)
{
	McPlayerEntry *entry;
	PlayerctlPlayer *player;
	GError *error = NULL;

	player = playerctl_player_new_from_name (player_name, &error);
	if (player == NULL) {
		g_warning ("Could not connect to player '%s': %s",
		           player_name->name != NULL ? player_name->name : "?",
		           error->message);
		g_error_free (error);
		return;
	}

	playerctl_player_manager_manage_player (applet->manager, player);

	entry = g_object_new (MC_TYPE_PLAYER_ENTRY, NULL);
	entry->player = player;
	entry->applet = applet;
	entry->name = g_strdup (player_name->name != NULL ? player_name->name : player_name->instance);
	entry->instance = g_strdup (player_name->instance);

	g_object_get (player, "playback-status", &entry->status, NULL);

	g_signal_connect (G_OBJECT (player), "playback-status",
	                  G_CALLBACK (player_playback_status), entry);
	g_signal_connect (G_OBJECT (player), "metadata",
	                  G_CALLBACK (player_metadata), entry);

	player_entry_refresh_metadata (entry);

	applet->players = g_list_append (applet->players, entry);

	/*
	 * Select the first player detected.
	 */
	if (applet->selected == NULL) {
		applet->selected = entry;
		applet_update_buttons (applet);
		applet_menu_refresh (applet);
	}

	applet_menu_players_rebuild (applet);
}

static void
manager_name_appeared (PlayerctlPlayerManager  *manager,
                       PlayerctlPlayerName     *player_name,
                       MultimediaControlApplet *applet)
{
	applet_add_player (applet, player_name);
}

static void
manager_player_vanished (PlayerctlPlayerManager  *manager,
                         PlayerctlPlayer         *player,
                         MultimediaControlApplet *applet)
{
	McPlayerEntry *entry;

	entry = applet_find_entry (applet, player);
	if (entry == NULL)
		return;

	applet->players = g_list_remove (applet->players, entry);

	if (applet->selected == entry) {
		/*
		 * Fall back to the first remaining player.
		 */
		applet->selected = (applet->players != NULL) ? applet->players->data : NULL;
	}

	g_object_unref (entry);

	applet_update_buttons (applet);
	applet_menu_refresh (applet);
	applet_menu_players_rebuild (applet);
}

/*
 * Player buttons.
 */
static void
player_button_prev (GtkWidget               *widget,
                    MultimediaControlApplet *applet)
{
	if (applet->selected == NULL)
		return;

	playerctl_player_previous (applet->selected->player, NULL);
}

static void
player_button_play_pause (GtkWidget               *widget,
                          MultimediaControlApplet *applet)
{
	if (applet->selected == NULL)
		return;

	playerctl_player_play_pause (applet->selected->player, NULL);
}

static void
player_button_stop (GtkWidget               *widget,
                    MultimediaControlApplet *applet)
{
	if (applet->selected == NULL)
		return;

	playerctl_player_stop (applet->selected->player, NULL);
}

static void
player_button_next (GtkWidget               *widget,
                    MultimediaControlApplet *applet)
{
	if (applet->selected == NULL)
		return;

	playerctl_player_next (applet->selected->player, NULL);
}

/*
 * GSettings.
 */
static void
applet_settings_changed (GSettings               *settings,
                         const gchar             *key,
                         MultimediaControlApplet *applet)
{
	applet->show_other_players = g_settings_get_boolean (settings, MC_SETTINGS_KEY_OTHER_PLAYERS);
	applet->art_now_playing = g_settings_get_boolean (settings, MC_SETTINGS_KEY_ART_NOW_PLAYING);
	applet->art_other_players = g_settings_get_boolean (settings, MC_SETTINGS_KEY_ART_OTHER_PLAYERS);

	applet_menu_refresh (applet);
	applet_menu_players_rebuild (applet);
}

static void
applet_settings_sync (MultimediaControlApplet *applet)
{
	applet_settings_changed (applet->settings, "", applet);
}

/*
 * About dialog.
 */
void
multimedia_control_about_show (GtkAction               *action,
                               MultimediaControlApplet *applet)
{
	const gchar *authors[] = {
		"Matias De Lellis <mati86dl@gmail.com>",
		"hmtheboy154 <buingoc67@gmail.com>",
		NULL
	};

	if (applet->about_dialog != NULL) {
		gtk_window_present (GTK_WINDOW (applet->about_dialog));
		return;
	}

	applet->about_dialog = gtk_about_dialog_new ();
	gtk_window_set_icon_name (GTK_WINDOW (applet->about_dialog), "multimedia-player");
	g_object_set (applet->about_dialog,
	              "program-name", _("Multimedia Controls"),
	              "version", PACKAGE_VERSION,
	              "copyright", "Copyright © 2014 Matias De Lellis\nCopyright © 2026 hmtheboy154",
	              "comments", _("Control MPRIS2 compatible media players from the MATE panel"),
	              "website", "https://github.com/hmtheboy154/mate-multimedia-control",
	              "website-label", _("Multimedia Controls website"),
	              "logo-icon-name", "multimedia-player",
	              "license-type", GTK_LICENSE_GPL_3_0,
	              "authors", authors,
	              NULL);
	g_signal_connect (G_OBJECT (applet->about_dialog), "destroy",
	                  G_CALLBACK (gtk_widget_destroyed), &applet->about_dialog);

	gtk_widget_show (applet->about_dialog);
}

/*
 * Popup menu setup.
 *
 * The menu is owned by the lib's ui manager, and it is built with
 * actions. We only take our proxies to update them dynamically:
 * the "Now Playing" entry shows the current track (with its album
 * art when enabled), and the "Players" entry offers a submenu with
 * a radio item per player.
 */
static void
applet_setup_menu (MultimediaControlApplet *applet)
{
	GtkWidget *item;
	GSList *proxies;

	applet->now_playing_action = gtk_action_new ("NowPlaying",
	                                             _("Now Playing"), NULL, NULL);
	gtk_action_set_sensitive (applet->now_playing_action, FALSE);
	gtk_action_group_add_action (applet->action_group,
	                             applet->now_playing_action);

	applet->players_action = gtk_action_new ("Players",
	                                         _("Players"), NULL, NULL);
	/*
	 * The ui manager hides submenu entries with an empty submenu;
	 * the player list is filled dynamically, so the entry must
	 * always stay visible.
	 */
	g_object_set (applet->players_action, "hide-if-empty", FALSE, NULL);
	gtk_action_group_add_action (applet->action_group,
	                             applet->players_action);

	mate_panel_applet_setup_menu (applet->applet, applet_menu_xml,
	                              applet->action_group);

	/*
	 * The "Now Playing" proxy is a GtkImageMenuItem: enable its
	 * image slot for the album art.
	 */
	proxies = gtk_action_get_proxies (applet->now_playing_action);
	if (proxies != NULL) {
		GtkWidget *image;

		applet->now_playing_item = GTK_WIDGET (proxies->data);
		gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (applet->now_playing_item),
		                                           TRUE);
		image = gtk_image_new ();
		gtk_image_set_pixel_size (GTK_IMAGE (image), MC_ART_SIZE);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (applet->now_playing_item),
		                               image);
	}

	/*
	 * The "Players" proxy holds the empty submenu created by the
	 * ui manager; we fill it with a radio item per player.
	 */
	proxies = gtk_action_get_proxies (applet->players_action);
	if (proxies != NULL) {
		item = GTK_WIDGET (proxies->data);
		applet->players_menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (item));
	}
}

/*
 * Applet construction and destruction.
 */
static void
applet_destroy (MatePanelApplet         *applet_widget,
                MultimediaControlApplet *applet)
{
	g_assert (applet != NULL);

	/*
	 * Close the preferences and about dialogs if they are open.
	 */
	if (applet->preferences_dialog != NULL)
		gtk_widget_destroy (applet->preferences_dialog);
	if (applet->about_dialog != NULL)
		gtk_widget_destroy (applet->about_dialog);

	/*
	 * Cancel the pending artwork loads before freeing anything.
	 */
	g_list_free_full (applet->players, g_object_unref);
	applet->players = NULL;
	applet->selected = NULL;

	if (applet->settings != NULL) {
		g_signal_handlers_disconnect_by_data (G_OBJECT (applet->settings),
		                                      applet);
		g_object_unref (applet->settings);
	}

	if (applet->action_group != NULL)
		g_object_unref (applet->action_group);

	if (applet->manager != NULL) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (applet->manager),
		                                      G_CALLBACK (manager_name_appeared),
		                                      applet);
		g_signal_handlers_disconnect_by_func (G_OBJECT (applet->manager),
		                                      G_CALLBACK (manager_player_vanished),
		                                      applet);

		g_object_unref (applet->manager);
	}

	g_free (applet);

	return;
}

static gboolean
multimedia_control_applet_factory (MatePanelApplet *applet_widget,
                                   const gchar     *iid,
                                   gpointer         data)
{
	MultimediaControlApplet *applet;
	GtkWidget *box, *button;
	GList *names, *l;
	GError *error = NULL;

	if (strcmp (iid, APPLET_ID) != 0)
		return FALSE;

	applet = g_malloc0 (sizeof (MultimediaControlApplet));

	applet->applet = applet_widget;

	/*
	 * GSettings.
	 */
	applet->settings = g_settings_new (MC_SETTINGS_SCHEMA);
	g_signal_connect (G_OBJECT (applet->settings), "changed",
	                  G_CALLBACK (applet_settings_changed), applet);
	applet_settings_sync (applet);

	/*
	 * Mpris2: playerctl backend.
	 */
	applet->manager = playerctl_player_manager_new (&error);
	if (applet->manager == NULL) {
		g_warning ("Could not create the player manager: %s", error->message);
		g_error_free (error);
		g_object_unref (applet->settings);
		g_free (applet);
		return FALSE;
	}

	g_signal_connect (G_OBJECT (applet->manager), "name-appeared",
	                  G_CALLBACK (manager_name_appeared), applet);
	g_signal_connect (G_OBJECT (applet->manager), "player-vanished",
	                  G_CALLBACK (manager_player_vanished), applet);

	/*
	 * Widgets.
	 */
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	button = GTK_WIDGET (gtk_button_new_from_icon_name ("media-skip-backward",
	                                                    GTK_ICON_SIZE_BUTTON));
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	g_signal_connect (G_OBJECT (button), "clicked",
	                  G_CALLBACK (player_button_prev), applet);
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (button),
	                    FALSE, FALSE, 0);
	applet->prev_button = button;

	button = GTK_WIDGET (gtk_button_new_from_icon_name ("media-playback-start",
	                                                    GTK_ICON_SIZE_BUTTON));
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	g_signal_connect (G_OBJECT (button), "clicked",
	                  G_CALLBACK (player_button_play_pause), applet);
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (button),
	                    FALSE, FALSE, 0);
	applet->play_button = button;

	button = GTK_WIDGET (gtk_button_new_from_icon_name ("media-playback-stop",
	                                                    GTK_ICON_SIZE_BUTTON));
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	g_signal_connect (G_OBJECT (button), "clicked",
	                  G_CALLBACK (player_button_stop), applet);
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (button),
	                    FALSE, FALSE, 0);
	applet->stop_button = button;

	button = GTK_WIDGET (gtk_button_new_from_icon_name ("media-skip-forward",
	                                                    GTK_ICON_SIZE_BUTTON));
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	g_signal_connect (G_OBJECT (button), "clicked",
	                  G_CALLBACK (player_button_next), applet);
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (button),
	                    FALSE, FALSE, 0);
	applet->next_button = button;

	gtk_container_add (GTK_CONTAINER (applet_widget), box);

	/*
	 * Popup menu.
	 */
	applet->action_group = gtk_action_group_new ("MultimediaControlAppletActions");
	gtk_action_group_set_translation_domain (applet->action_group,
	                                         GETTEXT_PACKAGE);
	gtk_action_group_add_actions (applet->action_group, applet_menu_actions,
	                              G_N_ELEMENTS (applet_menu_actions), applet);

	applet_setup_menu (applet);

	g_signal_connect (G_OBJECT (applet_widget), "destroy",
	                  G_CALLBACK (applet_destroy), (gpointer) applet);

	gtk_widget_show_all (GTK_WIDGET (applet_widget));

	/*
	 * Check the players that are already running.
	 */
	g_object_get (applet->manager, "player-names", &names, NULL);

	for (l = names; l != NULL; l = l->next)
		applet_add_player (applet, l->data);

	applet_update_buttons (applet);

	return TRUE;
}

MATE_PANEL_APPLET_OUT_PROCESS_FACTORY (APPLET_FACTORY, PANEL_TYPE_APPLET,
                                       APPLET_NAME,
                                       multimedia_control_applet_factory, NULL)
