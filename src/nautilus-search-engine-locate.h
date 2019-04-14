/*
 * Copyright (C) 2018 Canonical Ltd
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Marco Trevisan <marco.trevisan@canonical.com>
 *
 */

#ifndef __NAUTILUS_SEARCH_ENGINE_LOCATE_H__
#define __NAUTILUS_SEARCH_ENGINE_LOCATE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SEARCH_ENGINE_LOCATE (nautilus_search_engine_locate_get_type ())

G_DECLARE_FINAL_TYPE (NautilusSearchEngineLocate, nautilus_search_engine_locate, NAUTILUS, SEARCH_ENGINE_LOCATE, GObject);

NautilusSearchEngineLocate *nautilus_search_engine_locate_new (void);

G_END_DECLS

#endif /* __NAUTILUS_SEARCH_ENGINE_LOCATE_H__ */
