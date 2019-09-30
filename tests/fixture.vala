/*
 * Copyright (C) 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by Michal Hruby <michal.hruby@canonical.com>
 *
 * This file is taken from libunity.
 */

/* A bit of magic to get proper-ish fixture support */
public interface Fixture : Object
{
  class DelegateWrapper
  {
    TestDataFunc func;
    public DelegateWrapper (owned TestDataFunc f) { func = (owned) f; }
  }

  public virtual void setup () {}
  public virtual void teardown () {}

  [CCode (has_target = false)]
  public delegate void Callback<T> (T ptr);

  private static List<DelegateWrapper> _tests;

  public static unowned TestDataFunc create<F> (Callback<void*> cb)
    requires (typeof (F).is_a (typeof (Fixture)))
  {
    TestDataFunc functor = () =>
    {
      var type = typeof (F);
      var instance = Object.new (type) as Fixture;
      instance.setup ();
      cb (instance);
      instance.teardown ();
    };
    unowned TestDataFunc copy = functor;
    _tests.append (new DelegateWrapper ((owned) functor));
    return copy;
  }
  public static unowned TestDataFunc create_static<F> (Callback<F> cb)
  {
    return create<F> ((Callback<void*>) cb);
  }
}

public static bool run_with_timeout (MainLoop ml, uint timeout_ms = 5000)
{
  bool timeout_reached = false;
  var t_id = Timeout.add (timeout_ms, () =>
  {
    timeout_reached = true;
    debug ("Timeout reached");
    ml.quit ();
    return false;
  });

  ml.run ();

  if (!timeout_reached) Source.remove (t_id);

  return !timeout_reached;
}

/* calling this will ensure that the object was destroyed, but note that
 * it needs to be called with the (owned) modifier */
public static void ensure_destruction (owned Object obj)
{
  var ml = new MainLoop ();
  bool destroyed = false;
  obj.weak_ref (() => { destroyed = true; ml.quit (); });

  obj = null;
  if (!destroyed)
  {
    // wait a bit if there were async operations
    assert (run_with_timeout (ml));
  }
}

public class ErrorHandler
{
  public ErrorHandler ()
  {
    GLib.Test.log_set_fatal_handler (handle_fatal_func);
  }

  private bool handle_fatal_func (string? log_domain, LogLevelFlags flags,
                                  string message)
  {
    return false;
  }

  private uint[] handler_ids;
  private GenericArray<string?> handler_domains;

  public void ignore_message (string? domain, LogLevelFlags flags)
  {
    handler_ids += Log.set_handler (domain, flags | LogLevelFlags.FLAG_FATAL,
                                    () => {});
    if (handler_domains == null)
    {
      handler_domains = new GenericArray<string?> ();
    }
    handler_domains.add (domain);
  }

  ~ErrorHandler ()
  {
    for(uint i = 0; i < handler_ids.length; i++)
      Log.remove_handler (handler_domains[i], handler_ids[i]);
  }
}
