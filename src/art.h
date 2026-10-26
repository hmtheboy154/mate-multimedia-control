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

#ifndef __MULTIMEDIA_CONTROL_ART_H__
#define __MULTIMEDIA_CONTROL_ART_H__

#include <gdk-pixbuf/gdk-pixbuf.h>

void       multimedia_art_get_async (gpointer             owner,
                                     const gchar         *art_url,
                                     gint                 size,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data);
GdkPixbuf *multimedia_art_get_finish (GAsyncResult  *result,
                                      GError       **error);

#endif /* __MULTIMEDIA_CONTROL_ART_H__ */
