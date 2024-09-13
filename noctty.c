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
print_tty(void)
{
  printf("Terminal is:\n");
  system("tty");
  printf("------------------------------------------------------------\n\n");
}


static void
sighup_action(void(*action)(int))
{
  int const r = sigaction(SIGHUP,
                          &(struct sigaction const) { .sa_handler = action },
                          NULL);
  if (r != 0) { perror("sigaction error"); }
}

static void
relinquish_controlling_tty(void)
{
  // Must ignore SIGHUP because the TIOCNOTTY ioctl might send that signal to us.  Otherwise,
  // delivery of that signal would terminate us.
  sighup_action(SIG_IGN);

  int const controlling_tty = open("/dev/tty", O_RDWR | O_NOCTTY);
  if (controlling_tty == -1) {
    perror("error opening /dev/tty");
  }

  {
    int const r = ioctl(controlling_tty, TIOCNOTTY);
    if (r == -1) { perror("ioctl error"); }
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


static int
block_forever(void)
{
  sem_t sem;
  int r = sem_init(&sem, false, 0);
  if (r != 0) { perror("sem_init error"); }

  r = sem_wait(&sem);
  if (r != 0) { perror("sem_wait error"); }
  return r;
}


static void
print_help(FILE *stream, char const * const self)
{
  fprintf(stream, "Usage: %s [COMMAND]\n", self);
  fprintf(stream, "Relinquish the controlling terminal. Optionally, run a command.\n");
  fprintf(stream, "(Built from %s on %s.)\n", __COMMIT__, __DATE__);
}

static char const *
process_args(int const argc, char * const argv[])
{
  int c;
  while ((c = getopt(argc, argv, "h")) != -1) {
    switch (c) {
    case 'h':
      print_help(stdout, argv[0]);
      exit(EXIT_SUCCESS);
      break;
    case '?':
      exit(EXIT_FAILURE);
      break;
    default:
      assert(false);
      break;
    }
  }

  if (argc == 1) {
    return NULL;
  }
  else if (argc == 2) {
    char const * const command = argv[1];
    return command;
  }
  else {
    fprintf(stderr, "error: invalid arguments\n");
    fprintf(stderr, "\n");
    print_help(stderr, argv[0]);
    exit(EXIT_FAILURE);
  }
}


int
main(int const argc, char * const argv[])
{
  char const * const command = process_args(argc, argv);

  print_tty();

  relinquish_controlling_tty();

  if (command) {
    return run_given_command(command);
  }
  else {
    return block_forever();
  }
}
