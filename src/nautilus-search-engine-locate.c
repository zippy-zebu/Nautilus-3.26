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

#include "nautilus-search-hit.h"
#include "nautilus-search-provider.h"
#include "nautilus-search-engine-locate.h"
#include "nautilus-search-engine-private.h"
#include "nautilus-ui-utilities.h"
#define DEBUG_FLAG NAUTILUS_DEBUG_SEARCH
#include "nautilus-debug.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define LOCATE_BINARY "locate"
#define WORD_SEPARATORS "\t\n !\"#$%&'()*+,-./:;<=>?[\\]^_`{|}~"
#define BATCH_SIZE 300
#define FILE_ATTRIBS G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," \
                     G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP "," \
                     G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," \
                     G_FILE_ATTRIBUTE_ACCESS_CAN_READ "," \
                     G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
                     G_FILE_ATTRIBUTE_TIME_ACCESS

enum
{
    LOCATE_INSTALLED                  = (1 << 0),
    LOCATE_SUPPORTS_IGNORE_CASE       = (1 << 1),
    LOCATE_SUPPORTS_BASENAME_MATCHING = (1 << 2),
    LOCATE_SUPPORTS_TRANSLITERATION   = (1 << 3),
    LOCATE_SUPPORTS_IGNORE_SEPARATORS = (1 << 4),
};

struct _NautilusSearchEngineLocate
{
    GObject parent_instance;

    NautilusQuery *query;
    GCancellable *cancellable;
    gint locate_features;
    GSubprocess *locate_process;
};

typedef struct
{
    NautilusSearchEngineLocate *locate;
    gboolean completed;
    gsize pending_ops;

    GPtrArray *date_range;
    GList *mime_types;

    gsize n_processed_paths;
    GList *hits;

    NautilusQuery *query;
    gchar *query_path;
} LocateSearchData;

static void nautilus_search_provider_init (NautilusSearchProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (NautilusSearchEngineLocate,
                         nautilus_search_engine_locate,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SEARCH_PROVIDER,
                                                nautilus_search_provider_init))

enum
{
  PROP_0,
  PROP_RUNNING,
  LAST_PROP
};

NautilusSearchEngineLocate *
nautilus_search_engine_locate_new (void)
{
    return g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_LOCATE, NULL);
}

static void
nautilus_search_engine_locate_finalize (GObject *object)
{
    NautilusSearchEngineLocate *self = NAUTILUS_SEARCH_ENGINE_LOCATE (object);

    if (self->cancellable)
        g_cancellable_cancel (self->cancellable);

    if (self->locate_process)
        g_subprocess_force_exit(self->locate_process);

    g_clear_object (&self->query);
    g_clear_object (&self->cancellable);
    g_clear_object (&self->locate_process);

    G_OBJECT_CLASS (nautilus_search_engine_locate_parent_class)->finalize (object);
}

typedef struct
{
    NautilusSearchEngineLocate *locate;
    GList *hits;
} SearchHitsData;


static void
submit_available_hits (LocateSearchData *sdata)
{
    NautilusSearchEngineLocate *self = sdata->locate;
    NautilusSearchProvider *provider = NAUTILUS_SEARCH_PROVIDER (self);

    if (sdata->hits)
    {
        nautilus_search_provider_hits_added (provider, sdata->hits);
        g_list_free_full (sdata->hits, g_object_unref);
        sdata->hits = NULL;
    }

    sdata->n_processed_paths = 0;
}

static void
submit_results (LocateSearchData *sdata)
{
    NautilusSearchEngineLocate *self = sdata->locate;

    if (!g_cancellable_is_cancelled (self->cancellable))
        submit_available_hits (sdata);

    g_clear_object (&self->cancellable);
    g_clear_object (&self->locate_process);

    g_object_notify (G_OBJECT (self), "running");
    nautilus_search_provider_finished (NAUTILUS_SEARCH_PROVIDER (self),
                                       NAUTILUS_SEARCH_PROVIDER_STATUS_NORMAL);
    g_object_unref (self);

    g_clear_object (&sdata->query);
    g_clear_pointer (&sdata->date_range, g_ptr_array_unref);
    g_list_free_full (sdata->hits, g_object_unref);
    g_list_free_full (sdata->mime_types, g_free);
    g_free (sdata->query_path);
    g_free (sdata);
}

static void
maybe_submit_available_hits (LocateSearchData *sdata)
{
    if (G_UNLIKELY (sdata->completed && sdata->pending_ops == 0))
    {
        submit_results (sdata);
    }
    else if (sdata->n_processed_paths > BATCH_SIZE)
    {
        submit_available_hits (sdata);
    }
}

static void
maybe_add_search_hit (LocateSearchData *sdata,
                      GFile            *file,
                      GFileInfo        *info)
{
    guint64 atime;
    guint64 mtime;
    gdouble rank;
    NautilusSearchHit *hit;
    const gchar *display_name;
    g_autofree gchar *uri = NULL;
    g_autoptr (GDateTime) gmodified = NULL;
    g_autoptr (GDateTime) gaccessed = NULL;

    if (g_file_info_get_is_backup (info) || g_file_info_get_is_hidden (info) ||
        !g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
        return;

    atime = g_file_info_get_attribute_uint64 (info, "time::access");
    mtime = g_file_info_get_attribute_uint64 (info, "time::modified");

    if (sdata->date_range != NULL)
    {
        NautilusQuerySearchType type;
        GDateTime *initial_date;
        GDateTime *end_date;
        guint64 file_time;

        initial_date = g_ptr_array_index (sdata->date_range, 0);
        end_date = g_ptr_array_index (sdata->date_range, 1);
        type = nautilus_query_get_search_type (sdata->query);

        file_time = (type == NAUTILUS_QUERY_SEARCH_TYPE_LAST_ACCESS) ?
                     atime : mtime;

        if (!nautilus_file_date_in_between (file_time, initial_date, end_date))
            return;
    }

    uri = g_file_get_uri (file);
    display_name = g_file_info_get_display_name (info);
    rank = nautilus_query_matches_string (sdata->query, display_name);
    gaccessed = g_date_time_new_from_unix_local (atime);
    gmodified = g_date_time_new_from_unix_local (mtime);

    hit = nautilus_search_hit_new (uri);
    nautilus_search_hit_set_modification_time (hit, gmodified);
    nautilus_search_hit_set_access_time (hit, gaccessed);
    nautilus_search_hit_set_fts_rank (hit, rank);

    sdata->hits = g_list_prepend (sdata->hits, hit);
}

static void
path_info_callback (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
    LocateSearchData *sdata = user_data;
    g_autoptr (GFile) file = G_FILE (source_object);
    g_autoptr (GFileInfo) info = NULL;
    g_autoptr (GError) error = NULL;

    info = g_file_query_info_finish (file, res, &error);
    sdata->pending_ops--;

    if (!error)
    {
        maybe_add_search_hit (sdata, file, info);
    }
    else
    {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            return;

        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
            g_debug ("Impossible to read locate file info: %s", error->message);
    }

    maybe_submit_available_hits (sdata);
}

static void
check_path (LocateSearchData *sdata,
            gchar            *path)
{
    NautilusSearchEngineLocate *self = sdata->locate;

    sdata->n_processed_paths++;

    if (g_str_has_prefix (path, sdata->query_path) &&
        g_strrstr (path, G_DIR_SEPARATOR_S ".") == NULL)
    {
        GFile *file;
        const char *file_attribs = FILE_ATTRIBS;

        if (sdata->mime_types)
            file_attribs = FILE_ATTRIBS "," \
                           G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE;

        file = g_file_new_for_path (path);

        sdata->pending_ops++;
        g_file_query_info_async (file, file_attribs, G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_DEFAULT, self->cancellable,
                                 path_info_callback, sdata);
    }
    else
    {
        maybe_submit_available_hits (sdata);
    }
}

static void
read_line_callback (GObject *input_stream,
                    GAsyncResult *res,
                    gpointer user_data)
{
    LocateSearchData *sdata = user_data;
    NautilusSearchEngineLocate *self = sdata->locate;
    GDataInputStream *data_input = G_DATA_INPUT_STREAM (input_stream);
    g_autofree gchar *path = NULL;
    g_autoptr (GError) error = NULL;
    gsize length;

    path = g_data_input_stream_read_line_finish_utf8 (data_input, res,
                                                      &length, &error);
    sdata->pending_ops--;

    if (path != NULL)
    {
        check_path (sdata, path);
        sdata->pending_ops++;
        g_data_input_stream_read_line_async (data_input, G_PRIORITY_DEFAULT,
                                             self->cancellable,
                                             read_line_callback, sdata);
    }
    else
    {
        if (error)
        {
            if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                sdata->pending_ops = 0;
            else
                g_warning ("Impossible to read locate output path: %s",
                           error->message);
        }

        sdata->completed = TRUE;
        maybe_submit_available_hits (sdata);
    }
}

static void
search_string_rewrite_with_wildcards (gchar **query)
{
    gsize query_len;
    gsize rewritten_len;
    gchar *arg;
    gchar *query_text;
    gboolean needs_leading;
    gboolean needs_trailing;

    g_return_if_fail (*query);

    query_text = *query;
    query_len = strlen (query_text);
    rewritten_len = query_len;
    needs_leading = FALSE;
    needs_trailing = FALSE;

    if (query_text[0] != '*')
    {
        needs_leading = TRUE;
        rewritten_len++;
    }

    if (query_text[query_len-1] != '*' && (!needs_leading || query_len > 0))
    {
        needs_trailing = TRUE;
        rewritten_len++;
    }

    if (!needs_leading && !needs_trailing)
        return;

    arg = g_malloc0 (rewritten_len + 1);
    *query = arg;

    if (needs_trailing)
        arg[rewritten_len-1] = '*';

    if (needs_leading)
    {
        arg[0] = '*';
        arg += 1;
    }

    memcpy (arg, query_text, query_len);

    g_free (query_text);
}

static void
search_string_replace_separators (gchar **query)
{
    g_auto (GStrv) words = NULL;
    g_autofree gchar *query_text = *query;

    /* Using generated WORD_SEPARATORS, it uses this rule:
     * gsize i;
     * gchar c;
     * gchar separators[0xff] = {0};
     * for (c = 0, i = 0; c < sizeof (separators); ++c)
     * {
     *     if (g_ascii_isspace (c) || (g_ascii_ispunct (c) && c != '@'))
     *         separators[i++] = c;
     * }
     */

    words = g_strsplit_set (query_text, WORD_SEPARATORS, -1);
    *query = g_strjoinv ("*", words);
}

static void
nautilus_search_engine_locate_start (NautilusSearchProvider *provider)
{
    NautilusSearchEngineLocate *self = NAUTILUS_SEARCH_ENGINE_LOCATE (provider);
    GInputStream *output;
    GDataInputStream *input;
    LocateSearchData *sdata;
    g_autoptr (GError) error = NULL;
    g_autoptr (GFile) query_location = NULL;
    g_autoptr (GPtrArray) args = NULL;
    char *query_text;

    g_return_if_fail (self->query);
    g_return_if_fail (self->cancellable == NULL ||
                      !g_cancellable_is_cancelled (self->cancellable));

    sdata = g_new0 (LocateSearchData, 1);
    sdata->locate = g_object_ref (self);

    query_location = nautilus_query_get_location (self->query);

    if (!(self->locate_features & LOCATE_INSTALLED) ||
        !is_recursive_search (NAUTILUS_SEARCH_ENGINE_TYPE_INDEXED,
                              nautilus_query_get_recursive (self->query),
                              query_location))
    {
        g_idle_add ((GSourceFunc) submit_results, sdata);
        return;
    }

    args = g_ptr_array_new_full (3, g_free);
    g_ptr_array_add (args, g_strdup (LOCATE_BINARY));

    if (self->locate_features & LOCATE_SUPPORTS_IGNORE_CASE)
        g_ptr_array_add (args, g_strdup ("--ignore-case"));

    if (self->locate_features & LOCATE_SUPPORTS_IGNORE_SEPARATORS)
        g_ptr_array_add (args, g_strdup ("--ignore-spaces"));

    if (self->locate_features & LOCATE_SUPPORTS_BASENAME_MATCHING)
        g_ptr_array_add (args, g_strdup ("--basename"));

    if (self->locate_features & LOCATE_SUPPORTS_TRANSLITERATION)
        g_ptr_array_add (args, g_strdup ("--transliterate"));

    query_text = nautilus_query_get_text (self->query);

    if (!(self->locate_features & LOCATE_SUPPORTS_IGNORE_SEPARATORS))
        search_string_replace_separators (&query_text);

    search_string_rewrite_with_wildcards (&query_text);

    g_ptr_array_add (args, query_text);
    g_ptr_array_add (args, NULL);

    g_autoptr (GSubprocess) proc = NULL;
    proc = g_subprocess_newv ((const gchar * const *) args->pdata,
                              G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error);

    if (error)
    {
        g_warning ("Can't start locate process: %s", error->message);
        return;
    }

    sdata->query = g_object_ref (self->query);
    sdata->date_range = nautilus_query_get_date_range (self->query);
    sdata->mime_types = nautilus_query_get_mime_types (self->query);
    sdata->query_path = g_file_get_path (query_location);

    output = g_subprocess_get_stdout_pipe (proc);
    input = g_data_input_stream_new (output);

    self->locate_process = g_steal_pointer (&proc);
    self->cancellable = g_cancellable_new ();

    g_object_notify (G_OBJECT (provider), "running");

    g_object_set_data (G_OBJECT (self->locate_process), "search-data", sdata);
    g_object_set_data_full (G_OBJECT (self->locate_process), "input-stream",
                            input, g_object_unref);

    sdata->pending_ops++;
    g_data_input_stream_read_line_async (input, G_PRIORITY_DEFAULT,
                                         self->cancellable,
                                         read_line_callback, sdata);
}

static void
nautilus_search_engine_locate_stop (NautilusSearchProvider *provider)
{
    NautilusSearchEngineLocate *self = NAUTILUS_SEARCH_ENGINE_LOCATE (provider);

    if (self->locate_process != NULL)
    {
        DEBUG ("Locate engine stop");
        g_cancellable_cancel (self->cancellable);

        g_subprocess_force_exit (self->locate_process);
    }
}

static void
nautilus_search_engine_locate_set_query (NautilusSearchProvider *provider,
                                         NautilusQuery          *query)
{
    NautilusSearchEngineLocate *self = NAUTILUS_SEARCH_ENGINE_LOCATE (provider);

    g_clear_object (&self->query);
    self->query = g_object_ref (query);
}

static gboolean
nautilus_search_engine_locate_is_running (NautilusSearchProvider *provider)
{
    NautilusSearchEngineLocate *self = NAUTILUS_SEARCH_ENGINE_LOCATE (provider);

    return self->locate_process != NULL;
}

static void
check_locate_features (NautilusSearchEngineLocate *self)
{
    gint exit_status;
    g_autofree gchar *output = NULL;
    g_autoptr (GError) error = NULL;

    g_spawn_command_line_sync (LOCATE_BINARY " -h", &output, NULL,
                               &exit_status, &error);

    if (error)
    {
        g_warning ("locate can't be properly inspected for supported features: "
                   "%s", error->message);
        return;
    }

    self->locate_features |= LOCATE_INSTALLED;

    if (g_strrstr (output, "--basename"))
        self->locate_features |= LOCATE_SUPPORTS_BASENAME_MATCHING;

    if (g_strrstr (output, "--transliterate"))
        self->locate_features |= LOCATE_SUPPORTS_TRANSLITERATION;

    if (g_strrstr (output, "--ignore-case"))
        self->locate_features |= LOCATE_SUPPORTS_IGNORE_CASE;

    if (g_strrstr (output, "--ignore-spaces"))
        self->locate_features |= LOCATE_SUPPORTS_IGNORE_SEPARATORS;
}

static void
nautilus_search_engine_locate_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
    NautilusSearchProvider *provider = NAUTILUS_SEARCH_PROVIDER (object);

    switch (prop_id)
    {
        case PROP_RUNNING:
        {
            gboolean running;
            running = nautilus_search_engine_locate_is_running (provider);
            g_value_set_boolean (value, running);
        }
        break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nautilus_search_provider_init (NautilusSearchProviderInterface *iface)
{
    iface->set_query = nautilus_search_engine_locate_set_query;
    iface->start = nautilus_search_engine_locate_start;
    iface->stop = nautilus_search_engine_locate_stop;
    iface->is_running = nautilus_search_engine_locate_is_running;
}

static void
nautilus_search_engine_locate_class_init (NautilusSearchEngineLocateClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nautilus_search_engine_locate_finalize;
    object_class->get_property = nautilus_search_engine_locate_get_property;

    g_object_class_override_property (object_class, PROP_RUNNING, "running");
}

static void
nautilus_search_engine_locate_init (NautilusSearchEngineLocate *self)
{
    check_locate_features (self);
}
