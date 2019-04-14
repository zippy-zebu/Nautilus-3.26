/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * Copyright (C) 1999, 2000 Eazel, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Elliot Lee <sopwith@redhat.com>,
 *          Darin Adler <darin@bentspoon.com>,
 *          John Sullivan <sullivan@eazel.com>
 *
 */

/* nautilus-main.c: Implementation of the routines that drive program lifecycle and main window creation/destruction. */

#include <config.h>

#include "nautilus-application.h"
#include "nautilus-resources.h"

#include "nautilus-debug.h"
#include <eel/eel-debug.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

#include <libxml/parser.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_EXEMPI
#include <exempi/xmp.h>
#endif

int
main (int   argc,
      char *argv[])
{
    gint retval;
    NautilusApplication *application;

#if defined (HAVE_MALLOPT) && defined(M_MMAP_THRESHOLD)
    /* Nautilus uses lots and lots of small and medium size allocations,
     * and then a few large ones for the desktop background. By default
     * glibc uses a dynamic treshold for how large allocations should
     * be mmaped. Unfortunately this triggers quickly for nautilus when
     * it does the desktop background allocations, raising the limit
     * such that a lot of temporary large allocations end up on the
     * heap and are thus not returned to the OS. To fix this we set
     * a hardcoded limit. I don't know what a good value is, but 128K
     * was the old glibc static limit, lets use that.
     */
    mallopt (M_MMAP_THRESHOLD, 128 * 1024);
#endif

    if (g_getenv ("NAUTILUS_DEBUG") != NULL)
    {
        eel_make_warnings_and_criticals_stop_in_debugger ();
    }

    /* Initialize gettext support */
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    g_set_prgname ("nautilus");

#ifdef HAVE_EXEMPI
    xmp_init ();
#endif
    nautilus_register_resource ();
    /* Run the nautilus application. */
    application = nautilus_application_new ();

    /* hold indefinitely if we're asked to persist */
    if (g_getenv ("NAUTILUS_PERSIST") != NULL)
    {
        g_application_hold (G_APPLICATION (application));
    }

    retval = g_application_run (G_APPLICATION (application),
                                argc, argv);

    g_object_unref (application);

    eel_debug_shut_down ();

    return retval;
}
