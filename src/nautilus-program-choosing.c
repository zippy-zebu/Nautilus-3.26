/* nautilus-program-choosing.c - functions for selecting and activating
 *                                programs for opening/viewing particular files.
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Author: John Sullivan <sullivan@eazel.com>
 */

#include <config.h>
#include "nautilus-program-choosing.h"

#include "nautilus-global-preferences.h"
#include "nautilus-icon-info.h"
#include "nautilus-recent.h"
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <stdlib.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

void
nautilus_launch_application_for_mount (GAppInfo  *app_info,
                                       GMount    *mount,
                                       GtkWindow *parent_window)
{
    GFile *root;
    NautilusFile *file;
    GList *files;

    root = g_mount_get_root (mount);
    file = nautilus_file_get (root);
    g_object_unref (root);

    files = g_list_append (NULL, file);
    nautilus_launch_application (app_info,
                                 files,
                                 parent_window);

    g_list_free_full (files, (GDestroyNotify) nautilus_file_unref);
}

/**
 * nautilus_launch_application:
 *
 * Fork off a process to launch an application with a given file as a
 * parameter. Provide a parent window for error dialogs.
 *
 * @application: The application to be launched.
 * @uris: The files whose locations should be passed as a parameter to the application.
 * @parent_window: A window to use as the parent for any error dialogs.
 */
void
nautilus_launch_application (GAppInfo  *application,
                             GList     *files,
                             GtkWindow *parent_window)
{
    GList *uris, *l;

    uris = NULL;
    for (l = files; l != NULL; l = l->next)
    {
        uris = g_list_prepend (uris, nautilus_file_get_activation_uri (l->data));
    }
    uris = g_list_reverse (uris);
    nautilus_launch_application_by_uri (application, uris,
                                        parent_window);
    g_list_free_full (uris, g_free);
}

static GdkAppLaunchContext *
get_launch_context (GtkWindow *parent_window)
{
    GdkDisplay *display;
    GdkAppLaunchContext *launch_context;

    if (parent_window != NULL)
    {
        display = gtk_widget_get_display (GTK_WIDGET (parent_window));
    }
    else
    {
        display = gdk_display_get_default ();
    }

    launch_context = gdk_display_get_app_launch_context (display);

    if (parent_window != NULL)
    {
        gdk_app_launch_context_set_screen (launch_context,
                                           gtk_window_get_screen (parent_window));
    }

    return launch_context;
}

void
nautilus_launch_application_by_uri (GAppInfo  *application,
                                    GList     *uris,
                                    GtkWindow *parent_window)
{
    char *uri;
    GList *locations, *l;
    GFile *location;
    NautilusFile *file;
    gboolean result;
    GError *error;
    g_autoptr (GdkAppLaunchContext) launch_context = NULL;
    NautilusIconInfo *icon;
    int count, total;

    g_assert (uris != NULL);

    /* count the number of uris with local paths */
    count = 0;
    total = g_list_length (uris);
    locations = NULL;
    for (l = uris; l != NULL; l = l->next)
    {
        uri = l->data;

        location = g_file_new_for_uri (uri);
        if (g_file_is_native (location))
        {
            count++;
        }
        locations = g_list_prepend (locations, location);
    }
    locations = g_list_reverse (locations);

    launch_context = get_launch_context (parent_window);

    file = nautilus_file_get_by_uri (uris->data);
    icon = nautilus_file_get_icon (file,
                                   48, gtk_widget_get_scale_factor (GTK_WIDGET (parent_window)),
                                   0);
    nautilus_file_unref (file);
    if (icon)
    {
        gdk_app_launch_context_set_icon_name (launch_context,
                                              nautilus_icon_info_get_used_name (icon));
        g_object_unref (icon);
    }

    error = NULL;

    if (count == total)
    {
        /* All files are local, so we can use g_app_info_launch () with
         * the file list we constructed before.
         */
        result = g_app_info_launch (application,
                                    locations,
                                    G_APP_LAUNCH_CONTEXT (launch_context),
                                    &error);
    }
    else
    {
        /* Some files are non local, better use g_app_info_launch_uris ().
         */
        result = g_app_info_launch_uris (application,
                                         uris,
                                         G_APP_LAUNCH_CONTEXT (launch_context),
                                         &error);
    }

    if (result)
    {
        for (l = uris; l != NULL; l = l->next)
        {
            file = nautilus_file_get_by_uri (l->data);
            nautilus_recent_add_file (file, application);
            nautilus_file_unref (file);
        }
    }

    g_list_free_full (locations, g_object_unref);
}

static void
launch_application_from_command_internal (const gchar *full_command,
                                          GdkScreen   *screen,
                                          gboolean     use_terminal)
{
    GAppInfo *app;
    GdkAppLaunchContext *ctx;
    GdkDisplay *display;

    if (use_terminal)
    {
        eel_gnome_open_terminal_on_screen (full_command, screen);
    }
    else
    {
        app = g_app_info_create_from_commandline (full_command, NULL, 0, NULL);

        if (app != NULL)
        {
            display = gdk_screen_get_display (screen);
            ctx = gdk_display_get_app_launch_context (display);
            gdk_app_launch_context_set_screen (ctx, screen);

            g_app_info_launch (app, NULL, G_APP_LAUNCH_CONTEXT (ctx), NULL);

            g_object_unref (app);
            g_object_unref (ctx);
        }
    }
}

/**
 * nautilus_launch_application_from_command:
 *
 * Fork off a process to launch an application with a given uri as
 * a parameter.
 *
 * @command_string: The application to be launched, with any desired
 * command-line options.
 * @...: Passed as parameters to the application after quoting each of them.
 */
void
nautilus_launch_application_from_command (GdkScreen  *screen,
                                          const char *command_string,
                                          gboolean    use_terminal,
                                          ...)
{
    char *full_command, *tmp;
    char *quoted_parameter;
    char *parameter;
    va_list ap;

    full_command = g_strdup (command_string);

    va_start (ap, use_terminal);

    while ((parameter = va_arg (ap, char *)) != NULL)
    {
        quoted_parameter = g_shell_quote (parameter);
        tmp = g_strconcat (full_command, " ", quoted_parameter, NULL);
        g_free (quoted_parameter);

        g_free (full_command);
        full_command = tmp;
    }

    va_end (ap);

    launch_application_from_command_internal (full_command, screen, use_terminal);

    g_free (full_command);
}

/**
 * nautilus_launch_application_from_command:
 *
 * Fork off a process to launch an application with a given uri as
 * a parameter.
 *
 * @command_string: The application to be launched, with any desired
 * command-line options.
 * @parameters: Passed as parameters to the application after quoting each of them.
 */
void
nautilus_launch_application_from_command_array (GdkScreen          *screen,
                                                const char         *command_string,
                                                gboolean            use_terminal,
                                                const char * const *parameters)
{
    char *full_command, *tmp;
    char *quoted_parameter;
    const char * const *p;

    full_command = g_strdup (command_string);

    if (parameters != NULL)
    {
        for (p = parameters; *p != NULL; p++)
        {
            quoted_parameter = g_shell_quote (*p);
            tmp = g_strconcat (full_command, " ", quoted_parameter, NULL);
            g_free (quoted_parameter);

            g_free (full_command);
            full_command = tmp;
        }
    }

    launch_application_from_command_internal (full_command, screen, use_terminal);

    g_free (full_command);
}

void
nautilus_launch_desktop_file (GdkScreen   *screen,
                              const char  *desktop_file_uri,
                              const GList *parameter_uris,
                              GtkWindow   *parent_window)
{
    GError *error;
    char *message, *desktop_file_path;
    const GList *p;
    GList *files;
    int total, count;
    GFile *file, *desktop_file;
    GDesktopAppInfo *app_info;
    GdkAppLaunchContext *context;

    /* Don't allow command execution from remote locations
     * to partially mitigate the security
     * risk of executing arbitrary commands.
     */
    desktop_file = g_file_new_for_uri (desktop_file_uri);
    desktop_file_path = g_file_get_path (desktop_file);
    if (!g_file_is_native (desktop_file))
    {
        g_free (desktop_file_path);
        g_object_unref (desktop_file);
        eel_show_error_dialog
            (_("Sorry, but you cannot execute commands from "
               "a remote site."),
            _("This is disabled due to security considerations."),
            parent_window);

        return;
    }
    g_object_unref (desktop_file);

    app_info = g_desktop_app_info_new_from_filename (desktop_file_path);
    g_free (desktop_file_path);
    if (app_info == NULL)
    {
        eel_show_error_dialog
            (_("There was an error launching the application."),
            NULL,
            parent_window);
        return;
    }

    /* count the number of uris with local paths */
    count = 0;
    total = g_list_length ((GList *) parameter_uris);
    files = NULL;
    for (p = parameter_uris; p != NULL; p = p->next)
    {
        file = g_file_new_for_uri ((const char *) p->data);
        if (g_file_is_native (file))
        {
            count++;
        }
        files = g_list_prepend (files, file);
    }

    /* check if this app only supports local files */
    if (g_app_info_supports_files (G_APP_INFO (app_info)) &&
        !g_app_info_supports_uris (G_APP_INFO (app_info)) &&
        parameter_uris != NULL)
    {
        if (count == 0)
        {
            /* all files are non-local */
            eel_show_error_dialog
                (_("This drop target only supports local files."),
                _("To open non-local files copy them to a local folder and then"
                  " drop them again."),
                parent_window);

            g_list_free_full (files, g_object_unref);
            g_object_unref (app_info);
            return;
        }
        else if (count != total)
        {
            /* some files are non-local */
            eel_show_warning_dialog
                (_("This drop target only supports local files."),
                _("To open non-local files copy them to a local folder and then"
                  " drop them again. The local files you dropped have already been opened."),
                parent_window);
        }
    }

    error = NULL;
    context = gdk_display_get_app_launch_context (gtk_widget_get_display (GTK_WIDGET (parent_window)));
    /* TODO: Ideally we should accept a timestamp here instead of using GDK_CURRENT_TIME */
    gdk_app_launch_context_set_timestamp (context, GDK_CURRENT_TIME);
    gdk_app_launch_context_set_screen (context,
                                       gtk_window_get_screen (parent_window));
    if (count == total)
    {
        /* All files are local, so we can use g_app_info_launch () with
         * the file list we constructed before.
         */
        g_app_info_launch (G_APP_INFO (app_info),
                           files,
                           G_APP_LAUNCH_CONTEXT (context),
                           &error);
    }
    else
    {
        /* Some files are non local, better use g_app_info_launch_uris ().
         */
        g_app_info_launch_uris (G_APP_INFO (app_info),
                                (GList *) parameter_uris,
                                G_APP_LAUNCH_CONTEXT (context),
                                &error);
    }
    if (error != NULL)
    {
        message = g_strconcat (_("Details: "), error->message, NULL);
        eel_show_error_dialog
            (_("There was an error launching the application."),
            message,
            parent_window);

        g_error_free (error);
        g_free (message);
    }

    g_list_free_full (files, g_object_unref);
    g_object_unref (context);
    g_object_unref (app_info);
}

/* HAX
 *
 * TODO: remove everything below once it’s doable from GTK+.
 *
 * Context: https://bugzilla.gnome.org/show_bug.cgi?id=781132 and
 *          https://bugzilla.gnome.org/show_bug.cgi?id=779312
 *
 * In a sandboxed environment, this is needed to able to get the actual
 * result of the operation, since gtk_show_uri_on_window () neither blocks
 * nor returns a useful value.
 */

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

typedef void (*GtkWindowHandleExported) (GtkWindow               *window,
                                         const char              *handle,
                                         gpointer                 user_data);

#ifdef GDK_WINDOWING_WAYLAND
typedef struct
{
    GtkWindow *window;
    GtkWindowHandleExported callback;
    gpointer user_data;
} WaylandWindowHandleExportedData;

static void
wayland_window_handle_exported (GdkWindow  *window,
                                const char *wayland_handle_str,
                                gpointer    user_data)
{
    WaylandWindowHandleExportedData *data = user_data;
    char *handle_str;

    handle_str = g_strdup_printf ("wayland:%s", wayland_handle_str);
    data->callback (data->window, handle_str, data->user_data);
    g_free (handle_str);
}
#endif

static gboolean
window_export_handle (GtkWindow               *window,
                      GtkWindowHandleExported  callback,
                      gpointer                 user_data)
{

#ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
        GdkWindow *gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
        char *handle_str;
        guint32 xid = (guint32) gdk_x11_window_get_xid (gdk_window);

        handle_str = g_strdup_printf ("x11:%x", xid);
        callback (window, handle_str, user_data);

        return TRUE;
    }
#endif
#ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
        GdkWindow *gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
        WaylandWindowHandleExportedData *data;

        data = g_new0 (WaylandWindowHandleExportedData, 1);
        data->window = window;
        data->callback = callback;
        data->user_data = user_data;

        if (!gdk_wayland_window_export_handle (gdk_window,
                                               wayland_window_handle_exported,
                                               data,
                                               g_free))
        {
            g_free (data);
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }
#endif

    g_warning ("Couldn't export handle, unsupported windowing system");

    return FALSE;
}

void
gtk_window_unexport_handle (GtkWindow *window)
{
#ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
        GdkWindow *gdk_window = gtk_widget_get_window (GTK_WIDGET (window));

        gdk_wayland_window_unexport_handle (gdk_window);
    }
#endif
}

static void
on_launch_default_for_uri (GObject      *source,
                           GAsyncResult *result,
                           gpointer      data)
{
    GTask *task;
    GtkWindow *window;
    gboolean success;
    GError *error = NULL;

    task = data;
    window = g_task_get_source_object (task);

    success = g_app_info_launch_default_for_uri_finish (result, &error);

    if (window)
    {
        gtk_window_unexport_handle (window);
    }

    if (success)
    {
        g_task_return_boolean (task, success);
    }
    else
    {
        g_task_return_error (task, error);
    }

    /* Reffed in the call to window_export_handle */
    g_object_unref (task);
}

static void
on_window_handle_export (GtkWindow  *window,
                         const char *handle_str,
                         gpointer    user_data)
{
    GTask *task = user_data;
    GAppLaunchContext *context = g_task_get_task_data (task);
    const char *uri;

    uri = g_object_get_data (G_OBJECT (context), "uri");

    g_app_launch_context_setenv (context, "PARENT_WINDOW_ID", handle_str);

    g_app_info_launch_default_for_uri_async (uri,
                                             context,
                                             g_task_get_cancellable (task),
                                             on_launch_default_for_uri,
                                             task);
}

static void
launch_default_for_uri_thread_func (GTask        *task,
                                    gpointer      source_object,
                                    gpointer      task_data,
                                    GCancellable *cancellable)
{
    GAppLaunchContext *launch_context;
    const char *uri;
    gboolean success;
    GError *error = NULL;

    launch_context = task_data;
    uri = g_object_get_data (G_OBJECT (launch_context), "uri");
    success = g_app_info_launch_default_for_uri (uri, launch_context, &error);

    if (success)
    {
        g_task_return_boolean (task, success);
    }
    else
    {
        g_task_return_error (task, error);
    }
}

void
nautilus_launch_default_for_uri_async  (const char          *uri,
                                        GtkWindow           *parent_window,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             callback_data)
{
    g_autoptr (GdkAppLaunchContext) launch_context = NULL;
    g_autoptr (GTask) task = NULL;

    g_return_if_fail (uri != NULL);

    launch_context = get_launch_context (parent_window);
    task = g_task_new (parent_window, cancellable, callback, callback_data);

    gdk_app_launch_context_set_timestamp (launch_context, GDK_CURRENT_TIME);

    g_object_set_data_full (G_OBJECT (launch_context),
                            "uri", g_strdup (uri), g_free);
    g_task_set_task_data (task,
                          g_object_ref (launch_context), g_object_unref);

    if (parent_window != NULL)
    {
        gboolean handle_exported;

        handle_exported = window_export_handle (parent_window,
                                                on_window_handle_export,
                                                g_object_ref (task));

        if (handle_exported)
        {
            /* Launching will now be handled from the callback */
            return;
        }
    }

    g_task_run_in_thread (task, launch_default_for_uri_thread_func);
}

gboolean
nautilus_launch_default_for_uri_finish (GAsyncResult  *result,
                                        GError       **error)
{
    g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

/* END OF HAX */
