/* This relinquishes the controlling terminal of wherever it's invoked.  I use this when I want
 * something else to be able to take control of that terminal.  I use this to help when running a
 * program under GDB, to have the program's stdio be a separate terminal than GDB's, so that the
 * program's input/output doesn't pollute the interactive use of GDB.  That is especially helpful
 * for programs that do TUI or verbose things with the terminal while you want to use GDB on the
 * program - otherwise having the TUI or verbosity on the same terminal as GDB would make using
 * GDB unintelligible.
 *
 * For example:
 *
 * First, create a new terminal for the debugged program to use.  That requires the terminal to
 * not be the controlling of anything when GDB needs to take control of it to start the debugged
 * program.  That is why we relinquish the controlling terminal:
 *
 * ```shell
 * mate-terminal --execute noctty
 * ```
 *
 * That will print whatever the terminal's pathname is, which then needs to be given as argument
 * to the following GDB command:
 *
 * ```gdb
 * set inferior-tty $THE_NEW_TTY
 * ```
 */

#include <assert.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef __COMMIT__
  #define __COMMIT__ "(unspecified)"
#endif


static void
bail(char const * const msg)
{
  perror(msg);
  exit(EXIT_FAILURE);
}


#define verbose_print(msg)              \
  if (verbose) {                        \
    printf(msg);                        \
    fflush(stdout);                     \
  }


static void
print_tty(bool const verbose)
{
  verbose_print("Terminal is:\n")
  int const status = system("tty");
  assert(WIFEXITED(status));
  assert(WEXITSTATUS(status) == 0);
  verbose_print("------------------------------------------------------------\n\n")
}


static void
sighup_action(void(*action)(int))
{
  int const r = sigaction(SIGHUP,
                          &(struct sigaction const) { .sa_handler = action },
                          NULL);
  if (r != 0) { bail("sigaction error"); }
}

static void
relinquish_controlling_tty(void)
{
  // Must ignore SIGHUP because the TIOCNOTTY ioctl might send that signal to us.  Otherwise,
  // delivery of that signal would terminate us.
  sighup_action(SIG_IGN);

  int const controlling_tty = open("/dev/tty", O_RDWR | O_NOCTTY);
  if (controlling_tty == -1) {
    bail("error opening /dev/tty");
  }

  {
    int const r = ioctl(controlling_tty, TIOCNOTTY);
    if (r == -1) { bail("ioctl error"); }
  }

  // Reinstate SIGHUP, just to go back to normal default.
  sighup_action(SIG_DFL);
}


static int
run_given_command(char const * const command)
{
  int const status = system(command);

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  else if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  else {
    assert(false);
    return -1;
  }
}


static void
block_forever(void)
{
  sem_t sem;
  int r = sem_init(&sem, false, 0);
  if (r != 0) { bail("sem_init error"); }

  r = sem_wait(&sem);
  if (r != 0) { bail("sem_wait error"); }
}


static void
print_help(FILE *stream, char const * const self)
{
  fprintf(stream, "Usage: %s [-v] [COMMAND]\n", self);
  fprintf(stream, "Relinquish the controlling terminal. Optionally, run a command.\n");
  fprintf(stream, "(Built from %s on %s.)\n", __COMMIT__, __DATE__);
}


typedef struct options {
  char const *command;
  bool verbose;
} options;

static options
process_args(int const argc, char * const argv[])
{
  options opts = {
    .command = NULL,
    .verbose = false,
  };

  int c;
  while ((c = getopt(argc, argv, "hv")) != -1) {
    switch (c) {
    case 'h':
      print_help(stdout, argv[0]);
      exit(EXIT_SUCCESS);
      break;
    case 'v':
      opts.verbose = true;
      break;
    case '?':
      exit(EXIT_FAILURE);
      break;
    default:
      assert(false);
      break;
    }
  }

  int const posargc = argc - optind;
  char * const * const posargv = &argv[optind];

  if (posargc >= 1) {
    opts.command = posargv[0];
  }

  if (posargc >= 2) {
    fprintf(stderr, "error: invalid arguments\n");
    fprintf(stderr, "\n");
    print_help(stderr, argv[0]);
    exit(EXIT_FAILURE);
  }

  return opts;
}


int
main(int const argc, char * const argv[])
{
  options const opts = process_args(argc, argv);

  print_tty(opts.verbose);

  relinquish_controlling_tty();

  if (opts.command) {
    return run_given_command(opts.command);
  }
  else {
    block_forever();
  }
}
