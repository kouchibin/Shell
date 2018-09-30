/*
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file
 * you will need to modify Makefile to compile
 * your additional functions.
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Submit the entire lab1 folder as a tar archive (.tgz).
 * Command to create submission archive:
      $> tar cvf lab1.tgz lab1/
 *
 * All the best
 */


#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "parse.h"

#define PERM S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH

/*
 * Function declarations
 */

void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);
pid_t wait(int *wstatus);
pid_t fork(void);
int setpgid(pid_t pid, pid_t pgid);
void executePgm(Pgm *pgm);
void eexit(char *description);
void setupExecEnv(Command *cmd);
void SIGCHLD_handler(int signo);
void SIGINT_handler(int signo);

/* When non-zero, this global means the user is done using this program. */
int done = 0;

/* The pid of the process running in foreground. */
pid_t foreground_pid = 0;

/*
 * Name: main
 *
 * Description: Gets the ball rolling...
 *
 */
int main(void)
{
  Command cmd;
  int n;

  while (!done) {

    char *line;

    /* Register singal handlers */
    signal(SIGCHLD, SIGCHLD_handler);
    signal(SIGINT, SIGINT_handler);

    line = readline("> ");

    if (!line) {
      /* Encountered EOF at top level */
      done = 1;
    }
    else {
      /*
       * Remove leading and trailing whitespace from the line
       * Then, if there is anything left, add it to the history list
       * and execute it.
       */
      stripwhite(line);

      if(*line) {
        add_history(line);

        n = parse(line, &cmd);

        if (n == 1) {
          /* Parse cmd successfully */

          /* Handle the cd command */
          if (!strcmp((cmd.pgm) -> pgmlist[0], "cd") && !((cmd.pgm) -> next)) {
            char *to = (cmd.pgm) -> pgmlist[1];
            if (!to)
              to = getenv("HOME");
            if (chdir(to) == -1)
              perror("chdir error: ");
            continue;
          }

          /* Handle the exit command */
          if (!strcmp((cmd.pgm) -> pgmlist[0], "exit")&& !((cmd.pgm) -> next))
            exit(0);

          pid_t pid = fork();
          if (pid < 0) {
            eexit("Error when forking.\n");
          } else if (pid == 0) {
            /* In child process */
            /* Setup executing environment */
            setupExecEnv(&cmd);
            /* Start executing program list */
            executePgm(cmd.pgm);
          } else {
            /* In parent process */
            int status;

            /* Record the child process's pid, in case we want to kill it */
            foreground_pid = pid;

            if (!cmd.bakground && wait(&status) == -1) {
              /* Wait for child process to finish and clean up.
               * For background process, leave it to signal_handler.
               */
              eexit("Error when waiting for child process to complete.\n");
            }

            /* Nothing in foreground now */
            foreground_pid = 0;
          }
        } else {
          printf("Parsing cmd failed.\n");
        }

      }
    }

    if(line) {
      free(line);
    }
  }
  return 0;
}

/*
 * Name: setupExecEnv
 *
 * Description: Setup Execution Environment. Including wether or not runing in
 * background and redirection.
 *
 */
void setupExecEnv(Command *cmd)
{
  if (cmd -> bakground) {
    /* Run command in background */
    setpgid(0, 0);
  }

  if (cmd -> rstdin) {
    /* Redirect stdin to file */
    int infd;
    if ((infd = open(cmd -> rstdin, O_RDONLY)) < 0)
      eexit("open file error.\n");
    if (infd != STDIN_FILENO)
      if (dup2(infd, STDIN_FILENO) != STDIN_FILENO)
        eexit("dup2 error.\n");
    close(infd);
  }

  if (cmd -> rstdout) {
    /* Redirect stdout to file */
    int outfd;
    if ((outfd = open(cmd -> rstdout, O_CREAT | O_WRONLY | O_TRUNC, PERM)) < 0)
      eexit("open file error.\n");
    if (outfd != STDOUT_FILENO)
      if (dup2(outfd, STDOUT_FILENO) != STDOUT_FILENO)
        eexit("dup2 error.\n");
    close(outfd);
  }
}


/*
 * Name: executePgm
 *
 * Description: Execute all the commands that are in Pgm linked list. Every
 * command is run as the child process of the previous command in the linked list.
 *
 */
void executePgm(Pgm *pgm) {
  if (pgm == NULL) return;

  if (pgm -> next) {
    /* Has more command */
    int pipefd[2], status;
    pid_t pid;
    if (pipe(pipefd) == -1)
      eexit("Fail to create pipe.\n");
    pid = fork();
    if (pid < 0) {
      eexit("Fail to create child process when executing command.\n");
    } else if (pid == 0) {
      /* In child process, write end of the pipe */
      close(pipefd[0]);
      if (pipefd[1] != STDOUT_FILENO) {
        if (dup2(pipefd[1], STDOUT_FILENO) != STDOUT_FILENO)
          eexit("dup2 error.\n");
        close(pipefd[1]);
      }

      /* Call executePgm recursively */
      executePgm(pgm -> next);

    } else {
      /* In parent process, read end of the pipe */
      close(pipefd[1]);

      /* Bind stdin to the read end of the pipe */
      if (pipefd[0] != STDIN_FILENO) {
        if (dup2(pipefd[0], STDIN_FILENO) != STDIN_FILENO)
          eexit("dup2 error.\n");
        close(pipefd[0]);
      }

      /* Wait for the child process to finish */
      wait(&status);

      /* Execute the expected program after child finish*/
      execvp(pgm->pgmlist[0], pgm->pgmlist);
      perror("lsh error");
      exit(-1);
    }
  } else {
    /* No more command */
    execvp(pgm->pgmlist[0], pgm->pgmlist);
    perror("lsh error");
    exit(-1);
  }
}

/*
 * Name: eexit
 *
 * Description: Print error message and exit with code -1.
 *
 */
void eexit(char *description)
{
  perror(description);
  exit(-1);
}

/*
 * Name: SIGCHLD_handler
 *
 * Description: Handle the SIGCHLD signal and clean up the zombies
 *
 */
void SIGCHLD_handler(int signo)
{
  if (signo == SIGCHLD) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
      printf("Process PID=%d now terminated.\n", pid);
    }
  }
}

/*
 * Name: SIGINT_handler
 *
 * Description: Handle the SIGINT signal and terminate foreground running
 * process.
 *
 */
void SIGINT_handler(int signo)
{
  if (signo == SIGINT && foreground_pid > 0) {
    if ((kill(foreground_pid, SIGTERM)) == -1)
      eexit("Error when terminating foreground process.\n");
    else
      foreground_pid = 0;
  }

}


/*
 * Name: PrintCommand
 *
 * Description: Prints a Command structure as returned by parse on stdout.
 *
 */
void
PrintCommand (int n, Command *cmd)
{
  printf("Parse returned %d:\n", n);
  printf("   stdin : %s\n", cmd->rstdin  ? cmd->rstdin  : "<none>" );
  printf("   stdout: %s\n", cmd->rstdout ? cmd->rstdout : "<none>" );
  printf("   bg    : %s\n", cmd->bakground ? "yes" : "no");
  PrintPgm(cmd->pgm);
}



/*
 * Name: PrintPgm
 *
 * Description: Prints a list of Pgm:s
 *
 */
void
PrintPgm (Pgm *p)
{
  if (p == NULL) {
    return;
  }
  else {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */
    PrintPgm(p->next);
    printf("    [");
    while (*pl) {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}

/*
 * Name: stripwhite
 *
 * Description: Strip whitespace from the start and end of STRING.
 */
void
stripwhite (char *string)
{
  register int i = 0;

  while (isspace( string[i] )) {
    i++;
  }

  if (i) {
    strcpy (string, string + i);
  }

  i = strlen( string ) - 1;
  while (i> 0 && isspace (string[i])) {
    i--;
  }

  string [++i] = '\0';
}