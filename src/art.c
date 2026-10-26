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

#include "art.h"

#include <gio/gio.h>
#include <libsoup/soup.h>

/*
 * Album art handling.
 *
 * Sources are the "mpris:artUrl" metadata entries published by MPRIS2
 * players. Local files ("file://") are loaded directly; remote artwork
 * ("http://" and "https://") is downloaded with libsoup and stored in a
 * cache directory below $XDG_CACHE_HOME, keyed by a hash of the uri, so
 * artwork is only downloaded once.
 *
 * The cached files keep the original bytes; scaling to the requested
 * size happens in memory while loading with gdk-pixbuf.
 */

static SoupSession *art_session = NULL;

static void
art_ensure_session (void)
{
	if (art_session == NULL)
		art_session = soup_session_new ();
}

static void
art_ensure_cache_dir (void)
{
	gchar *dir;

	dir = g_build_filename (g_get_user_cache_dir (), "mate-multimedia-control", NULL);
	g_mkdir_with_parents (dir, 0700);
	g_free (dir);
}

static gchar *
art_cache_path (const gchar *art_url)
{
	gchar *hash, *dir, *path;

	hash = g_compute_checksum_for_string (G_CHECKSUM_SHA1, art_url, -1);
	dir = g_build_filename (g_get_user_cache_dir (), "mate-multimedia-control", NULL);
	path = g_build_filename (dir, hash, NULL);

	g_free (hash);
	g_free (dir);

	return path;
}

static GdkPixbuf *
art_scale_file (const gchar *filename, gint size, GError **error)
{
	return gdk_pixbuf_new_from_file_at_scale (filename, size, size, TRUE, error);
}

static GdkPixbuf *
art_scale_bytes (GBytes *bytes, gint size, GError **error)
{
	GInputStream *stream;
	GdkPixbuf *pixbuf;

	stream = g_memory_input_stream_new_from_bytes (bytes);
	pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream, size, size, TRUE,
	                                              NULL, error);
	g_object_unref (stream);

	return pixbuf;
}

/*
 * Task data for the asynchronous loading.
 */
typedef struct {
	gchar  *art_url;
	gint    size;
} ArtRequest;

static void
art_request_free (gpointer data)
{
	ArtRequest *request = data;

	g_free (request->art_url);
	g_slice_free (ArtRequest, request);
}

/*
 * Complete the task, taking ownership of the pixbuf and of the error:
 * exactly one of them must be set.
 */
static void
art_task_return (GTask *task, GdkPixbuf *pixbuf, GError *error)
{
	if (pixbuf != NULL) {
		g_task_return_pointer (task, pixbuf, g_object_unref);
	}
	else if (error != NULL) {
		g_task_return_error (task, error);
	}
	else {
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
		                         "Could not load the artwork");
	}

	g_object_unref (task);
}

static void
art_http_downloaded (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
	SoupSession *session = SOUP_SESSION (source);
	GTask *task = user_data;
	ArtRequest *request = g_task_get_task_data (task);
	GBytes *bytes;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	gchar *path;

	bytes = soup_session_send_and_read_finish (session, result, &error);
	if (bytes == NULL) {
		art_task_return (task, NULL, error);
		return;
	}

	/*
	 * Store the original artwork in the cache, then scale it.
	 */
	art_ensure_cache_dir ();
	path = art_cache_path (request->art_url);
	g_file_set_contents (path, g_bytes_get_data (bytes, NULL),
	                     g_bytes_get_size (bytes), NULL);
	g_free (path);

	pixbuf = art_scale_bytes (bytes, request->size, &error);
	g_bytes_unref (bytes);

	art_task_return (task, pixbuf, error);
}

static void
art_load_in_idle (GTask        *task,
                  gpointer      source_object,
                  gpointer      task_data,
                  GCancellable *cancellable)
{
	ArtRequest *request = task_data;
	gchar *path;
	GdkPixbuf *pixbuf;
	GError *error = NULL;

	/*
	 * Artwork already cached? Load it and scale it.
	 */
	path = art_cache_path (request->art_url);
	if (g_file_test (path, G_FILE_TEST_EXISTS)) {
		pixbuf = art_scale_file (path, request->size, &error);
		g_free (path);
		art_task_return (task, pixbuf, error);
		return;
	}
	g_free (path);

	/*
	 * Remote artwork: download it and cache the original bytes.
	 */
	if (g_str_has_prefix (request->art_url, "http://") ||
	    g_str_has_prefix (request->art_url, "https://")) {
		SoupMessage *message;

		art_ensure_session ();

		message = soup_message_new (SOUP_METHOD_GET, request->art_url);
		soup_session_send_and_read_async (art_session, message,
		                                  G_PRIORITY_DEFAULT, NULL,
		                                  art_http_downloaded, task);
		g_object_unref (message);
		return;
	}

	/*
	 * Local files are loaded directly and are not cached: the players
	 * provide them and they change with every track anyway.
	 */
	if (g_str_has_prefix (request->art_url, "file://")) {
		gchar *filename;

		filename = g_filename_from_uri (request->art_url, NULL, &error);
		if (filename != NULL) {
			pixbuf = art_scale_file (filename, request->size, &error);
			g_free (filename);
		}

		art_task_return (task, pixbuf, error);
		return;
	}

	{
		GError *unsupported = NULL;

		g_set_error (&unsupported, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
		             "Unsupported artwork uri: %s", request->art_url);
		art_task_return (task, NULL, unsupported);
	}
}

/*
 * Load the artwork of an "mpris:artUrl" at the requested size. The
 * completion callback receives a scaled #GdkPixbuf, or an error when
 * the artwork could not be loaded. The owner object is kept alive
 * until the completion callback has run.
 */
void
multimedia_art_get_async (gpointer             owner,
                          const gchar         *art_url,
                          gint                 size,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
	ArtRequest *request;
	GTask *task;

	request = g_slice_new0 (ArtRequest);
	request->art_url = g_strdup (art_url);
	request->size = size;

	task = g_task_new (owner, NULL, callback, user_data);
	g_task_set_task_data (task, request, art_request_free);
	g_task_set_return_on_cancel (task, TRUE);
	g_task_run_in_thread (task, art_load_in_idle);
}

/*
 * Finish a multimedia_art_get_async() operation. Returns a full
 * reference to the scaled pixbuf, or NULL with the error set.
 */
GdkPixbuf *
multimedia_art_get_finish (GAsyncResult  *result,
                           GError       **error)
{
	return g_task_propagate_pointer (G_TASK (result), error);
}
