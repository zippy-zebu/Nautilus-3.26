/* gnome-canvas-container-private.h

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef NAUTILUS_CANVAS_CONTAINER_PRIVATE_H
#define NAUTILUS_CANVAS_CONTAINER_PRIVATE_H

#include <eel/eel-glib-extensions.h>
#include "nautilus-canvas-item.h"
#include "nautilus-canvas-container.h"
#include "nautilus-canvas-dnd.h"

/* An Icon. */

typedef struct {
	/* Object represented by this icon. */
	NautilusCanvasIconData *data;

	/* Canvas item for the icon. */
	NautilusCanvasItem *item;

	/* X/Y coordinates. */
	double x, y;

	/*
	 * In RTL mode x is RTL x position, we use saved_ltr_x for
	 * keeping track of x value before it gets converted into
	 * RTL value, this is used for saving the icon position 
	 * to the nautilus metafile. 
	 */
	 double saved_ltr_x;
	
	/* Scale factor (stretches icon). */
	double scale;

	/* Position in the view */
	int position;

	/* Whether this item is selected. */
	eel_boolean_bit is_selected : 1;

	/* Whether this item was selected before rubberbanding. */
	eel_boolean_bit was_selected_before_rubberband : 1;

	/* Whether this item is visible in the view. */
	eel_boolean_bit is_visible : 1;

	eel_boolean_bit has_lazy_position : 1;
} NautilusCanvasIcon;


/* Private NautilusCanvasContainer members. */

typedef struct {
	gboolean active;

	double start_x, start_y;

	EelCanvasItem *selection_rectangle;
	GdkDevice *device;

	guint timer_id;

	guint prev_x, prev_y;
	int last_adj_x;
	int last_adj_y;
} NautilusCanvasRubberbandInfo;

typedef enum {
	DRAG_STATE_INITIAL,
	DRAG_STATE_MOVE_OR_COPY,
	DRAG_STATE_STRETCH
} DragState;

typedef struct {
	/* Pointer position in canvas coordinates. */
	int pointer_x, pointer_y;

	/* Icon top, left, and size in canvas coordinates. */
	int icon_x, icon_y;
	guint icon_size;
} StretchState;

typedef enum {
	AXIS_NONE,
	AXIS_HORIZONTAL,
	AXIS_VERTICAL
} Axis;

enum {
	LABEL_COLOR,
	LABEL_COLOR_HIGHLIGHT,
	LABEL_COLOR_ACTIVE,
	LABEL_COLOR_PRELIGHT,
	LABEL_INFO_COLOR,
	LABEL_INFO_COLOR_HIGHLIGHT,
	LABEL_INFO_COLOR_ACTIVE,
	LAST_LABEL_COLOR
};

struct NautilusCanvasContainerDetails {
	/* List of icons. */
	GList *icons;
	GList *new_icons;
	GList *selection;
	GHashTable *icon_set;

	/* Currently focused icon for accessibility. */
	NautilusCanvasIcon *focus;
	gboolean keyboard_focus;

	/* Starting icon for keyboard rubberbanding. */
	NautilusCanvasIcon *keyboard_rubberband_start;

	/* Current icon with stretch handles, so we have only one. */
	NautilusCanvasIcon *stretch_icon;
	double stretch_initial_x, stretch_initial_y;
	guint stretch_initial_size;
	
	/* Last highlighted drop target. */
	NautilusCanvasIcon *drop_target;

	/* Rubberbanding status. */
	NautilusCanvasRubberbandInfo rubberband_info;

	/* Timeout used to make a selected icon fully visible after a short
	 * period of time. (The timeout is needed to make sure
	 * double-clicking still works.)
	 */
	guint keyboard_icon_reveal_timer_id;
	NautilusCanvasIcon *keyboard_icon_to_reveal;

	/* Used to coalesce selection changed signals in some cases */
	guint selection_changed_id;
	
	/* If a request is made to reveal an unpositioned icon we remember
	 * it and reveal it once it gets positioned (in relayout).
	 */
	NautilusCanvasIcon *pending_icon_to_reveal;

	/* Remembered information about the start of the current event. */
	guint32 button_down_time;
	
	/* Drag state. Valid only if drag_button is non-zero. */
	guint drag_button;
	NautilusCanvasIcon *drag_icon;
	int drag_x, drag_y;
	DragState drag_state;
	gboolean drag_started;
	StretchState stretch_start;

	gboolean icon_selected_on_button_down;
	gboolean double_clicked;
	NautilusCanvasIcon *double_click_icon[2]; /* Both clicks in a double click need to be on the same icon */
	guint double_click_button[2];

	NautilusCanvasIcon *range_selection_base_icon;
	
	/* Idle ID. */
	guint idle_id;

	/* Idle handler for stretch code */
	guint stretch_idle_id;

	/* Align idle id */
	guint align_idle_id;

	/* DnD info. */
	NautilusCanvasDndInfo *dnd_info;
	NautilusDragInfo *dnd_source_info;

	/* zoom level */
	int zoom_level;

	/* specific fonts used to draw labels */
	char *font;
	
	/* State used so arrow keys don't wander if icons aren't lined up.
	 */
	int arrow_key_start_x;
	int arrow_key_start_y;
	GtkDirectionType arrow_key_direction;

	/* Mode settings. */
	gboolean single_click_mode;
	gboolean auto_layout;

	/* Should the container keep icons aligned to a grid */
	gboolean keep_aligned;

        /* Set to TRUE after first allocation has been done */
	gboolean has_been_allocated;

	int size_allocation_count;
	guint size_allocation_count_id;
	
	/* Is the container fixed or resizable */
	gboolean is_fixed_size;
	
	/* Is the container for a desktop window */
	gboolean is_desktop;

	/* Ignore the visible area the next time the scroll region is recomputed */
	gboolean reset_scroll_region_trigger;
	
	/* The position we are scaling to on stretch */
	double world_x;
	double world_y;

	/* margins to follow, used for the desktop panel avoidance */
	int left_margin;
	int right_margin;
	int top_margin;
	int bottom_margin;

	/* a11y items used by canvas items */
	guint a11y_item_action_idle_handler;
	GQueue* a11y_item_action_queue;

	eel_boolean_bit in_layout_now : 1;
	eel_boolean_bit is_loading : 1;
	eel_boolean_bit needs_resort : 1;
	eel_boolean_bit selection_needs_resort : 1;

	eel_boolean_bit store_layout_timestamps : 1;
	eel_boolean_bit store_layout_timestamps_when_finishing_new_icons : 1;
	time_t layout_timestamp;
};

/* Private functions shared by mutiple files. */
NautilusCanvasIcon *nautilus_canvas_container_get_icon_by_uri             (NautilusCanvasContainer *container,
									 const char            *uri);
void          nautilus_canvas_container_move_icon                   (NautilusCanvasContainer *container,
								       NautilusCanvasIcon      *icon,
								       int                    x,
								       int                    y,
								       double                 scale,
								       gboolean               raise,
								       gboolean               snap,
								       gboolean		  update_position);
void          nautilus_canvas_container_select_list_unselect_others (NautilusCanvasContainer *container,
								     GList                 *icons);
char *        nautilus_canvas_container_get_icon_uri                (NautilusCanvasContainer *container,
								       NautilusCanvasIcon          *canvas);
char *        nautilus_canvas_container_get_icon_activation_uri     (NautilusCanvasContainer *container,
								     NautilusCanvasIcon          *canvas);
char *        nautilus_canvas_container_get_icon_drop_target_uri    (NautilusCanvasContainer *container,
								       NautilusCanvasIcon          *canvas);
void          nautilus_canvas_container_update_icon                 (NautilusCanvasContainer *container,
								       NautilusCanvasIcon          *canvas);
gboolean      nautilus_canvas_container_scroll                      (NautilusCanvasContainer *container,
								     int                    delta_x,
								     int                    delta_y);
void          nautilus_canvas_container_update_scroll_region        (NautilusCanvasContainer *container);

#endif /* NAUTILUS_CANVAS_CONTAINER_PRIVATE_H */
