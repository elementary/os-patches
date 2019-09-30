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
#include <sys/reboot.h>
#include <linux/reboot.h>
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

static int verbose = 1;
static int got_plymouth = 0;
static ply_event_loop_t *ply_event_loop = NULL;

int set_nocanonical_tty(int fd);

void parse_cmdline(void) {
  FILE *cmdline = fopen("/proc/cmdline", "r");
  char buf[1024];
  size_t len;
  char *bufp = buf, *tok;

  if (!cmdline)
    return;

  len = fread(buf, 1023, 1, cmdline);
  buf[len] = '\0';

  while ((tok = strsep(&bufp, " ")) != NULL) {
    if (strncmp(tok, "quiet", 5) == 0)
      verbose = 0;
  }

  fclose(cmdline);
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
    ply_event_loop_process_pending_events(ply_event_loop);
  } else if (verbose)
    printf("%s\n", s);

  free(s);
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
    ply_event_loop_process_pending_events(ply_event_loop);
  } else if (verbose) {
    printf("%s...", s);
    fflush(stdout);
  }

  free(s);
}

void plymouth_prompt(ply_boot_client_t *client, char *format, ...) {
  char *s, *s1;
  va_list argp;

  va_start(argp, format);
  vasprintf(&s, format, argp);
  va_end(argp);

  if (got_plymouth) {
    asprintf(&s1, "keys:%s", s);
    ply_boot_client_tell_daemon_to_display_message(client, s1,
                                                   plymouth_response,
                                                   plymouth_response, NULL);

    ply_boot_client_ask_daemon_to_watch_for_keystroke(client, NULL,
                                                      plymouth_answer,
                                                      (ply_boot_client_response_handler_t)plymouth_answer, NULL);
    ply_event_loop_run(ply_event_loop);
    ply_boot_client_attach_to_event_loop(client, ply_event_loop);
    ply_boot_client_tell_daemon_to_quit(client, 1, plymouth_response,
                                        plymouth_response, NULL);
    ply_event_loop_run(ply_event_loop);
  } else {
    printf("%s\n", s);
    set_nocanonical_tty(0);
    getchar();
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
    ply_event_loop_process_pending_events(ply_event_loop);
  } else
    printf("%s\n", s);

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
    ply_event_loop_process_pending_events(ply_event_loop);
  } else if (verbose)
    printf("%s\n", s);

  free(s);
}

void plymouth_progress(ply_boot_client_t *client, int progress) {
  static int prevprogress = -1;
  char *s;

  if (progress == prevprogress)
    return;
  prevprogress = progress;

  if (got_plymouth) {
    asprintf(&s, "md5check:verifying:%d", progress);
    ply_boot_client_update_daemon(client, s, plymouth_response,
                                  plymouth_response, NULL);
    ply_event_loop_process_pending_events(ply_event_loop);
    free(s);
  } else {
    printf("%d%%...", progress);
    fflush(stdout);
  }
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
  
  FILE *md5_file;
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

  client = ply_boot_client_new();
  if (client)
    ply_event_loop = ply_event_loop_new();
  if (ply_event_loop)
    ply_boot_client_attach_to_event_loop(client, ply_event_loop);

  if (!client || !ply_event_loop || !ply_boot_client_connect(client, plymouth_disconnected, NULL))
  {
    /* Fall back to text output */
    perror("Connecting to plymouth");
    got_plymouth = 0;
  } else
    got_plymouth = 1;


  plymouth_progress(client, 0);
  plymouth_urgent(client, "Checking integrity, this may take some time");
  md5_file = fopen(argv[2], "r");
  if (!md5_file) {
          perror("fopen md5_file");
          exit(1);
  }
  while (fscanf(md5_file, "%as %a[^\n]", &checksum, &checkfile) == 2) {
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
  while (fscanf(md5_file, "%as %a[^\n]", &checksum, &checkfile) == 2) {
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
      plymouth_progress(client, 100*((long double)csize)/tsize);

      md5_append(&state, (const md5_byte_t *)buf, rsize);
      rsize = read(check_fd, buf, sizeof(buf));
    }
    
    close(check_fd);
    md5_finish(&state, digest);
    
    for (i = 0; i < MD5_LEN; i++)
      sprintf(hex_output + i * 2, "%02x", digest[i]);
    
    if (strncmp(hex_output, checksum, strlen(hex_output)) == 0) {
      plymouth_success(client, "%s: OK", checkfile);
    } else {
      plymouth_failure(client, "%s: mismatch", checkfile);
      failed++;
    }
    free(checksum);
    free(checkfile);
  }
  fclose(md5_file);
  if (failed) {
    plymouth_urgent(client, "Check finished: errors found in %d files!", failed);
  } else {
    plymouth_urgent(client, "Check finished: no errors found");
  }

  plymouth_prompt(client, "Press any key to reboot your system");

  reboot(LINUX_REBOOT_CMD_RESTART);
  return 0;
  
}
