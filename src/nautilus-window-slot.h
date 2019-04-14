/*
   nautilus-window-slot.h: Nautilus window slot
 
   Copyright (C) 2008 Free Software Foundation, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
  
   Author: Christian Neumair <cneumair@gnome.org>
*/

#ifndef NAUTILUS_WINDOW_SLOT_H
#define NAUTILUS_WINDOW_SLOT_H

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

typedef enum {
	NAUTILUS_LOCATION_CHANGE_STANDARD,
	NAUTILUS_LOCATION_CHANGE_BACK,
	NAUTILUS_LOCATION_CHANGE_FORWARD,
	NAUTILUS_LOCATION_CHANGE_RELOAD
} NautilusLocationChangeType;

#define NAUTILUS_TYPE_WINDOW_SLOT (nautilus_window_slot_get_type ())
G_DECLARE_DERIVABLE_TYPE (NautilusWindowSlot, nautilus_window_slot, NAUTILUS, WINDOW_SLOT, GtkBox)

#include "nautilus-query-editor.h"
#include "nautilus-files-view.h"
#include "nautilus-view.h"
#include "nautilus-window.h"
#include "nautilus-toolbar-menu-sections.h"

typedef struct
{
    NautilusFile *file;
    gint view_before_search;
    GList *back_list;
    GList *forward_list;
} RestoreTabData;

struct _NautilusWindowSlotClass {
	GtkBoxClass parent_class;

	/* wrapped NautilusWindowInfo signals, for overloading */
	void (* active)   (NautilusWindowSlot *slot);
	void (* inactive) (NautilusWindowSlot *slot);

        /* Use this in case the subclassed slot has some special views differents
         * that the ones supported here. You can return your nautilus view
         * subclass in this function.
         */
        NautilusView*  (* get_view_for_location) (NautilusWindowSlot *slot,
                                                  GFile              *location);
        /* Whether this type of slot handles the location or not. This can be used
         * for the special slots which handle special locations like the desktop
         * or the other locations. */
        gboolean (* handles_location) (NautilusWindowSlot *slot,
                                       GFile              *location);
};

NautilusWindowSlot * nautilus_window_slot_new              (NautilusWindow     *window);

NautilusWindow * nautilus_window_slot_get_window           (NautilusWindowSlot *slot);
void             nautilus_window_slot_set_window           (NautilusWindowSlot *slot,
							    NautilusWindow     *window);

void nautilus_window_slot_open_location_full              (NautilusWindowSlot      *slot,
                                                           GFile                   *location,
                                                           NautilusWindowOpenFlags  flags,
                                                           GList                   *new_selection);

GFile * nautilus_window_slot_get_location		   (NautilusWindowSlot *slot);

NautilusBookmark *nautilus_window_slot_get_bookmark        (NautilusWindowSlot *slot);

GList * nautilus_window_slot_get_back_history              (NautilusWindowSlot *slot);
GList * nautilus_window_slot_get_forward_history           (NautilusWindowSlot *slot);

gboolean nautilus_window_slot_get_allow_stop               (NautilusWindowSlot *slot);
void     nautilus_window_slot_set_allow_stop		   (NautilusWindowSlot *slot,
							    gboolean	        allow_stop);
void     nautilus_window_slot_stop_loading                 (NautilusWindowSlot *slot);

const gchar *nautilus_window_slot_get_title                (NautilusWindowSlot *slot);
void         nautilus_window_slot_update_title		   (NautilusWindowSlot *slot);

gboolean nautilus_window_slot_handle_event       	   (NautilusWindowSlot *slot,
							    GdkEventKey        *event);

void    nautilus_window_slot_queue_reload		   (NautilusWindowSlot *slot);

GIcon*   nautilus_window_slot_get_icon                     (NautilusWindowSlot *slot);

NautilusToolbarMenuSections * nautilus_window_slot_get_toolbar_menu_sections (NautilusWindowSlot *slot);

gboolean nautilus_window_slot_get_active                   (NautilusWindowSlot *slot);

void     nautilus_window_slot_set_active                   (NautilusWindowSlot *slot,
                                                            gboolean            active);
gboolean nautilus_window_slot_get_loading                  (NautilusWindowSlot *slot);

void     nautilus_window_slot_search                       (NautilusWindowSlot *slot,
                                                            NautilusQuery      *query);

gboolean nautilus_window_slot_handles_location (NautilusWindowSlot *self,
                                                GFile              *location);

void nautilus_window_slot_restore_from_data (NautilusWindowSlot *self,
                                             RestoreTabData     *data);

RestoreTabData* nautilus_window_slot_get_restore_tab_data (NautilusWindowSlot *self);

/* Only used by slot-dnd */
NautilusView*  nautilus_window_slot_get_current_view       (NautilusWindowSlot *slot);

#endif /* NAUTILUS_WINDOW_SLOT_H */
