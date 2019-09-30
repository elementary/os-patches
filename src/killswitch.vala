/*
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 */

/**
 * Monitors whether or not bluetooth is blocked,
 * either by software (e.g., a session configuration setting)
 * or by hardware (e.g., user disabled it via a physical switch on her laptop).
 *
 * KillSwitchBluetooth uses this as the impl for its Bluetooth.blocked property
 */
public interface KillSwitch: Object
{
  public abstract bool is_valid ();

  public abstract bool blocked { get; protected set; }

  /* Try to block/unblock bluetooth.
   * This can fail if the requested state is overruled by a hardware block. */
  public abstract void try_set_blocked (bool blocked);
}

/**
 * KillSwitch impementation for Linux using /dev/rfkill
 */
public class RfKillSwitch: KillSwitch, Object
{
  public bool blocked { get; protected set; default = false; }

  public void try_set_blocked (bool blocked)
  {
    return_if_fail (this.blocked != blocked);

    // Try to soft-block all the bluetooth devices
    var event = Linux.RfKillEvent() {
      op    = Linux.RfKillOp.CHANGE_ALL,
      type  = Linux.RfKillType.BLUETOOTH,
      soft  = (uint8)blocked
    };

    /* Write this request to rfkill.
     * Don't update this object's "blocked" property here --
     * We'll get the update when on_channel_event() reads it below */
    var bwritten = Posix.write (fd, &event, sizeof(Linux.RfKillEvent));
    if (bwritten == -1)
      warning (@"Could not write rfkill event: $(strerror(errno))");
  }

  /* represents an entry that we've read from the rfkill file */
  private class Entry
  {
    public uint32 idx;
    public Linux.RfKillType type;
    public bool soft;
    public bool hard;
  }

  private HashTable<uint32,Entry> entries;
  private int fd = -1;
  private IOChannel channel;
  private uint watch;

  protected override void dispose ()
  {
    if (watch != 0)
      {
        Source.remove (watch);
        watch = 0;
      }

    if (fd != -1)
      {
        Posix.close (fd);
        fd = -1;
      }

    base.dispose ();
  }

  public RfKillSwitch ()
  {
    entries = new HashTable<uint32,Entry>(direct_hash, direct_equal);

    var path = "/dev/rfkill";
    fd = Posix.open (path, Posix.O_RDWR | Posix.O_NONBLOCK );
    if (fd == -1)
      {
        warning (@"Can't open $path for use as a killswitch backend: $(strerror(errno))");
      }
    else
      {
        // read everything that's already there, then watch for more
        while (read_event());
        channel = new IOChannel.unix_new (fd);
        watch = channel.add_watch (IOCondition.IN, on_channel_event);
      }
  }

  public bool is_valid ()
  {
    return fd != -1;
  }

  private bool on_channel_event (IOChannel source, IOCondition condition)
  {
    read_event ();
    return true;
  }

  private bool read_event ()
  {
    assert (fd != -1);

    var event = Linux.RfKillEvent();
    var n = sizeof (Linux.RfKillEvent);
    var bytesread = Posix.read (fd, &event, n);
   
    if (bytesread == n)
      {
        process_event (event);
        return true;
      }

    return false;
  }

  private void process_event (Linux.RfKillEvent event)
  {
    // we only want things that affect bluetooth
    if ((event.type != Linux.RfKillType.ALL) &&
        (event.type != Linux.RfKillType.BLUETOOTH))
      return;

    switch (event.op)
      {
        case Linux.RfKillOp.CHANGE:
        case Linux.RfKillOp.ADD:
          Entry entry = new Entry ();
          entry.idx = event.idx;
          entry.type = event.type;
          entry.soft = event.soft != 0;
          entry.hard = event.hard != 0;
          entries.insert (entry.idx, entry);
          break;

        case Linux.RfKillOp.DEL:
          entries.remove (event.idx);
          break;
      }

    /* update our blocked property.
       it should be true if any bluetooth entry is hard- or soft-blocked */
    var b = false;
    foreach (var entry in entries.get_values ())
      if ((b = (entry.soft || entry.hard)))
        break;
    blocked = b;
  }
}
