/*
 *  nautilus-mime-application-chooser.c: an mime-application chooser
 *
 *  Copyright (C) 2004 Novell, Inc.
 *  Copyright (C) 2007, 2010 Red Hat, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but APPLICATIONOUT ANY WARRANTY; applicationout even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along application the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Dave Camp <dave@novell.com>
 *           Alexander Larsson <alexl@redhat.com>
 *           Cosimo Cecchi <ccecchi@redhat.com>
 */

#include <config.h>
#include "nautilus-mime-application-chooser.h"

#include "nautilus-file.h"
#include "nautilus-signaller.h"
#include <eel/eel-stock-dialogs.h>

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

struct _NautilusMimeApplicationChooser
{
    GtkBox parent_instance;

    GList *files;

    char *content_type;

    GtkWidget *label;
    GtkWidget *entry;
    GtkWidget *set_as_default_button;
    GtkWidget *open_with_widget;
    GtkWidget *add_button;
};

enum
{
    PROP_CONTENT_TYPE = 1,
    PROP_FILES,
    NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NautilusMimeApplicationChooser, nautilus_mime_application_chooser, GTK_TYPE_BOX);

static void
add_clicked_cb (GtkButton *button,
                gpointer   user_data)
{
    NautilusMimeApplicationChooser *chooser = user_data;
    GAppInfo *info;
    gchar *message;
    GError *error = NULL;

    info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (chooser->open_with_widget));

    if (info == NULL)
    {
        return;
    }

    g_app_info_set_as_last_used_for_type (info, chooser->content_type, &error);

    if (error != NULL)
    {
        message = g_strdup_printf (_("Error while adding “%s”: %s"),
                                   g_app_info_get_display_name (info), error->message);
        eel_show_error_dialog (_("Could not add application"),
                               message,
                               GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (chooser))));
        g_error_free (error);
        g_free (message);
    }
    else
    {
        gtk_app_chooser_refresh (GTK_APP_CHOOSER (chooser->open_with_widget));
        g_signal_emit_by_name (nautilus_signaller_get_current (), "mime-data-changed");
    }

    g_object_unref (info);
}

static void
remove_clicked_cb (GtkMenuItem *item,
                   gpointer     user_data)
{
    NautilusMimeApplicationChooser *chooser = user_data;
    GError *error;
    GAppInfo *info;

    info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (chooser->open_with_widget));

    if (info)
    {
        error = NULL;
        if (!g_app_info_remove_supports_type (info,
                                              chooser->content_type,
                                              &error))
        {
            eel_show_error_dialog (_("Could not forget association"),
                                   error->message,
                                   GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (chooser))));
            g_error_free (error);
        }

        gtk_app_chooser_refresh (GTK_APP_CHOOSER (chooser->open_with_widget));
        g_object_unref (info);
    }

    g_signal_emit_by_name (nautilus_signaller_get_current (), "mime-data-changed");
}

static void
populate_popup_cb (GtkAppChooserWidget *widget,
                   GtkMenu             *menu,
                   GAppInfo            *app,
                   gpointer             user_data)
{
    GtkWidget *item;
    NautilusMimeApplicationChooser *chooser = user_data;

    if (g_app_info_can_remove_supports_type (app))
    {
        item = gtk_menu_item_new_with_label (_("Forget association"));
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        gtk_widget_show (item);

        g_signal_connect (item, "activate",
                          G_CALLBACK (remove_clicked_cb), chooser);
    }
}

static void
reset_clicked_cb (GtkButton *button,
                  gpointer   user_data)
{
    NautilusMimeApplicationChooser *chooser;

    chooser = NAUTILUS_MIME_APPLICATION_CHOOSER (user_data);

    g_app_info_reset_type_associations (chooser->content_type);
    gtk_app_chooser_refresh (GTK_APP_CHOOSER (chooser->open_with_widget));

    g_signal_emit_by_name (nautilus_signaller_get_current (), "mime-data-changed");
}

static void
set_as_default_clicked_cb (GtkButton *button,
                           gpointer   user_data)
{
    NautilusMimeApplicationChooser *chooser = user_data;
    GAppInfo *info;
    GError *error = NULL;
    gchar *message = NULL;

    info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (chooser->open_with_widget));

    g_app_info_set_as_default_for_type (info, chooser->content_type,
                                        &error);

    if (error != NULL)
    {
        message = g_strdup_printf (_("Error while setting “%s” as default application: %s"),
                                   g_app_info_get_display_name (info), error->message);
        eel_show_error_dialog (_("Could not set as default"),
                               message,
                               GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (chooser))));
    }

    g_object_unref (info);

    gtk_app_chooser_refresh (GTK_APP_CHOOSER (chooser->open_with_widget));
    g_signal_emit_by_name (nautilus_signaller_get_current (), "mime-data-changed");
}

static gint
app_compare (gconstpointer a,
             gconstpointer b)
{
    return !g_app_info_equal (G_APP_INFO (a), G_APP_INFO (b));
}

static gboolean
app_info_can_add (GAppInfo    *info,
                  const gchar *content_type)
{
    GList *recommended, *fallback;
    gboolean retval = FALSE;

    recommended = g_app_info_get_recommended_for_type (content_type);
    fallback = g_app_info_get_fallback_for_type (content_type);

    if (g_list_find_custom (recommended, info, app_compare))
    {
        goto out;
    }

    if (g_list_find_custom (fallback, info, app_compare))
    {
        goto out;
    }

    retval = TRUE;

out:
    g_list_free_full (recommended, g_object_unref);
    g_list_free_full (fallback, g_object_unref);

    return retval;
}

static void
application_selected_cb (GtkAppChooserWidget *widget,
                         GAppInfo            *info,
                         gpointer             user_data)
{
    NautilusMimeApplicationChooser *chooser = user_data;
    GAppInfo *default_app;

    default_app = g_app_info_get_default_for_type (chooser->content_type, FALSE);
    if (default_app != NULL)
    {
        gtk_widget_set_sensitive (chooser->set_as_default_button,
                                  !g_app_info_equal (info, default_app));
        g_object_unref (default_app);
    }
    gtk_widget_set_sensitive (chooser->add_button,
                              app_info_can_add (info, chooser->content_type));
}

static void
nautilus_mime_application_chooser_apply_labels (NautilusMimeApplicationChooser *chooser)
{
    gchar *label, *extension = NULL, *description = NULL;
    gint num_files;
    NautilusFile *file;

    num_files = g_list_length (chooser->files);
    file = chooser->files->data;

    /* here we assume all files are of the same content type */
    if (g_content_type_is_unknown (chooser->content_type))
    {
        extension = nautilus_file_get_extension (file);

        /* Translators: the %s here is a file extension */
        description = g_strdup_printf (_("%s document"), extension);
    }
    else
    {
        description = g_content_type_get_description (chooser->content_type);
    }

    if (num_files > 1)
    {
        /* Translators; %s here is a mime-type description */
        label = g_strdup_printf (_("Open all files of type “%s” with"),
                                 description);
    }
    else
    {
        gchar *display_name;
        display_name = nautilus_file_get_display_name (file);

        /* Translators: first %s is filename, second %s is mime-type description */
        label = g_strdup_printf (_("Select an application to open “%s” and other files of type “%s”"),
                                 display_name, description);

        g_free (display_name);
    }

    gtk_label_set_markup (GTK_LABEL (chooser->label), label);

    g_free (label);
    g_free (extension);
    g_free (description);
}

static void
nautilus_mime_application_chooser_build_ui (NautilusMimeApplicationChooser *chooser)
{
    GtkWidget *box, *button;
    GAppInfo *info;

    gtk_container_set_border_width (GTK_CONTAINER (chooser), 8);
    gtk_box_set_spacing (GTK_BOX (chooser), 0);
    gtk_box_set_homogeneous (GTK_BOX (chooser), FALSE);

    chooser->label = gtk_label_new ("");
    gtk_label_set_xalign (GTK_LABEL (chooser->label), 0);
    gtk_label_set_line_wrap (GTK_LABEL (chooser->label), TRUE);
    gtk_label_set_line_wrap_mode (GTK_LABEL (chooser->label),
                                  PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars (GTK_LABEL (chooser->label), 60);
    gtk_box_pack_start (GTK_BOX (chooser), chooser->label,
                        FALSE, FALSE, 0);

    gtk_widget_show (chooser->label);

    chooser->open_with_widget = gtk_app_chooser_widget_new (chooser->content_type);
    gtk_app_chooser_widget_set_show_default (GTK_APP_CHOOSER_WIDGET (chooser->open_with_widget),
                                             TRUE);
    gtk_app_chooser_widget_set_show_fallback (GTK_APP_CHOOSER_WIDGET (chooser->open_with_widget),
                                              TRUE);
    gtk_app_chooser_widget_set_show_other (GTK_APP_CHOOSER_WIDGET (chooser->open_with_widget),
                                           TRUE);
    gtk_box_pack_start (GTK_BOX (chooser), chooser->open_with_widget,
                        TRUE, TRUE, 6);
    gtk_widget_show (chooser->open_with_widget);

    box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_set_spacing (GTK_BOX (box), 6);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (box), GTK_BUTTONBOX_END);
    gtk_box_pack_start (GTK_BOX (chooser), box, FALSE, FALSE, 6);
    gtk_widget_show (box);

    button = gtk_button_new_with_label (_("Reset"));
    g_signal_connect (button, "clicked",
                      G_CALLBACK (reset_clicked_cb),
                      chooser);
    gtk_widget_show (button);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
    gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (box), button, TRUE);

    button = gtk_button_new_with_mnemonic (_("_Add"));
    g_signal_connect (button, "clicked",
                      G_CALLBACK (add_clicked_cb),
                      chooser);
    gtk_widget_show (button);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
    chooser->add_button = button;

    button = gtk_button_new_with_label (_("Set as default"));
    g_signal_connect (button, "clicked",
                      G_CALLBACK (set_as_default_clicked_cb),
                      chooser);
    gtk_widget_show (button);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

    chooser->set_as_default_button = button;

    /* initialize sensitivity */
    info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (chooser->open_with_widget));
    if (info != NULL)
    {
        application_selected_cb (GTK_APP_CHOOSER_WIDGET (chooser->open_with_widget),
                                 info, chooser);
        g_object_unref (info);
    }

    g_signal_connect (chooser->open_with_widget,
                      "application-selected",
                      G_CALLBACK (application_selected_cb),
                      chooser);
    g_signal_connect (chooser->open_with_widget,
                      "populate-popup",
                      G_CALLBACK (populate_popup_cb),
                      chooser);
}

static void
nautilus_mime_application_chooser_init (NautilusMimeApplicationChooser *chooser)
{
    gtk_orientable_set_orientation (GTK_ORIENTABLE (chooser),
                                    GTK_ORIENTATION_VERTICAL);
}

static void
nautilus_mime_application_chooser_constructed (GObject *object)
{
    NautilusMimeApplicationChooser *chooser = NAUTILUS_MIME_APPLICATION_CHOOSER (object);

    if (G_OBJECT_CLASS (nautilus_mime_application_chooser_parent_class)->constructed != NULL)
    {
        G_OBJECT_CLASS (nautilus_mime_application_chooser_parent_class)->constructed (object);
    }

    nautilus_mime_application_chooser_build_ui (chooser);
    nautilus_mime_application_chooser_apply_labels (chooser);
}

static void
nautilus_mime_application_chooser_finalize (GObject *object)
{
    NautilusMimeApplicationChooser *chooser;

    chooser = NAUTILUS_MIME_APPLICATION_CHOOSER (object);

    g_free (chooser->content_type);
    nautilus_file_list_free (chooser->files);

    G_OBJECT_CLASS (nautilus_mime_application_chooser_parent_class)->finalize (object);
}

static void
nautilus_mime_application_chooser_get_property (GObject    *object,
                                                guint       property_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
    NautilusMimeApplicationChooser *chooser = NAUTILUS_MIME_APPLICATION_CHOOSER (object);

    switch (property_id)
    {
        case PROP_CONTENT_TYPE:
        {
            g_value_set_string (value, chooser->content_type);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
nautilus_mime_application_chooser_set_property (GObject      *object,
                                                guint         property_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
    NautilusMimeApplicationChooser *chooser = NAUTILUS_MIME_APPLICATION_CHOOSER (object);

    switch (property_id)
    {
        case PROP_CONTENT_TYPE:
        {
            chooser->content_type = g_value_dup_string (value);
        }
        break;

        case PROP_FILES:
        {
            chooser->files = nautilus_file_list_copy (g_value_get_pointer (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
nautilus_mime_application_chooser_class_init (NautilusMimeApplicationChooserClass *class)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->set_property = nautilus_mime_application_chooser_set_property;
    gobject_class->get_property = nautilus_mime_application_chooser_get_property;
    gobject_class->finalize = nautilus_mime_application_chooser_finalize;
    gobject_class->constructed = nautilus_mime_application_chooser_constructed;

    properties[PROP_CONTENT_TYPE] = g_param_spec_string ("content-type",
                                                         "Content type",
                                                         "Content type for this widget",
                                                         NULL,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS);
    properties[PROP_FILES] = g_param_spec_pointer ("files",
                                                   "Files",
                                                   "Files for this widget",
                                                   G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                                                   G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

GtkWidget *
nautilus_mime_application_chooser_new (GList      *files,
                                       const char *mime_type)
{
    GtkWidget *chooser;

    chooser = g_object_new (NAUTILUS_TYPE_MIME_APPLICATION_CHOOSER,
                            "files", files,
                            "content-type", mime_type,
                            NULL);

    return chooser;
}
