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

[DBus (name = "com.canonical.indicator.sound.AccountsService")]
public interface AccountsServiceSoundSettings : Object {
	// properties
	public abstract uint64 timestamp {owned get; set;}
	public abstract string player_name {owned get; set;}
	public abstract Variant player_icon {owned get; set;}
	public abstract bool running {owned get; set;}
	public abstract string state {owned get; set;}
	public abstract string title {owned get; set;}
	public abstract string artist {owned get; set;}
	public abstract string album {owned get; set;}
	public abstract string art_url {owned get; set;}
}

