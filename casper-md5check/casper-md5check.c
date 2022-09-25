/* casper-md5check - a tool to check md5sums and talk to plymouth
   (C) Canonical Ltd 2006, 2010
   Written by Tollef Fog Heen <tfheen@ubuntu.com>
   Ported to plymouth by Steve Langasek <steve.langasek@ubuntu.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA. */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <math.h>
#include <termios.h>

#include <ply-boot-client.h>
#include <ply-event-loop.h>

#include "md5.h"
#define DEBUG

#define MD5_LEN 16

#define RESULT_FILE "/run/casper-md5check.json"
#define BROKEN_FILE "  \"checksum_missmatch\": [ "
#define RESULT_PASS "  \"result\": \"pass\"\n}\n"
#define RESULT_FAIL "  \"result\": \"fail\"\n}\n"
#define RESULT_SKIP "  \"result\": \"skip\"\n}\n"

static int verbose = 1;
static int got_plymouth = 0;
static int skip_and_exit = 0;
static int spinner_theme = 0;
static ply_event_loop_t *ply_event_loop = NULL;

int set_nocanonical_tty(int fd);

void plymouth_keystruck(void *user_data, const char *keys, ply_boot_client_t *client);

void parse_cmdline(void) {
  FILE *cmdline = fopen("/proc/cmdline", "r");
  char buf[1024];
  char *bufp = buf, *tok;
  char *theme;

  if (!cmdline)
    return;

  /* /proc/cmdline ends with '\n' instead of '\0',
   * so don't rely on terminating '\0' (strlen()),
   * and use ' ' and '\n' as delimiters. */

  memset(buf, '\0', 1024);
  fread(buf, 1023, 1, cmdline);

  while ((tok = strsep(&bufp, " \n")) != NULL && bufp != NULL) {
    if (strncmp(tok, "fsck.mode=skip", sizeof("fsck.mode=skip")) == 0)
      skip_and_exit = 1;
  }

  fclose(cmdline);

  theme = realpath("/usr/share/plymouth/themes/default.plymouth", NULL);
  if (theme != NULL) {
    if (strcmp(theme, "/usr/share/plymouth/themes/bgrt/bgrt.plymouth") == 0)
      spinner_theme = 1;
    free(theme);
  }
}

void plymouth_disconnected(void *user_data, ply_boot_client_t *client) {
        printf("Disconnected from Plymouth\n");
        got_plymouth = 0;
	ply_event_loop_exit(ply_event_loop, 0);
}

void plymouth_answer(void *user_data, const char *keys,
                     ply_boot_client_t *client)
{
	ply_event_loop_exit(ply_event_loop, 0);
}

void plymouth_response(void *user_data, ply_boot_client_t *client) {
  /* No response */
}

void plymouth_failure(ply_boot_client_t *client, char *format, ...) {
  char *s;
  va_list argp;

  va_start(argp, format);
  vasprintf(&s, format, argp);
  va_end(argp);

  if (got_plymouth) {
    ply_boot_client_tell_daemon_to_display_message(client, s,
                                                   plymouth_response,
                                                   plymouth_response, NULL);
    ply_boot_client_flush(client);
  } else if (verbose)
    printf("%s\n", s);

  free(s);
}

void plymouth_pause(ply_boot_client_t *client) {
  if (got_plymouth) {
    ply_boot_client_tell_daemon_to_progress_pause (client,
                                                   plymouth_response,
                                                   plymouth_response, NULL);
    ply_boot_client_flush(client);
  }
}

void plymouth_text(ply_boot_client_t *client, char *format, ...) {
  char *s;
  va_list argp;

  va_start(argp, format);
  vasprintf(&s, format, argp);
  va_end(argp);

  if (got_plymouth) {
    ply_boot_client_tell_daemon_to_display_message(client, s,
                                                   plymouth_response,
                                                   plymouth_response, NULL);
    ply_boot_client_flush(client);
  } else if (verbose) {
    printf("%s...", s);
    fflush(stdout);
  }

  free(s);
}

void plymouth_keystrokes(ply_boot_client_t *client, char* keystrokes, char *format, ...) {
  char *s, *s1;
  va_list argp;

  va_start(argp, format);
  vasprintf(&s, format, argp);
  va_end(argp);

  if (got_plymouth) {
    if (spinner_theme)
            asprintf(&s1, "fsckd-cancel-msg:%s", s);
    else
            asprintf(&s1, "keys:%s", s);
    ply_boot_client_tell_daemon_to_display_message(client, s1,
                                                   plymouth_response,
                                                   plymouth_response, NULL);
    ply_boot_client_ask_daemon_to_watch_for_keystroke(client, keystrokes,
                                                      plymouth_keystruck,
                                                      plymouth_response, NULL);
    ply_boot_client_flush(client);
  }
  free(s);
}

void plymouth_urgent(ply_boot_client_t *client, char *format, ...) {
  char *s;
  va_list argp;

  va_start(argp, format);
  vasprintf(&s, format, argp);
  va_end(argp);

  if (got_plymouth) {
    ply_boot_client_tell_daemon_to_display_message(client, s,
                                                   plymouth_response,
                                                   plymouth_response, NULL);
    ply_boot_client_flush(client);
  } else
    printf("\n%s\n", s);

  free(s);
}


void plymouth_success(ply_boot_client_t *client, char *format, ...) {
  char *s;
  va_list argp;

  va_start(argp, format);
  vasprintf(&s, format, argp);
  va_end(argp);

  if (got_plymouth) {
    ply_boot_client_tell_daemon_to_display_message(client, s,
                                                   plymouth_response,
                                                   plymouth_response, NULL);
    ply_boot_client_flush(client);
  } else if (verbose)
    printf("%s\n", s);

  free(s);
}

void plymouth_progress(ply_boot_client_t *client, int progress, char *checkfile) {
  static int prevprogress = -1;
  char *s;

  if (progress == prevprogress)
    return;
  prevprogress = progress;

  if (got_plymouth) {
    if (checkfile) {
      if (spinner_theme)
        asprintf(&s, "fsckd:1:%d:Checking %s", progress, checkfile);
      else
        asprintf(&s, "fsck:md5sums:%d", progress);
    } else {
      if (spinner_theme)
        asprintf(&s, "fsckd:1:%d: ", progress);
      else
        asprintf(&s, "fsck:md5sums:%d", progress);
    }
    ply_boot_client_update_daemon(client, s, plymouth_response,
                                  plymouth_response, NULL);
    ply_boot_client_flush(client);
    free(s);
  } else {
    printf(".");
    fflush(stdout);
  }
}

void plymouth_keystruck(void *user_data, const char *keys,
                        ply_boot_client_t *client)
{
        if (! keys)
                return;
        skip_and_exit = 1;
}

int set_nocanonical_tty(int fd) {
  struct termios t;

  if (tcgetattr(fd, &t) == -1) {
    perror("tcgetattr");
  }
  t.c_lflag &= ~ICANON;
  t.c_cc[VMIN] = 1;
  t.c_cc[VTIME] = 0;
  return tcsetattr(fd, TCSANOW, &t);
}

int is_md5sum(const char *checksum)
{
  if (strlen(checksum) != MD5_LEN * 2)
    return 0;
  if (strspn(checksum, "0123456789abcdef") != MD5_LEN * 2)
    return 0;
  return 1;
}

int main(int argc, char **argv) {
  
  int check_fd;
  int failed = 0;
  char *result = RESULT_SKIP;
  
  FILE *md5_file;
  FILE *result_file;
  md5_state_t state;
  md5_byte_t digest[MD5_LEN];
  char hex_output[MD5_LEN * 2 + 1];
  char *checksum, *checkfile;
  ssize_t tsize, csize;
  ply_boot_client_t *client = NULL;

  tsize = 0;
  csize = 0;

  if (argc != 3) {
    fprintf(stderr,"Wrong number of arguments\n");
    fprintf(stderr,"%s <root directory> <md5sum file>\n", argv[0]);
    exit(1);
  }
  
  if (chdir(argv[1]) != 0) {
    perror("chdir");
    exit(1);
  }
  
  parse_cmdline();

  //client = ply_boot_client_new();
  if (client)
    ply_event_loop = ply_event_loop_new();
  if (ply_event_loop)
    ply_boot_client_attach_to_event_loop(client, ply_event_loop);

  if (!client || !ply_event_loop || !ply_boot_client_connect(client, plymouth_disconnected, NULL))
  {
    got_plymouth = 0;
  } else
    got_plymouth = 1;


  result_file = fopen(RESULT_FILE, "w");
  if (!result_file) {
          perror("fopen result_file");
          exit(1);
  }
  fprintf(result_file, "{\n");

  plymouth_progress(client, 0, NULL);

  if (skip_and_exit)
    goto cmdline_skip;

  plymouth_urgent(client, "Checking integrity, this may take some time (or try: fsck.mode=skip)");
  plymouth_keystrokes(client, "\x03", "Press Ctrl+C to cancel all filesystem checks in progress");
  md5_file = fopen(argv[2], "r");
  if (!md5_file) {
          perror("fopen md5_file");
          exit(1);
  }
  while (fscanf(md5_file, "%ms %m[^\n]", &checksum, &checkfile) == 2) {
    struct stat statbuf;

    if (!is_md5sum(checksum))
      continue;

    if (stat(checkfile, &statbuf) == 0) {
      tsize += statbuf.st_size;
    }

    free(checksum);
    free(checkfile);
  }

  rewind(md5_file);
  fprintf(result_file, BROKEN_FILE);
  while (fscanf(md5_file, "%ms %m[^\n]", &checksum, &checkfile) == 2) {
    char buf[BUFSIZ];
    ssize_t rsize;
    int i;

    if (!is_md5sum(checksum))
      continue;

    md5_init(&state);
    
    plymouth_text(client, "Checking %s", checkfile);
    
    check_fd = open(checkfile, O_RDONLY);
    if (check_fd < 0) {
      plymouth_failure(client, "%s: %s", checkfile, strerror(errno));
      sleep(10);
    }
    
    rsize = read(check_fd, buf, sizeof(buf));

    while (rsize > 0) {
      csize += rsize;
      plymouth_progress(client, 100*((long double)csize)/tsize, checkfile);

      md5_append(&state, (const md5_byte_t *)buf, rsize);
      rsize = read(check_fd, buf, sizeof(buf));
      if (skip_and_exit) {
              break;
      }
    }
    
    close(check_fd);
    md5_finish(&state, digest);
    
    for (i = 0; i < MD5_LEN; i++)
      sprintf(hex_output + i * 2, "%02x", digest[i]);
    
    if (strncmp(hex_output, checksum, strlen(hex_output)) == 0) {
      plymouth_success(client, "%s: OK", checkfile);
    } else {
      plymouth_failure(client, "%s: mismatch", checkfile);
      fprintf(result_file, "\n    \"%s\",", checkfile);
      failed++;
    }
    free(checksum);
    free(checkfile);
    if (skip_and_exit) {
            break;
    }
  }
  fclose(md5_file);
  fseek(result_file, -1, SEEK_CUR);
  fprintf(result_file, "\n],\n");
  if (got_plymouth) {
    if (spinner_theme)
      plymouth_text(client, "fsckd-cancel-msg:");
    else
      plymouth_text(client, "keys:");
    plymouth_progress(client, 100, NULL);
    plymouth_text(client, "");
  }
cmdline_skip:
  if (skip_and_exit) {
    result = RESULT_SKIP;
    plymouth_urgent(client, "Check skipped.");
  } else if (failed) {
    result = RESULT_FAIL;
    plymouth_urgent(client, "Check finished: errors found in %d files! You might encounter errors.", failed);
    sleep(5);
  } else {
    result = RESULT_PASS;
    plymouth_urgent(client, "Check finished: no errors found.");
  }
  fprintf(result_file, "%s", result);
  fclose(result_file);
  plymouth_urgent(client, "");
  return 0;
}
