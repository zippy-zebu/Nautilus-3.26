/*
   nautilus-desktop-directory.h: Subclass of NautilusDirectory to implement
   a virtual directory consisting of the desktop directory and the desktop
   icons
 
   Copyright (C) 2003 Red Hat, Inc.
  
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
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef NAUTILUS_DESKTOP_DIRECTORY_H
#define NAUTILUS_DESKTOP_DIRECTORY_H

#include <src/nautilus-directory.h>

#define NAUTILUS_TYPE_DESKTOP_DIRECTORY nautilus_desktop_directory_get_type()
#define NAUTILUS_DESKTOP_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_DESKTOP_DIRECTORY, NautilusDesktopDirectory))
#define NAUTILUS_DESKTOP_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DESKTOP_DIRECTORY, NautilusDesktopDirectoryClass))
#define NAUTILUS_IS_DESKTOP_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_DESKTOP_DIRECTORY))
#define NAUTILUS_IS_DESKTOP_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DESKTOP_DIRECTORY))
#define NAUTILUS_DESKTOP_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_DESKTOP_DIRECTORY, NautilusDesktopDirectoryClass))

#define NAUTILUS_DESKTOP_DIRECTORY_PROVIDER_NAME "desktop-directory-provider"

typedef struct NautilusDesktopDirectoryDetails NautilusDesktopDirectoryDetails;

typedef struct {
	NautilusDirectory parent_slot;
	NautilusDesktopDirectoryDetails *details;
} NautilusDesktopDirectory;

typedef struct {
	NautilusDirectoryClass parent_slot;

} NautilusDesktopDirectoryClass;

GType   nautilus_desktop_directory_get_type             (void);
NautilusDirectory * nautilus_desktop_directory_get_real_directory   (NautilusDesktopDirectory *desktop_directory);

#endif /* NAUTILUS_DESKTOP_DIRECTORY_H */
