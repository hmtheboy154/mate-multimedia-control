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

#include <gtk/gtk.h>
#include <mate-panel-applet.h>
#include <playerctl/playerctl.h>

#define APPLET_FACTORY "MultimediaControlAppletFactory"
#define APPLET_ID "MultimediaControlApplet"
#define APPLET_NAME "multimedia-control"

typedef struct {
	MatePanelApplet        *applet;

	PlayerctlPlayerManager *manager;
	PlayerctlPlayer        *player;

	GtkWidget              *prev_button;
	GtkWidget              *play_button;
	GtkWidget              *stop_button;
	GtkWidget              *next_button;
} MultimediaControlApplet;

#endif /* __MULTIMEDIA_CONTROL_APPLET_H__ */
