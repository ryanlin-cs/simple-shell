#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXLINE 100

/**
 * Precondition: words in buffer are separated by space AND the buffer must
 * end with '\0'; there is at most one operator ('>', '<', '|', or '&') in the
 * buffer
 *
 * Postcondition: each entry of command is a string of non-space
 * characters, or an argument of the command; the last element is a NULL
 * pointer, as required by execvp
 *
 * Return the index in command containing an operator ('>', '<', '|', or '&')
 * or -1 if no operator is found.
 */
int parse_command(char buffer[MAXLINE], char *command[MAXLINE]);
void handler(int s);

int child_pid; // PID of the child in foreground

/*
 * This shell supports '>', '<', '|', and '&'. The first 3 operators are handled
 * in the child process, whereas '&' is handled in the shell process itself.
 */
int main() {
  // Prevents shell from exiting when recieving SIGINT
  signal(SIGINT, handler);

  while (1) {
    char *command[MAXLINE]; // For commands with up to 100 words
    printf("%% ");

    // Read a string from command line:
    /* fgets creates null-terminated string. stdin is pre-defined C constant
     * for standard input.  feof(stdin) tests for file:end-of-file.
     * NOTE: fgets, feof, etc., are part of 'man 3 ...' (Section3 of man pages)
     */
    char buffer[MAXLINE]; // For commands with up to 100 characters
    if (fgets(buffer, MAXLINE, stdin) == NULL && feof(stdin)) {
      printf("Couldn't read from standard input. End of file? Exiting ...\n");
      exit(1); /* any non-zero value for exit means failure. */
    }

    // Fill in 'command' with pointers to words (char *), followed
    //   by a NULL pointer.  'execvp' requires that the last pointer be NULL.
    int operator_idx = parse_command(buffer /* input */, command /* output */);
    if (command[0] == NULL) {
      // Empty line, so nothing to run
      continue;
    }

    int childpid = fork(); // See 'man fork'
    // Retry if fork failed
    if (childpid == -1) {
      perror("fork");
      continue;
    }

    if (childpid == 0) { // if child
      // Ensure child process doesn't inherit signal handler in parent
      // child process should be terminated by SIGINT
      signal(SIGINT, SIG_DFL);

      // Handle '|'
      if (operator_idx != -1 && strcmp(command[operator_idx], "|") == 0) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
          perror("pipe");
          exit(1);
        }

        // grand_child runs the left command, and the child process runs the
        // right command with the data in the pipe
        int grand_child = fork();
        if (grand_child == -1) {
          perror("fork");
          exit(1);
        }

        // if grand_child process: run left command
        if (grand_child == 0) {
          if (dup2(pipefd[1], 1) == -1) {
            perror("dup2");
            exit(1);
          }

          close(pipefd[0]); // Does not need the read end
          close(pipefd[1]); // No longer need the write end

          // Execute left command (stops at |)
          command[operator_idx] = NULL;
          execvp(command[0], command);
          perror("execvp");
          exit(1);
        } else {
          // if child process: run right command
          if (dup2(pipefd[0], 0) == -1) {
            perror("dup2");
            exit(1);
          }

          close(pipefd[0]); // No longer need the read end
          close(pipefd[1]); // Must close the write end to indicate EOF

          // Execute the right command; `parse_command` already guarentees NULL
          // at the end of command
          execvp(command[operator_idx + 1], &command[operator_idx + 1]);
          perror("execvp");
          exit(1);
        }
      }

      // Handle '>' and '<'
      if (operator_idx != -1) {
        if (strcmp(command[operator_idx], ">") == 0) {
          char *path = command[operator_idx + 1];
          int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
          if (fd == -1) {
            perror("open");
            exit(1);
          }

          // Replaced stdout with file at path
          if (dup2(fd, 1) == -1) {
            perror("dup2");
            exit(1);
          }

          close(fd);
        } else if (strcmp(command[operator_idx], "<") == 0) {
          char *path = command[operator_idx + 1];
          int fd = open(path, O_RDONLY);
          if (fd == -1) {
            perror("open");
            exit(1);
          }

          // Replaced stdin with file at path
          if (dup2(fd, 0) == -1) {
            perror("dup2");
            exit(1);
          }

          close(fd);
        }

        // execvp stops at the operator
        command[operator_idx] = NULL;
      }

      execvp(command[0], command);
      perror("execvp");
      exit(1);
    } else { // else parent
      // For removing background child process whose state is zombie
      while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

      // Handle `&`; do not wait for background child process
      if (operator_idx != -1 && strcmp(command[operator_idx], "&") == 0) {
        continue;
      }

      // Wait for foreground child process
      child_pid = childpid;
      if (waitpid(childpid, NULL, 0) == -1) {
        perror("waitpid");
        continue;
      } else {
        child_pid = -1;
      }
    }
  }
}

void handler(int s) {
  if (child_pid <= 0) {
    // No foreground process
    return;
  }
  if (kill(child_pid, SIGINT) == 1) {
    perror("kill");
  }
}

int parse_command(char buffer[MAXLINE], char *command[MAXLINE]) {
  int operator = -1;

  // Strip '\n' that appears at the end of the command
  char *endline = strstr(buffer, "\n");
  if (endline != NULL) {
    *endline = '\0';
  }

  // strtok modifies buffer by changing space to '\0', splitting buffer into
  // tokens.
  int i = 0;
  for (char *ptr = strtok(buffer, " "); ptr != NULL; ptr = strtok(NULL, " ")) {
    if (strcmp(ptr, ">") == 0 || strcmp(ptr, "<") == 0 ||
        strcmp(ptr, "|") == 0 || strcmp(ptr, "&") == 0) {
      operator = i;
    }
    command[i] = ptr;
    i++;
  }

  // Set the last element of command to NULL
  command[i] = NULL;

  return operator;
}
