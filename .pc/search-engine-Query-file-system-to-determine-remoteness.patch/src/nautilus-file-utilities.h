
/* nautilus-file-utilities.h - interface for file manipulation routines.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#ifndef NAUTILUS_FILE_UTILITIES_H
#define NAUTILUS_FILE_UTILITIES_H

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <config.h>

#include "nautilus-query.h"

#define NAUTILUS_DESKTOP_ID "org.gnome.Nautilus.desktop"

/* These functions all return something something that needs to be
 * freed with g_free, is not NULL, and is guaranteed to exist.
 */
char *   nautilus_get_user_directory                 (void);
char *   nautilus_get_desktop_directory              (void);
GFile *  nautilus_get_desktop_location               (void);
char *   nautilus_get_desktop_directory_uri          (void);
char *   nautilus_get_home_directory_uri             (void);
gboolean nautilus_is_desktop_directory_file          (GFile *dir,
						      const char *filename);
gboolean nautilus_is_root_directory                  (GFile *dir);
gboolean nautilus_is_desktop_directory               (GFile *dir);
gboolean nautilus_is_home_directory                  (GFile *dir);
gboolean nautilus_is_home_directory_file             (GFile *dir,
						      const char *filename);
gboolean nautilus_is_in_system_dir                   (GFile *location);
gboolean nautilus_is_search_directory                (GFile *dir);
gboolean nautilus_is_recent_directory                (GFile *dir);
gboolean nautilus_is_trash_directory                 (GFile *dir);
gboolean nautilus_is_other_locations_directory       (GFile *dir);
GMount * nautilus_get_mounted_mount_for_root         (GFile *location);

gboolean nautilus_should_use_templates_directory     (void);
char *   nautilus_get_templates_directory            (void);
char *   nautilus_get_templates_directory_uri        (void);

char *	 nautilus_compute_title_for_location	     (GFile *file);

gboolean nautilus_is_file_roller_installed           (void);

GIcon *  nautilus_special_directory_get_icon         (GUserDirectory directory);
GIcon *  nautilus_special_directory_get_symbolic_icon (GUserDirectory directory);

gboolean nautilus_uri_parse                          (const char  *uri,
						      char       **host,
						      guint16     *port,
						      char       **userinfo);

/* Return an allocated file location that is guranteed to be unique, but
 * tries to make the location name readable to users.
 * This isn't race-free, so don't use for security-related things
 */
GFile * nautilus_generate_unique_file_in_directory (GFile      *directory,
                                                    const char *basename);

GFile *  nautilus_find_existing_uri_in_hierarchy     (GFile *location);

char * nautilus_get_scripts_directory_path (void);

GHashTable * nautilus_trashed_files_get_original_directories (GList *files,
							      GList **unhandled_files);
void nautilus_restore_files_from_trash (GList *files,
					GtkWindow *parent_window);

typedef void (*NautilusMountGetContent) (const char **content, gpointer user_data);

char ** nautilus_get_cached_x_content_types_for_mount (GMount *mount);
void nautilus_get_x_content_types_for_mount_async (GMount *mount,
						   NautilusMountGetContent callback,
						   GCancellable *cancellable,
						   gpointer user_data);
char * get_message_for_content_type (const char *content_type);
char * get_message_for_two_content_types (const char * const *content_types);
gboolean should_handle_content_type (const char *content_type);
gboolean should_handle_content_types (const char * const *content_type);

gboolean nautilus_file_selection_equal (GList *selection_a, GList *selection_b);

/**
 * nautilus_get_common_filename_prefix:
 * @file_list: set of files (NautilusFile *)
 * @min_required_len: the minimum number of characters required in the prefix
 *
 * Returns: the common filename prefix for a set of files, or NULL if
 * there isn't a common prefix of length min_required_len
 */
char * nautilus_get_common_filename_prefix (GList *file_list,
                                            int    min_required_len);

/**
 * nautilus_get_common_filename_prefix_from_filenames:
 * @filename_list: set of file names (char *)
 * @min_required_len: the minimum number of characters required in the prefix
 *
 * Returns: the common filename prefix for a set of filenames, or NULL if
 * there isn't a common prefix of length min_required_len
 */
char * nautilus_get_common_filename_prefix_from_filenames (GList *filename_list,
                                                           int    min_required_len);

void nautilus_ensure_extension_points (void);
void nautilus_ensure_extension_builtins (void);

gboolean nautilus_file_can_rename_files (GList *files);

GList * nautilus_file_list_from_uri_list (GList *uris);

gchar * nautilus_uri_to_native_uri (const gchar *uri);

NautilusQueryRecursive location_settings_search_get_recursive (void);
NautilusQueryRecursive location_settings_search_get_recursive_for_location (GFile *location);

#endif /* NAUTILUS_FILE_UTILITIES_H */
