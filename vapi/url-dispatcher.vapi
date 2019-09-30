[CCode (cprefix="", lower_case_cprefix="", cheader_filename="liburl-dispatcher-1/url-dispatcher.h")]

namespace UrlDispatch
{
  public delegate void DispatchCallback ();

  [CCode (cname = "url_dispatch_send")]
  public static void send (string url, DispatchCallback? func = null);
}
