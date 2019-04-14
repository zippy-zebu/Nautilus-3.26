/*
 * Nautilus
 *
 * Copyright (C) 2011 Red Hat, Inc.
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
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 */

#ifndef __NAUTILUS_DESKTOP_METADATA_H__
#define __NAUTILUS_DESKTOP_METADATA_H__

#include <glib.h>

#include <src/nautilus-file.h>

void nautilus_desktop_set_metadata_string (NautilusFile *file,
                                           const gchar *name,
                                           const gchar *key,
                                           const gchar *string);

void nautilus_desktop_set_metadata_stringv (NautilusFile *file,
                                            const char *name,
                                            const char *key,
                                            const char * const *stringv);

gboolean nautilus_desktop_update_metadata_from_keyfile (NautilusFile *file,
                                                        const gchar *name);

#endif /* __NAUTILUS_DESKTOP_METADATA_H__ */
