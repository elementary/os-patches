/*
 * Copyright 2014 © Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Ted Gould <ted@canonical.com>
 */

[DBus (name = "com.canonical.Unity.Greeter.Broadcast")]
public interface GreeterBroadcast : Object {
  // methods
  // unused public abstract async void RequestApplicationStart(string name, string appid) throws IOError;
  // unused public abstract async void RequestHomeShown(string name) throws IOError;
  public abstract async void RequestSoundPlayPause(string name) throws IOError;
  public abstract async void RequestSoundNext(string name) throws IOError;
  public abstract async void RequestSoundPrev(string name) throws IOError;
  // signals
  // unused public signal void StartApplication(string username, string appid);
  // unused public signal void ShowHome(string username);
  public signal void SoundPlayPause(string username);
  public signal void SoundNext(string username);
  public signal void SoundPrev(string username);
}
