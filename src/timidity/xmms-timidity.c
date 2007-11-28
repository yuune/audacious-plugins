/*
    xmms-timidity - MIDI Plugin for XMMS
    Copyright (C) 2004 Konstantin Korikov <lostclus@ua.fm>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "config.h"

#include <audacious/util.h>
#include <audacious/configdb.h>
#include <audacious/main.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <timidity.h>
#include <stdio.h>
#include <audacious/output.h>
#include <audacious/i18n.h>

#include "xmms-timidity.h"
#include "interface.h"

InputPlugin xmmstimid_ip = {
	.description = "TiMidity Audio Plugin",
	.init = xmmstimid_init,
	.about = xmmstimid_about,
	.configure = xmmstimid_configure,
	.play_file = xmmstimid_play_file,
	.stop = xmmstimid_stop,
	.pause = xmmstimid_pause,
	.seek = xmmstimid_seek,
	.get_time = xmmstimid_get_time,
	.cleanup = xmmstimid_cleanup,
	.get_song_info = xmmstimid_get_song_info,
	.is_our_file_from_vfs = xmmstimid_is_our_fd,
};

InputPlugin *timidity_iplist[] = { &xmmstimid_ip, NULL };

DECLARE_PLUGIN(timidity, NULL, NULL, timidity_iplist, NULL, NULL, NULL, NULL, NULL);

static struct {
	gchar *config_file;
	gint rate;
	gint bits;
	gint channels;
	gint buffer_size;
} xmmstimid_cfg;

static gboolean xmmstimid_initialized = FALSE;
static GThread *xmmstimid_decode_thread;
static gboolean xmmstimid_audio_error = FALSE;
static MidSongOptions xmmstimid_opts;
static MidSong *xmmstimid_song;
static gint xmmstimid_seek_to;

static GtkWidget *xmmstimid_conf_wnd = NULL, *xmmstimid_about_wnd = NULL;
static GtkEntry
	*xmmstimid_conf_config_file;
static GtkToggleButton
	*xmmstimid_conf_rate_11000,
	*xmmstimid_conf_rate_22000,
	*xmmstimid_conf_rate_44100;
static GtkToggleButton
	*xmmstimid_conf_bits_8,
	*xmmstimid_conf_bits_16;
static GtkToggleButton
	*xmmstimid_conf_channels_1,
	*xmmstimid_conf_channels_2;

void xmmstimid_init(void) {
	ConfigDb *db;

	xmmstimid_cfg.config_file = NULL;
	xmmstimid_cfg.rate = 44100;
	xmmstimid_cfg.bits = 16;
	xmmstimid_cfg.channels = 2;
	xmmstimid_cfg.buffer_size = 512;

	db = aud_cfg_db_open();

	if (! aud_cfg_db_get_string(db, "timidity", "config_file",
                                &xmmstimid_cfg.config_file))
        xmmstimid_cfg.config_file = g_strdup("/etc/timidity/timidity.cfg");

	aud_cfg_db_get_int(db, "timidity", "samplerate", &xmmstimid_cfg.rate);
	aud_cfg_db_get_int(db, "timidity", "bits", &xmmstimid_cfg.bits);
	aud_cfg_db_get_int(db, "timidity", "channels", &xmmstimid_cfg.channels);
	aud_cfg_db_close(db);

	if (mid_init(xmmstimid_cfg.config_file) != 0) {
		xmmstimid_initialized = FALSE;
		return;
	}
	xmmstimid_initialized = TRUE;
}

void xmmstimid_about(void) {
	if (!xmmstimid_about_wnd) {
		gchar *about_title, *about_text;
		about_text = g_strjoin( "" ,
			_("TiMidity Plugin\nhttp://libtimidity.sourceforge.net\nby Konstantin Korikov") , NULL );
		about_title = g_strdup_printf( _("TiMidity Plugin %s") , PACKAGE_VERSION );
		xmmstimid_about_wnd = audacious_info_dialog( about_title , about_text , _("Ok") , FALSE , NULL , NULL );
		g_signal_connect(G_OBJECT(xmmstimid_about_wnd), "destroy",
					(GCallback)gtk_widget_destroyed, &xmmstimid_about_wnd);
		g_free(about_title);
		g_free(about_text);
	}
	else
	{
                gtk_window_present(GTK_WINDOW(xmmstimid_about_wnd));
	}
}

void xmmstimid_conf_ok(GtkButton *button, gpointer user_data);

void xmmstimid_configure(void) {
	GtkToggleButton *tb;
	if (xmmstimid_conf_wnd == NULL) {
		xmmstimid_conf_wnd = create_xmmstimid_conf_wnd();

#define get_conf_wnd_item(type, name) \
	type (g_object_get_data(G_OBJECT(xmmstimid_conf_wnd), name))
	
		xmmstimid_conf_config_file = get_conf_wnd_item(
				GTK_ENTRY, "config_file");
		xmmstimid_conf_rate_11000 = get_conf_wnd_item(
				GTK_TOGGLE_BUTTON, "rate_11000");
		xmmstimid_conf_rate_22000 = get_conf_wnd_item(
				GTK_TOGGLE_BUTTON, "rate_22000");
		xmmstimid_conf_rate_44100 = get_conf_wnd_item(
				GTK_TOGGLE_BUTTON, "rate_44100");
		xmmstimid_conf_bits_8 = get_conf_wnd_item(
				GTK_TOGGLE_BUTTON, "bits_8");
		xmmstimid_conf_bits_16 = get_conf_wnd_item(
				GTK_TOGGLE_BUTTON, "bits_16");
		xmmstimid_conf_channels_1 = get_conf_wnd_item(
				GTK_TOGGLE_BUTTON, "channels_1");
		xmmstimid_conf_channels_2 = get_conf_wnd_item(
				GTK_TOGGLE_BUTTON, "channels_2");

		gtk_signal_connect_object(
				get_conf_wnd_item(GTK_OBJECT, "conf_ok"),
				"clicked", G_CALLBACK(xmmstimid_conf_ok),
				NULL);
	}

	gtk_entry_set_text(xmmstimid_conf_config_file,
			xmmstimid_cfg.config_file);
	switch (xmmstimid_cfg.rate) {
		case 11000: tb = xmmstimid_conf_rate_11000; break;
		case 22000: tb = xmmstimid_conf_rate_22000; break;
		case 44100: tb = xmmstimid_conf_rate_44100; break;
		default: tb = NULL;
	}
	if (tb != NULL) gtk_toggle_button_set_active(tb, TRUE);
	switch (xmmstimid_cfg.bits) {
		case 8: tb = xmmstimid_conf_bits_8; break;
		case 16: tb = xmmstimid_conf_bits_16; break;
		default: tb = NULL;
	}
	if (tb != NULL) gtk_toggle_button_set_active(tb, TRUE);
	switch (xmmstimid_cfg.channels) {
		case 1: tb = xmmstimid_conf_channels_1; break;
		case 2: tb = xmmstimid_conf_channels_2; break;
		default: tb = NULL;
	}
	if (tb != NULL) gtk_toggle_button_set_active(tb, TRUE);

	gtk_widget_show(xmmstimid_conf_wnd);
        gtk_window_present(GTK_WINDOW(xmmstimid_conf_wnd));
}

void xmmstimid_conf_ok(GtkButton *button, gpointer user_data) {
	ConfigDb *db;

	if (gtk_toggle_button_get_active(xmmstimid_conf_rate_11000))
		xmmstimid_cfg.rate = 11000;
	else if (gtk_toggle_button_get_active(xmmstimid_conf_rate_22000))
		xmmstimid_cfg.rate = 22000;
	else if (gtk_toggle_button_get_active(xmmstimid_conf_rate_44100))
		xmmstimid_cfg.rate = 44100;
	if (gtk_toggle_button_get_active(xmmstimid_conf_bits_8))
		xmmstimid_cfg.bits = 8;
	else if (gtk_toggle_button_get_active(xmmstimid_conf_bits_16))
		xmmstimid_cfg.bits = 16;
	if (gtk_toggle_button_get_active(xmmstimid_conf_channels_1))
		xmmstimid_cfg.channels = 1;
	else if (gtk_toggle_button_get_active(xmmstimid_conf_channels_2))
		xmmstimid_cfg.channels = 2;

	db = aud_cfg_db_open();

	g_free(xmmstimid_cfg.config_file);
	xmmstimid_cfg.config_file = g_strdup(
        gtk_entry_get_text(xmmstimid_conf_config_file));

	aud_cfg_db_set_string(db, "timidity", "config_file", xmmstimid_cfg.config_file);

	aud_cfg_db_set_int(db, "timidity", "samplerate", xmmstimid_cfg.rate);
	aud_cfg_db_set_int(db, "timidity", "bits", xmmstimid_cfg.bits);
	aud_cfg_db_set_int(db, "timidity", "channels", xmmstimid_cfg.channels);
	aud_cfg_db_close(db);

	gtk_widget_hide(xmmstimid_conf_wnd);
}

static gint xmmstimid_is_our_fd( gchar * filename, VFSFile * fp )
{
	gchar magic_bytes[4];

	aud_vfs_fread( magic_bytes , 1 , 4 , fp );

	if ( !memcmp( magic_bytes , "MThd" , 4 ) )
		return TRUE;

	if ( !memcmp( magic_bytes , "RIFF" , 4 ) )
	{
	/* skip the four bytes after RIFF,
	   then read the next four */
	aud_vfs_fseek( fp , 4 , SEEK_CUR );
	aud_vfs_fread( magic_bytes , 1 , 4 , fp );
	if ( !memcmp( magic_bytes , "RMID" , 4 ) )
		return TRUE;
	}
	return FALSE;
}

static void *xmmstimid_play_loop(void *arg) {
	InputPlayback *playback = arg;
	size_t buffer_size;
	void *buffer;
	size_t bytes_read;
	AFormat fmt;

	buffer_size = ((xmmstimid_opts.format == MID_AUDIO_S16LSB) ? 16 : 8) * 
			xmmstimid_opts.channels / 8 *
			xmmstimid_opts.buffer_size;

	buffer = g_malloc(buffer_size);
	if (buffer == NULL) return(NULL);

	fmt = (xmmstimid_opts.format == MID_AUDIO_S16LSB) ? FMT_S16_LE : FMT_S8;

	while (playback->playing) {
		bytes_read = mid_song_read_wave(xmmstimid_song,
				buffer, buffer_size);

		if (bytes_read != 0)
			playback->pass_audio(playback,
					fmt, xmmstimid_opts.channels,
					bytes_read, buffer, &playback->playing);
		else {
		        playback->eof = TRUE;
		        playback->output->buffer_free ();
		        playback->output->buffer_free ();
		        while (playback->output->buffer_playing())
		                g_usleep(10000);
			playback->playing = FALSE;
		}

		if (xmmstimid_seek_to != -1) {
			mid_song_seek(xmmstimid_song, xmmstimid_seek_to * 1000);
			playback->output->flush(xmmstimid_seek_to * 1000);
			xmmstimid_seek_to = -1;
			bytes_read = 0;
		}
	}

	g_free(buffer);
	return(NULL);
}

static gchar *xmmstimid_get_title(gchar *filename) {
	Tuple *input;
	gchar *title;

	input = aud_tuple_new_from_filename(filename);

	title = aud_tuple_formatter_make_title_string(input, aud_get_gentitle_format());
	if (title == NULL || *title == '\0')
		title = g_strdup(aud_tuple_get_string(input, FIELD_FILE_NAME, NULL));

	aud_tuple_free(input);

	return title;
}

void xmmstimid_play_file(InputPlayback * playback) {
	char *filename = playback->filename;
	MidIStream *stream;
	gchar *title;

	if (!xmmstimid_initialized) {
		xmmstimid_init();
		if (!xmmstimid_initialized) return;
	}

	if (xmmstimid_song != NULL) {
		mid_song_free(xmmstimid_song);
		xmmstimid_song = NULL;
	}

	stream = mid_istream_open_file(filename);
	if (stream == NULL) return;

	xmmstimid_audio_error = FALSE;

	xmmstimid_opts.rate = xmmstimid_cfg.rate;
	xmmstimid_opts.format = (xmmstimid_cfg.bits == 16) ?
		MID_AUDIO_S16LSB : MID_AUDIO_S8;
	xmmstimid_opts.channels = xmmstimid_cfg.channels;
	xmmstimid_opts.buffer_size = xmmstimid_cfg.buffer_size;

	xmmstimid_song = mid_song_load(stream, &xmmstimid_opts);
	mid_istream_close(stream);

	if (xmmstimid_song == NULL) {
		playback->set_title(playback, _("Couldn't load MIDI file"));
		return;
	}

	if (playback->output->open_audio(
				(xmmstimid_opts.format == MID_AUDIO_S16LSB) ?
				FMT_S16_LE : FMT_S8,
				xmmstimid_opts.rate, xmmstimid_opts.channels) == 0) {
		xmmstimid_audio_error = TRUE;
		mid_song_free(xmmstimid_song);
		xmmstimid_song = NULL;
		return;
	}

	title = xmmstimid_get_title(filename);
	playback->set_params(playback, title,
			mid_song_get_total_time(xmmstimid_song),
			0, xmmstimid_opts.rate, xmmstimid_opts.channels);
	g_free(title);

	mid_song_start(xmmstimid_song);
	playback->playing = TRUE;
	playback->eof = FALSE;
	xmmstimid_seek_to = -1;

	xmmstimid_decode_thread = g_thread_self();
	playback->set_pb_ready(playback);
	xmmstimid_play_loop(playback);
}

void xmmstimid_stop(InputPlayback * playback) {
	if (xmmstimid_song != NULL && playback->playing) {
		playback->playing = FALSE;
		g_thread_join(xmmstimid_decode_thread);
		playback->output->close_audio();
		mid_song_free(xmmstimid_song);
		xmmstimid_song = NULL;
	}
}

void xmmstimid_pause(InputPlayback * playback, short p) {
	playback->output->pause(p);
}

void xmmstimid_seek(InputPlayback * playback, int time) {
	xmmstimid_seek_to = time;
	playback->eof = FALSE;

	while (xmmstimid_seek_to != -1)
		g_usleep(10000);
}

int xmmstimid_get_time(InputPlayback * playback) {
	if (xmmstimid_audio_error)
		return -2;
	if (xmmstimid_song == NULL)
		return -1;
	if (!playback->playing || (playback->eof &&
				playback->output->buffer_playing()))
		return -1;

	return mid_song_get_time(xmmstimid_song);
}

void xmmstimid_cleanup(void) {
	if (xmmstimid_cfg.config_file) {
		free(xmmstimid_cfg.config_file);
		xmmstimid_cfg.config_file = NULL;
	}

	if (xmmstimid_initialized)
		mid_exit();
}

void xmmstimid_get_song_info(char *filename, char **title, int *length) {
	MidIStream *stream;
	MidSongOptions opts;
	MidSong *song;

	if (!xmmstimid_initialized) {
		xmmstimid_init();
		if (!xmmstimid_initialized) return;
	}

	stream = mid_istream_open_file(filename);
	if (stream == NULL) return;

	opts.rate = xmmstimid_cfg.rate;
	opts.format = (xmmstimid_cfg.bits == 16) ?
		MID_AUDIO_S16LSB : MID_AUDIO_S8;
	opts.channels = xmmstimid_cfg.channels;
	opts.buffer_size = 8;

	song = mid_song_load(stream, &opts);
	mid_istream_close(stream);

	if (song == NULL) return;

	*length = mid_song_get_total_time(song);
	*title = xmmstimid_get_title(filename);

	mid_song_free(song);
}
