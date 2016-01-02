#include <config.h>

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <libintl.h>
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <iconv.h>
#include <langinfo.h>

typedef struct {
  char *name;
  char *path;
} Directory;

Directory backwards_compat_dirs[] = {
  { "DESKTOP", "Desktop" },
  { "TEMPLATES", "Templates" },
  { "PUBLICSHARE", "Public" },
  { NULL}
};

static Directory *default_dirs = NULL;
static Directory *user_dirs = NULL;
static int user_dirs_changed = 0;

/* Config: */

static int enabled = 1;
static char *filename_encoding = NULL; /* NULL => utf8 */

/* Args */
static char *dummy_file = NULL;

static iconv_t filename_converter = (iconv_t)(-1);

static char *
concat_strings (const char *first, ...)
{
  char *res;
  const char *next;
  va_list va;
  int len;

  res = strdup (first);
  len = strlen (res) + 1;
  
  va_start (va, first);
  while ( (next = va_arg(va, const char *)) != NULL)
    {
      res = realloc (res, len + strlen (next) + 1);
      strcat (res, next);
    }
  
  va_end(va);

  return res;
}

static char *
strdup_end (const char *start, const char *end)
{
  char *res;

  res = malloc (end - start + 1);
  memcpy (res, start, end - start);
  res[end-start] = 0;
  return res;
}

static int
has_prefix (const char *str, const char *prefix)
{
  return strncmp (str, prefix, strlen (prefix)) == 0;
}

static char
ascii_toupper (char c)
{
  if (c >= 'a' && c <= 'z')
    c = (c - 'a') + 'A';
  return c;
}

static void
ascii_str_toupper (char *c)
{
  while (*c)
    {
      *c = ascii_toupper (*c);
      c++;  
    }
}

static int
is_space (char c)
{
  return c == ' ' || c == '\t';
}

static void
remove_trailing_whitespace (char *s)
{
  int len;

  len = strlen (s);
  while (len > 0 && is_space (s[len-1]))
    {
      s[len-1] = 0;
      len--;
    }
}

static int
is_regular_file (char *path)
{
  struct stat statbuf;
  if (stat (path, &statbuf) == -1)
    return 0;
  
  return S_ISREG(statbuf.st_mode);
}

static int
is_directory (char *path)
{
  struct stat statbuf;
  if (stat (path, &statbuf) == -1)
    return 0;
  
  return S_ISDIR(statbuf.st_mode);
}


static int 
mkdir_all (char *path)
{
  char *p;
  int revert;
  int result;

  path = strdup (path);

  result = 1;
  p = path;
  while (*p)
    {
      /* skip initial slashes */
      while (*p == '/')
	p++;

      while (*p && *p != '/')
	p++;

      revert = 0;
      if (*p == '/')
	{
	  *p = 0;
	  revert = 1;
	}

      if ((mkdir (path, 0755) == -1) &&
	  (errno != EEXIST))
	{
	  result = 0;
	  break;
	}
      
      if (revert)
	*p = '/';
    }
	  
  free (path);
  return result;
}

static char *
shell_unescape (char *escaped)
{
  char *unescaped;
  char *d;

  unescaped = malloc (strlen (escaped) + 1);

  d = unescaped;

  while (*escaped)
    {
      if (*escaped == '\\' && *(escaped + 1) != 0)
	escaped++;
      *d++ = *escaped++;
    }
  *d = 0;
  return unescaped;
}

static char *
shell_escape (char *unescaped)
{
  char *escaped;
  char *d;

  escaped = malloc (strlen (unescaped) * 2 + 1);

  d = escaped;

  while (*unescaped)
    {
      if (*unescaped == '$' ||
	  *unescaped == '`' ||
	  *unescaped == '\\')
	*d++ = '\\';
      *d++ = *unescaped++;
    }
  *d = 0;
  return escaped;
}

static char *
filename_from_utf8 (const char *utf8_path)
{
  size_t res, len;
  const char *in;
  char *out, *outp;
  size_t in_left, out_left, outbuf_size;
  int done;
  
  if (filename_converter == (iconv_t)(-1))
    return strdup (utf8_path);

  len = strlen (utf8_path);
  outbuf_size = len + 1;

  done = 0;
  do
    {
      in = utf8_path;
      in_left = len;
      out = malloc (outbuf_size);
      out_left = outbuf_size - 1;
      outp = out;
  
      res = iconv (filename_converter,
		   (ICONV_CONST char **)&in, &in_left,
		   &outp, &out_left);
      if (res == (size_t)(-1) &&  errno == E2BIG)
	{
	  free (out);
	  outbuf_size *= 2;
	}
      else
	done = 1;
    }
  while (!done);

  if (res == (size_t)(-1))
    {
      free (out);
      return NULL;
    }

  /* zero terminate */
  *outp = 0;
  return out;
}

static char *
get_home_dir (void)
{
  struct passwd *pw;
  static char *home_dir = NULL;

  if (home_dir != NULL)
    return home_dir;

  setpwent ();
  pw = getpwuid (getuid ());
  endpwent ();
  
  if (pw && pw->pw_dir)
    home_dir = strdup (pw->pw_dir);
  else
    home_dir = getenv ("HOME");

  return home_dir;
}

static char *
get_user_config_file (const char *filename)
{
  char *config_home, *file;
  int free_config_home;

  config_home = getenv ("XDG_CONFIG_HOME");

  free_config_home = 0;
  if (config_home == NULL || config_home[0] == 0)
    {
      config_home = concat_strings (get_home_dir (), "/.config", NULL);
      free_config_home = 1;
    }
  
  file = concat_strings (config_home, "/", filename, NULL);
  
  if (free_config_home)
    free (config_home);

  return file;
}

static void
freev (char **strs)
{
  int i;
  if (strs)
    {
      for (i = 0; strs[i] != NULL; i++)
	free (strs[i]);
      free (strs);
    }
}

static char **
parse_colon_separated_dirs (const char *dirs)
{
  int numfiles;
  char **paths;
  const char *p;

  numfiles = 0;
  paths = malloc (sizeof (char *));
  paths[0] = NULL;

  p = dirs;
  while (p != NULL && *p != 0)
    {
      int len;
      const char *path;
      char *colon;

      path = p;
      colon = strchr (path, ':');
      if (colon)
	{
	  len = colon - p;
	  p = colon + 1;
	}
      else
	{
	  len = strlen (p);
	  p = NULL;
	}

      paths = realloc (paths, sizeof (char *) * (numfiles + 2));
      paths[numfiles++] = strndup (path, len);
      paths[numfiles] = NULL;
    }

  return paths;
}

static char **
get_config_files (char *filename)
{
  int i;
  int numfiles;
  char *config_dirs;
  char *file;
  char **paths, **config_paths;

  numfiles = 0;
  paths = malloc (sizeof (char *));
  paths[0] = NULL;

  file = get_user_config_file (filename);
  if (file)
    {
      if (is_regular_file (file))
	{
	  paths = realloc (paths, sizeof (char *) * (numfiles + 2));
	  paths[numfiles++] = file;
	  paths[numfiles] = NULL;
	}
      else
	free (file);
    }

  config_dirs = getenv ("XDG_CONFIG_DIRS");
  if (config_dirs)
    config_paths = parse_colon_separated_dirs (config_dirs);
  else
    config_paths = parse_colon_separated_dirs (XDGCONFDIR);

  for (i = 0; config_paths[i] != NULL; i++)
    {
      file = concat_strings (config_paths[i], "/", filename, NULL);
      if (is_regular_file (file))
	{
	  paths = realloc (paths, sizeof (char *) * (numfiles + 2));
	  paths[numfiles++] = file;
	  paths[numfiles] = NULL;
	}
      else
	free (file);
      free (config_paths[i]);
    }
  
  free (config_paths);

  return paths;
}

static Directory *
add_directory (Directory *dirs, Directory *dir)
{
  Directory *new_dirs;
  int i;
  
  if (dirs == NULL)
    {
      new_dirs = malloc (sizeof (Directory) * 2);
      new_dirs[0] = *dir;
      new_dirs[1].name = NULL;
    }
  else
    {
      for (i = 0; dirs[i].name != NULL; i++)
	;
      new_dirs = realloc (dirs, (i + 2) * sizeof (Directory));
      new_dirs[i] = *dir;
      new_dirs[i+1].name = NULL;
    }
  return new_dirs;
}

static int
is_true (const char *str)
{
  while (is_space (*str))
    str++;
  
  if (*str == '1' ||
      has_prefix (str, "True") ||
      has_prefix (str, "true"))
    return 1;
  return 0;
}

static void
load_config (char *path)
{
  FILE *file;
  char buffer[512];
  char *p;
  int len;

  file = fopen (path, "r");
  if (file == NULL)
    return;

  while (fgets (buffer, sizeof (buffer), file))
    {
      /* Remove newline at end */
      len = strlen (buffer);
      if (len > 0 && buffer[len-1] == '\n')
	buffer[len-1] = 0;
      
      p = buffer;
      /* Skip whitespace */
      while (is_space (*p))
	p++;
      
      if (*p == '#')
	continue;

      remove_trailing_whitespace (p);
      
      if (has_prefix (p, "enabled="))
	{
	  p += strlen ("enabled=");
	  enabled = is_true (p);
	}
      if (has_prefix (p, "filename_encoding="))
	{
	  p += strlen ("filename_encoding=");

	  while (is_space (*p))
	    p++;
	  
	  ascii_str_toupper (p);
	  remove_trailing_whitespace (p);
	  if (filename_encoding)
	    free (filename_encoding);
	  
	  if (strcmp (p, "UTF8") == 0 ||
	      strcmp (p, "UTF-8") == 0)
	    filename_encoding = NULL;
	  else if (strcmp (p, "LOCALE") == 0)
	    filename_encoding = strdup (nl_langinfo (CODESET));
	  else
	    filename_encoding = strdup (p);
	}
    }

  fclose (file);
}

static void
load_all_configs (void)
{
  char **paths;
  int i;
  
  paths = get_config_files ("user-dirs.conf");

  /* Load config files in reverse */
  for (i = 0; paths[i] != NULL; i++)
    ;

  while (--i >= 0)
    load_config (paths[i]);
  
  freev (paths);
}

static void
load_default_dirs (void)
{
  FILE *file;
  char buffer[512];
  char *p;
  char *key, *key_end, *value;
  int len;
  Directory dir;
  char **paths;
  
  paths = get_config_files ("user-dirs.defaults");
  if (paths[0] == NULL)
    {
      fprintf (stderr, "No default user directories\n");
      exit (1);
    }
  
  file = fopen (paths[0], "r");
  if (file == NULL)
    {
      fprintf (stderr, "Can't open %s\n", paths[0]);
      exit (1);
    }

  while (fgets (buffer, sizeof (buffer), file))
    {
      /* Remove newline at end */
      len = strlen (buffer);
      if (len > 0 && buffer[len-1] == '\n')
	buffer[len-1] = 0;
      
      p = buffer;
      /* Skip whitespace */
      while (is_space (*p))
	p++;
      
      if (*p == '#')
	continue;

      key = p;
      while (*p && !is_space (*p) && * p != '=')
	p++;

      key_end = p;

      while (is_space (*p))
	p++;
      if (*p == '=')
	p++;
      while (is_space (*p))
	p++;
      
      value = p;

      *key_end = 0;

      if (*key == 0 || *value == 0)
	continue;
      
      dir.name = strdup (key);
      dir.path = strdup (value);
      default_dirs = add_directory (default_dirs, &dir);
    }

  fclose (file);
  freev (paths);
}

static void
load_user_dirs (void)
{
  FILE *file;
  char buffer[512];
  char *p;
  char *key, *key_end, *value, *value_end;
  int len;
  Directory dir;
  char *user_config_file;

  user_config_file = get_user_config_file ("user-dirs.dirs");
  
  file = fopen (user_config_file, "r");
  free (user_config_file);
  
  if (file == NULL)
    return;

  while (fgets (buffer, sizeof (buffer), file))
    {
      /* Remove newline at end */
      len = strlen (buffer);
      if (len > 0 && buffer[len-1] == '\n')
	buffer[len-1] = 0;
      
      p = buffer;
      /* Skip whitespace */
      while (is_space (*p))
	p++;

      /* Skip comment lines */
      if (*p == '#')
	continue;

      if (!has_prefix(p, "XDG_"))
	continue;
      p += 4;
      key = p;
         
      while (*p && !is_space (*p) && * p != '=')
	p++;

      if (*p == 0)
	continue;

      key_end = p - 4;
      if (key_end <= key ||
	  !has_prefix (key_end, "_DIR"))
	continue;

      if (*p == '=')
	p++;

      while (is_space (*p))
	p++;

      if (*p++ != '"')
	continue;
	

      if (has_prefix (p, "$HOME"))
	{
	  p += 5;
	  if (*p == '/')
	    p++;
	  else if (*p != '"' && *p != 0)
	    continue; /* Not ending after $HOME, nor followed by slash. Ignore */
	}
      else if (*p != '/')
	continue;
      value = p;

      while (*p)
	{
	  if (*p == '"')
	    break;
	  if (*p == '\\' && *(p+1) != 0)
	    p++;

	  p++;
	}
      value_end = p;

      *key_end = 0;
      *value_end = 0;

      if (*key == 0)
	continue;

      dir.name = strdup (key);
      dir.path = shell_unescape (value);
      user_dirs = add_directory (user_dirs, &dir);
    }

  fclose (file);
}

static void
save_locale (void)
{
  FILE *file;
  char *user_locale_file;
  char *locale, *dot;

  user_locale_file = get_user_config_file ("user-dirs.locale");
  file = fopen (user_locale_file, "w");
  free (user_locale_file);
  
  if (file == NULL)
    {
      fprintf (stderr, "Can't save user-dirs.locale\n");
      return;
    }

  locale = strdup (setlocale (LC_MESSAGES, NULL));
  /* Skip encoding part */
  dot = strchr (locale, '.');
  if (dot)
    *dot = 0;
  fprintf (file, "%s", locale);
  free (locale);
  fclose (file);
}

static int
save_user_dirs (void)
{
  FILE *file;
  char *user_config_file;
  char *tmp_file;
  int i;
  char *escaped;
  int tmp_fd;
  int res;
  char *dir, *slash;
  struct stat stat_buf;

  res = 1;

  tmp_file = NULL;
  if (dummy_file)
    user_config_file = strdup (dummy_file);
  else
    user_config_file = get_user_config_file ("user-dirs.dirs");

  dir = strdup (user_config_file);
  slash = strrchr (dir, '/');
  if (slash)
    *slash = 0;
  
  if (stat (dir, &stat_buf) == -1 && errno == ENOENT)
    {
      if (mkdir (dir, 0700) == -1)
	{
	  free (dir);
	  fprintf (stderr, "Can't save user-dirs.dirs, failed to create directory\n");
	  res = 0;
	  goto out;
	}
    }
  free (dir);
  
  tmp_file = malloc (strlen (user_config_file) + 6 + 1);
  strcpy (tmp_file, user_config_file);
  strcat (tmp_file, "XXXXXX");
  
  tmp_fd = mkstemp (tmp_file);
  if (tmp_fd == -1)
    {
      fprintf (stderr, "Can't save user-dirs.dirs\n");
      res = 0;
      goto out;
    }
  
  file = fdopen (tmp_fd, "w");
  if (file == NULL)
    {
      unlink (tmp_file);
      fprintf (stderr, "Can't save user-dirs.dirs\n");
      res = 0;
      goto out;
    }

  fprintf (file, "# This file is written by xdg-user-dirs-update\n");
  fprintf (file, "# If you want to change or add directories, just edit the line you're\n");
  fprintf (file, "# interested in. All local changes will be retained on the next run\n");
  fprintf (file, "# Format is XDG_xxx_DIR=\"$HOME/yyy\", where yyy is a shell-escaped\n");
  fprintf (file, "# homedir-relative path, or XDG_xxx_DIR=\"/yyy\", where /yyy is an\n");
  fprintf (file, "# absolute path. No other format is supported.\n");
  fprintf (file, "# \n");

  if (user_dirs)
    {
      for (i = 0; user_dirs[i].name != NULL; i++)
	{
	  escaped = shell_escape (user_dirs[i].path);
	  fprintf (file, "XDG_%s_DIR=\"%s%s\"\n",
		   user_dirs[i].name,
		   (*escaped == '/')?"":"$HOME/",
		   escaped);
	  free (escaped);
	}
    }

  fclose (file);

  if (rename (tmp_file, user_config_file) == -1)
    {
      unlink (tmp_file);
      fprintf (stderr, "Can't save user-dirs.dirs\n");
      res = 0;
    }

 out:
  if (tmp_file)
    free (tmp_file);
  free (user_config_file);
  return res;
}


static char *
localize_path_name (const char *path)
{
  char *res;
  const char *element, *element_end;
  char *element_copy;
  char *translated;
  int has_slash;

  res = strdup ("");

  while (*path)
    {
      has_slash = 0;
      while (*path == '/')
	{
	  path++;
	  has_slash = 1;
	}

      element = path;
      while (*path && *path != '/')
	path++;
      element_end = path;

      element_copy = strdup_end (element, element_end);
      translated = gettext (element_copy);

      res = realloc (res, strlen (res) + 1 + strlen (translated) + 1);
      if (has_slash)
	strcat (res, "/");
      strcat (res, translated);
      
      free (element_copy);
    }
  
  return res;
}

static Directory *
lookup_backwards_compat (Directory *dir)
{
  int i;
  for (i = 0; backwards_compat_dirs[i].name != NULL; i++)
    {
      if (strcmp (dir->name, backwards_compat_dirs[i].name) == 0)
	return &backwards_compat_dirs[i];
    }
  return NULL;
}

static Directory *
find_dir (Directory *dirs, const char *name)
{
  int i;

  if (dirs == NULL)
    return NULL;
  
  for (i = 0; dirs[i].name != NULL; i++)
    {
      if (strcmp (dirs[i].name, name) == 0)
	return &dirs[i];
    }
  return NULL;
}

static void
create_dirs (int force)
{
  int i;
  Directory dir;
  Directory *user_dir, *default_dir, *compat_dir;
  char *path_name, *relative_path_name, *translated_name;

  if (default_dirs == NULL)
    return;
  
  for (i = 0; default_dirs[i].name != NULL; i++)
    {
      default_dir = &default_dirs[i];
      user_dir = NULL;
      user_dir = find_dir (user_dirs, default_dir->name);

      if (user_dir && !force)
	{
	  if (user_dir->path[0] == '/')
	    path_name = strdup (user_dir->path);
	  else
	    path_name = concat_strings (get_home_dir (), "/", user_dir->path, NULL);
	  if (!is_directory (path_name))
	    {
	      fprintf (stderr, "%s was removed, reassigning %s to homedir\n",
		       path_name, user_dir->name);
	      free (user_dir->path);
	      user_dir->path = strdup ("");
	      user_dirs_changed = 1;
	    }
	  free (path_name);
	  continue;
	}

      path_name = NULL;
      relative_path_name = NULL;
      if (user_dir == NULL && !force)
	{
	  /* New default dir. Check if its an old named dir. We want to
	     reuse that if it exists. */
	  compat_dir = lookup_backwards_compat (default_dir);
	  if (compat_dir)
	    {
	      path_name = concat_strings (get_home_dir (), "/", compat_dir->path, NULL);
	      if (!is_directory (path_name))
		{
		  free (path_name);
		  path_name = NULL;
		}
	      else
		relative_path_name = strdup (compat_dir->path);
	    }
	}

      if (path_name == NULL)
	{
	  translated_name = localize_path_name (default_dir->path);
	  relative_path_name = filename_from_utf8 (translated_name);
	  if (relative_path_name == NULL)
	    relative_path_name = strdup (translated_name);
	  free (translated_name);
	  if (relative_path_name[0] == '/')
	    path_name = strdup (relative_path_name); /* default path was absolute, not homedir relative */
	  else
	    path_name = concat_strings (get_home_dir (), "/", relative_path_name, NULL);
	}
	      
      if (user_dir == NULL || strcmp (relative_path_name, user_dir->path) != 0)
	{
	  /* Don't make the directories if we're writing a dummy output file */
	  if (dummy_file == NULL &&
	      !mkdir_all (path_name))
	    {
	      fprintf (stderr, "Can't create dir %s\n", path_name);
	    }
	  else
	    {
	      user_dirs_changed = 1;
	      if (user_dir == NULL)
		{
		  dir.name = strdup (default_dir->name);
		  dir.path = strdup (relative_path_name);
		  user_dirs = add_directory (user_dirs, &dir);
		}
	      else
		{
		  /* We forced an update */
		  fprintf (stdout, "Moving %s directory from %s to %s\n",
			   default_dir->name, user_dir->path, relative_path_name);
		  free (user_dir->path);
		  user_dir->path = strdup (relative_path_name);
		}
	    }
	}
      
      free (relative_path_name);
      free (path_name);
    }
}

int
main (int argc, char *argv[])
{
  int force;
  int i;
  int was_empty;
  char *set_dir = NULL;
  char *set_value = NULL;
  char *locale_dir = NULL;
  
  setlocale (LC_ALL, "");
  
  if (is_directory (LOCALEDIR))
    locale_dir = strdup (LOCALEDIR);
  else
    {
      /* In case LOCALEDIR does not exist, e.x. xdg-user-dirs is installed in
       * a different location than the one determined at compile time, look
       * through the XDG_DATA_DIRS environment variable for alternate locations
       * of the locale files */
      char *data_dirs = getenv ("XDG_DATA_DIRS");
      if (data_dirs)
	{
	  char **data_paths;

	  data_paths = parse_colon_separated_dirs (data_dirs);
	  for (i = 0; data_paths[i] != NULL; i++)
	    {
	      if (!locale_dir)
		{
		  char *dir = NULL;
		  dir = concat_strings (data_paths[i], "/", "locale", NULL);
		  if (is_directory (dir))
		    locale_dir = dir;
		  else
		    free (dir);
		}
	      free (data_paths[i]);
	    }
	  free (data_paths);
	}
    }

  bindtextdomain (GETTEXT_PACKAGE, locale_dir);
  free (locale_dir);

  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  force = 0;
  for (i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "--help") == 0)
	{
	  printf ("Usage: xdg-user-dirs-update [--force] [--dummy-output <path>] [--set DIR path]\n");
	  exit (0);
	}
      else if (strcmp (argv[i], "--force") == 0)
	force = 1;
      else if (strcmp (argv[i], "--dummy-output") == 0 && i + 1 < argc)
	dummy_file = argv[++i];
      else if (strcmp (argv[i], "--set") == 0 && i + 2 < argc)
	{
	  set_dir = argv[++i];
	  set_value = argv[++i];

	  if (*set_value != '/')
	    {
	      printf ("directory value must be absolute path (was %s)\n", set_value);
	      exit (1);
	    }
	}
      else
	{
	  printf ("Invalid argument %s\n", argv[i]);
	  exit (1);
	}
    }
  
  load_all_configs ();
  if (filename_encoding)
    {
      filename_converter = iconv_open (filename_encoding, "UTF-8");
      if (filename_converter == (iconv_t)(-1))
	{
	  fprintf (stderr, "Can't convert from UTF-8 to %s\n", filename_encoding);
	  return 1;
	}
    }

  if (set_dir != NULL)
    {
      Directory *dir;
      char *path, *home;
      /* Set a key */

      load_user_dirs ();

      home = get_home_dir ();

      path = set_value;
      if (has_prefix (path, home))
	{
	  path += strlen (home);
	  while (*path == '/')
	    path++;
	}
      
      dir = find_dir (user_dirs, set_dir);
      if (dir != NULL)
	{
	  free (dir->path);
	  dir->path = strdup (path);
	}
      else
	{
	  Directory new_dir;

	  new_dir.name = strdup (set_dir);
	  new_dir.path = strdup (path);
	  
	  user_dirs = add_directory (user_dirs, &new_dir);
	}
      if (!save_user_dirs ())
	return 1;
    }
  else
    {
      
      /* default: update */
      if (!enabled)
	return 0;
      
      load_default_dirs ();
      load_user_dirs ();
      
      was_empty = (user_dirs == NULL) || (user_dirs->name == NULL);
      
      create_dirs (force);
      
      if (user_dirs_changed)
	{
	  if (!save_user_dirs ())
	    return 1;
	  
	  if ((force || was_empty) && dummy_file == NULL)
	    save_locale ();
	}

    }
  return 0;
}
