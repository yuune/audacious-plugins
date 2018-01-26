/*
 * settings.c
 * Copyright 2013 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include "gtkui.h"

#include <libaudcore/i18n.h>
#include <libaudcore/preferences.h>

#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"

static void redisplay_playlists ()
{
    pl_notebook_purge ();
    pl_notebook_populate ();
}

static const PreferencesWidget gtkui_widgets[] = {
    WidgetLabel (N_("<b>Playlist Tabs</b>")),
    WidgetCheck (N_("Always show tabs"),
        WidgetBool ("gtkui", "playlist_tabs_visible", show_hide_playlist_tabs)),
    WidgetCheck (N_("Show entry counts"),
        WidgetBool ("gtkui", "entry_count_visible", redisplay_playlists)),
    WidgetCheck (N_("Show close buttons"),
        WidgetBool ("gtkui", "close_button_visible", redisplay_playlists)),
    WidgetLabel (N_("<b>Playlist Columns</b>")),
    WidgetCustomGTK (pw_col_create_chooser),
    WidgetCheck (N_("Show column headers"),
        WidgetBool ("gtkui", "playlist_headers", redisplay_playlists)),
    WidgetLabel (N_("<b>Miscellaneous</b>")),
    WidgetSpin (N_("Arrow keys seek by:"),
        WidgetFloat ("gtkui", "step_size", update_step_size),
        {0.1, 60, 0.1, N_("seconds")}),
    WidgetCheck (N_("Scroll on song change"),
        WidgetBool ("gtkui", "autoscroll")),
    WidgetLabel (N_("<b>UI Tweaks (need restart audacious after changes)</b>")),
    WidgetCheck (N_("Show volume button at toolbar"),
        WidgetBool ("gtkui", "volume_button_visible", nullptr)),
    WidgetCheck (N_("Move the toolbar to the bottom of infobar"),
        WidgetBool ("gtkui", "toolbar_bottom_visible", nullptr))
};

const PluginPreferences gtkui_prefs = {{gtkui_widgets}};
