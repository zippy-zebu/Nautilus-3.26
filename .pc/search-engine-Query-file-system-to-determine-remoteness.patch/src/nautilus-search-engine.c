/*
 * Copyright (C) 2005 Novell, Inc.
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
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#include <config.h>
#include "nautilus-search-engine.h"
#include "nautilus-search-engine-private.h"

#include "nautilus-file.h"
#include "nautilus-search-engine-model.h"
#include "nautilus-search-provider.h"
#include <glib/gi18n.h>
#define DEBUG_FLAG NAUTILUS_DEBUG_SEARCH
#include "nautilus-debug.h"
#include "nautilus-search-engine-simple.h"
#include "nautilus-search-engine-tracker.h"

typedef struct
{
    NautilusSearchEngineTracker *tracker;
    NautilusSearchEngineSimple *simple;
    NautilusSearchEngineModel *model;

    GHashTable *uris;
    guint providers_running;
    guint providers_finished;
    guint providers_error;

    gboolean running;
    gboolean restart;
} NautilusSearchEnginePrivate;

enum
{
    PROP_0,
    PROP_RUNNING,
    LAST_PROP
};

static void nautilus_search_provider_init (NautilusSearchProviderInterface *iface);

static gboolean nautilus_search_engine_is_running (NautilusSearchProvider *provider);

G_DEFINE_TYPE_WITH_CODE (NautilusSearchEngine,
                         nautilus_search_engine,
                         G_TYPE_OBJECT,
                         G_ADD_PRIVATE (NautilusSearchEngine)
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SEARCH_PROVIDER,
                                                nautilus_search_provider_init))

static void
nautilus_search_engine_set_query (NautilusSearchProvider *provider,
                                  NautilusQuery          *query)
{
    NautilusSearchEngine *engine;
    NautilusSearchEnginePrivate *priv;

    engine = NAUTILUS_SEARCH_ENGINE (provider);
    priv = nautilus_search_engine_get_instance_private (engine);

    nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (priv->tracker), query);
    nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (priv->model), query);
    nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (priv->simple), query);
}

static void
search_engine_start_real (NautilusSearchEngine *engine)
{
    NautilusSearchEnginePrivate *priv;

    priv = nautilus_search_engine_get_instance_private (engine);

    priv->providers_running = 0;
    priv->providers_finished = 0;
    priv->providers_error = 0;

    priv->restart = FALSE;

    DEBUG ("Search engine start real");

    g_object_ref (engine);

    nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (priv->tracker));
    priv->providers_running++;

    if (nautilus_search_engine_model_get_model (priv->model))
    {
        nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (priv->model));
        priv->providers_running++;
    }

    nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (priv->simple));
    priv->providers_running++;
}

static void
nautilus_search_engine_start (NautilusSearchProvider *provider)
{
    NautilusSearchEngine *engine;
    NautilusSearchEnginePrivate *priv;
    gint num_finished;

    engine = NAUTILUS_SEARCH_ENGINE (provider);
    priv = nautilus_search_engine_get_instance_private (engine);

    DEBUG ("Search engine start");

    num_finished = priv->providers_error + priv->providers_finished;

    if (priv->running)
    {
        if (num_finished == priv->providers_running &&
            priv->restart)
        {
            search_engine_start_real (engine);
        }

        return;
    }

    priv->running = TRUE;

    g_object_notify (G_OBJECT (provider), "running");

    if (num_finished < priv->providers_running)
    {
        priv->restart = TRUE;
    }
    else
    {
        search_engine_start_real (engine);
    }
}

static void
nautilus_search_engine_stop (NautilusSearchProvider *provider)
{
    NautilusSearchEngine *engine;
    NautilusSearchEnginePrivate *priv;

    engine = NAUTILUS_SEARCH_ENGINE (provider);
    priv = nautilus_search_engine_get_instance_private (engine);

    DEBUG ("Search engine stop");

    nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (priv->tracker));
    nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (priv->model));
    nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (priv->simple));

    priv->running = FALSE;
    priv->restart = FALSE;

    g_object_notify (G_OBJECT (provider), "running");
}

static void
search_provider_hits_added (NautilusSearchProvider *provider,
                            GList                  *hits,
                            NautilusSearchEngine   *engine)
{
    NautilusSearchEnginePrivate *priv;
    GList *added = NULL;
    GList *l;

    priv = nautilus_search_engine_get_instance_private (engine);

    if (!priv->running || priv->restart)
    {
        DEBUG ("Ignoring hits-added, since engine is %s",
               !priv->running ? "not running" : "waiting to restart");
        return;
    }

    for (l = hits; l != NULL; l = l->next)
    {
        NautilusSearchHit *hit = l->data;
        int count;
        const char *uri;

        uri = nautilus_search_hit_get_uri (hit);
        count = GPOINTER_TO_INT (g_hash_table_lookup (priv->uris, uri));
        if (count == 0)
        {
            added = g_list_prepend (added, hit);
        }
        g_hash_table_replace (priv->uris, g_strdup (uri), GINT_TO_POINTER (++count));
    }
    if (added != NULL)
    {
        added = g_list_reverse (added);
        nautilus_search_provider_hits_added (NAUTILUS_SEARCH_PROVIDER (engine), added);
        g_list_free (added);
    }
}

static void
check_providers_status (NautilusSearchEngine *engine)
{
    NautilusSearchEnginePrivate *priv;
    gint num_finished;

    priv = nautilus_search_engine_get_instance_private (engine);
    num_finished = priv->providers_error + priv->providers_finished;

    if (num_finished < priv->providers_running)
    {
        return;
    }

    if (num_finished == priv->providers_error)
    {
        DEBUG ("Search engine error");
        nautilus_search_provider_error (NAUTILUS_SEARCH_PROVIDER (engine),
                                        _("Unable to complete the requested search"));
    }
    else
    {
        if (priv->restart)
        {
            DEBUG ("Search engine finished and restarting");
        }
        else
        {
            DEBUG ("Search engine finished");
        }
        nautilus_search_provider_finished (NAUTILUS_SEARCH_PROVIDER (engine),
                                           priv->restart ? NAUTILUS_SEARCH_PROVIDER_STATUS_RESTARTING :
                                           NAUTILUS_SEARCH_PROVIDER_STATUS_NORMAL);
    }

    priv->running = FALSE;
    g_object_notify (G_OBJECT (engine), "running");

    g_hash_table_remove_all (priv->uris);

    if (priv->restart)
    {
        nautilus_search_engine_start (NAUTILUS_SEARCH_PROVIDER (engine));
    }

    g_object_unref (engine);
}

static void
search_provider_error (NautilusSearchProvider *provider,
                       const char             *error_message,
                       NautilusSearchEngine   *engine)
{
    NautilusSearchEnginePrivate *priv;

    DEBUG ("Search provider error: %s", error_message);

    priv = nautilus_search_engine_get_instance_private (engine);
    priv->providers_error++;

    check_providers_status (engine);
}

static void
search_provider_finished (NautilusSearchProvider       *provider,
                          NautilusSearchProviderStatus  status,
                          NautilusSearchEngine         *engine)
{
    NautilusSearchEnginePrivate *priv;

    DEBUG ("Search provider finished");

    priv = nautilus_search_engine_get_instance_private (engine);
    priv->providers_finished++;

    check_providers_status (engine);
}

static void
connect_provider_signals (NautilusSearchEngine   *engine,
                          NautilusSearchProvider *provider)
{
    g_signal_connect (provider, "hits-added",
                      G_CALLBACK (search_provider_hits_added),
                      engine);
    g_signal_connect (provider, "finished",
                      G_CALLBACK (search_provider_finished),
                      engine);
    g_signal_connect (provider, "error",
                      G_CALLBACK (search_provider_error),
                      engine);
}

static gboolean
nautilus_search_engine_is_running (NautilusSearchProvider *provider)
{
    NautilusSearchEngine *engine;
    NautilusSearchEnginePrivate *priv;

    engine = NAUTILUS_SEARCH_ENGINE (provider);
    priv = nautilus_search_engine_get_instance_private (engine);

    return priv->running;
}

static void
nautilus_search_provider_init (NautilusSearchProviderInterface *iface)
{
    iface->set_query = nautilus_search_engine_set_query;
    iface->start = nautilus_search_engine_start;
    iface->stop = nautilus_search_engine_stop;
    iface->is_running = nautilus_search_engine_is_running;
}

static void
nautilus_search_engine_finalize (GObject *object)
{
    NautilusSearchEngine *engine;
    NautilusSearchEnginePrivate *priv;

    engine = NAUTILUS_SEARCH_ENGINE (object);
    priv = nautilus_search_engine_get_instance_private (engine);

    g_hash_table_destroy (priv->uris);

    g_clear_object (&priv->tracker);
    g_clear_object (&priv->model);
    g_clear_object (&priv->simple);

    G_OBJECT_CLASS (nautilus_search_engine_parent_class)->finalize (object);
}

static void
nautilus_search_engine_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
    NautilusSearchProvider *self = NAUTILUS_SEARCH_PROVIDER (object);

    switch (prop_id)
    {
        case PROP_RUNNING:
        {
            g_value_set_boolean (value, nautilus_search_engine_is_running (self));
        }
        break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nautilus_search_engine_class_init (NautilusSearchEngineClass *class)
{
    GObjectClass *object_class;

    object_class = (GObjectClass *) class;

    object_class->finalize = nautilus_search_engine_finalize;
    object_class->get_property = nautilus_search_engine_get_property;

    /**
     * NautilusSearchEngine::running:
     *
     * Whether the search engine is running a search.
     */
    g_object_class_override_property (object_class, PROP_RUNNING, "running");
}

static void
nautilus_search_engine_init (NautilusSearchEngine *engine)
{
    NautilusSearchEnginePrivate *priv;

    priv = nautilus_search_engine_get_instance_private (engine);
    priv->uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    priv->tracker = nautilus_search_engine_tracker_new ();
    connect_provider_signals (engine, NAUTILUS_SEARCH_PROVIDER (priv->tracker));

    priv->model = nautilus_search_engine_model_new ();
    connect_provider_signals (engine, NAUTILUS_SEARCH_PROVIDER (priv->model));

    priv->simple = nautilus_search_engine_simple_new ();
    connect_provider_signals (engine, NAUTILUS_SEARCH_PROVIDER (priv->simple));
}

NautilusSearchEngine *
nautilus_search_engine_new (void)
{
    NautilusSearchEngine *engine;

    engine = g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE, NULL);

    return engine;
}

NautilusSearchEngineModel *
nautilus_search_engine_get_model_provider (NautilusSearchEngine *engine)
{
    NautilusSearchEnginePrivate *priv;

    priv = nautilus_search_engine_get_instance_private (engine);

    return priv->model;
}

gboolean
is_recursive_search (NautilusSearchEngineType  engine_type,
                     NautilusQueryRecursive    recursive,
                     GFile                    *location)
{
    switch (recursive)
    {
        case NAUTILUS_QUERY_RECURSIVE_NEVER:
            return FALSE;

        case NAUTILUS_QUERY_RECURSIVE_ALWAYS:
            return TRUE;

        case NAUTILUS_QUERY_RECURSIVE_INDEXED_ONLY:
            return engine_type == NAUTILUS_SEARCH_ENGINE_TYPE_INDEXED;

        case NAUTILUS_QUERY_RECURSIVE_LOCAL_ONLY:
        {
            g_autoptr (NautilusFile) file = nautilus_file_get (location);
            return !nautilus_file_is_remote (file);
        }
    }

    return TRUE;
}
