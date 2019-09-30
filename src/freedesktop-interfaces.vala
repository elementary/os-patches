/*
Copyright 2010 Canonical Ltd.

Authors:
    Conor Curran <conor.curran@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

[DBus (name = "org.freedesktop.DBus")]
public interface FreeDesktopObject: Object {
  public abstract async string[] list_names() throws IOError;
  public abstract signal void name_owner_changed ( string name,
                                                   string old_owner,
                                                   string new_owner );
}

[DBus (name = "org.freedesktop.DBus.Introspectable")]
public interface FreeDesktopIntrospectable: Object {
  public abstract string Introspect() throws IOError;
}

[DBus (name = "org.freedesktop.DBus.Properties")]
public interface FreeDesktopProperties : Object{
  public signal void PropertiesChanged (string source, HashTable<string, Variant?> changed_properties,
                                        string[] invalid );
}

public errordomain XmlError {
    FILE_NOT_FOUND,
    XML_DOCUMENT_EMPTY
}

const string FREEDESKTOP_SERVICE = "org.freedesktop.DBus";
const string FREEDESKTOP_OBJECT = "/org/freedesktop/DBus";


