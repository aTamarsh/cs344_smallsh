/*
  Tamarsh Abeysekera, CS 344 - Operating Systems
  Assignment 3: smallsh
  `main.c` file - 
  implements `smallsh` with a subset of features of well-known shells
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>

/*
  Struct to keep track of user-entered commands

  `char **argument` - an array of char pointers
    e.g. [0] will be user's commnand, with [1] following "args"

  `uInput` - to represent user `input` designation;
  `uOutput - to represent user `output` designation;
  `numArguments` - # of args entered by User;
  `background` - indicating intended backgrunding by User
*/
struct command {
  char **argument;
  char *uInput;
  char *uOutput;
  int numArguments; 
  int background;
};

// GLOBAL for handling `SIGTSTP` (terminal stop)
int TSTP = 0;

//  function prototypes
struct command checkArguments(char *userInput);
int listDirectory(struct command uCommand);
int changeDirectory(char *userArg);
void expansion(char *userInput);
int runProcess(struct command uCommand);
void ignoreSigint();
void handle_SIGTSTP(int signo);


// function implementations

/*
  To handle a3 requirements for `SIGTSTP`
  Simply sets TSTP to 1 
*/
void handle_SIGTSTP(int signo){
  TSTP = 1;
}


/*
  Ignores SIGINT
*/
void ignoreSigint(){
  struct sigaction ignore_action = {0};
  ignore_action.sa_handler = SIG_IGN;
  sigaction(SIGINT, &ignore_action, NULL);
}


/*
  `runProcess` 

  `uCommand` - filled User's command object passed in;
  returns exit `status` to caller (main);
*/
int runProcess(struct command uCommand){
  char **argv = uCommand.argument;
  int status = 0;

  pid_t spawnPid = fork();

  // handle cases
  switch (spawnPid) {
    // case where child process failed
    case -1: {
      perror("fork()\n");
      exit(EXIT_FAILURE);
      break;
    }

    // within child process
    case 0: {
      // any child running as background process MUST ignore SIGINT
      if (uCommand.background) {
        ignoreSigint();

        // if user doesn't redirect sInput for a backgrund cmd, 
        // sInput redirect to /dev/null & same for sInput
        if (!uCommand.uOutput) {
          uCommand.uOutput = strdup("/dev/null");
        }

        if (!uCommand.uInput) {
          uCommand.uInput = strdup("/dev/null");
        }
      } else { // children in fg must terminate itself when receiving `SIGINT`
        struct sigaction ignore_action = {0};  
        ignore_action.sa_handler = SIG_DFL;
        sigaction(SIGINT, &ignore_action, NULL);
      }

      // setting ignores for `SIGTSTP`
      // any children running as fg/bg process must ignore SIGTSTP
      struct sigaction ignore_action = {0};
      ignore_action.sa_handler = SIG_IGN;
      sigaction(SIGTSTP, &ignore_action, NULL);

      // handling Redirection & Output below
      if (uCommand.uInput) {
        int fd = open(uCommand.uInput, O_RDONLY); 

        if (fd < 0) {
          perror("Unable to open file for Redirection\n");
          exit(EXIT_FAILURE); 
        }

        int result = dup2(fd, 0);
        if (result == -1) {
          perror("source dup2()\n");
          exit(EXIT_FAILURE);
        }
      }

      if (uCommand.uOutput) {
        int fd = open(uCommand.uOutput, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

        if (fd < 0) {
          printf("Unable to open file %s", uCommand.uOutput);
          perror(" for Output\n");
          exit(EXIT_FAILURE);
        }

        int result = dup2(fd, 1);
        if (result == -1) {
          perror("target dup2()\n");
          exit(EXIT_FAILURE);
        }
      }

      // using `execvp` from exec() family
      execvp(argv[0], argv);

      perror("execv error");
      exit(EXIT_FAILURE);
      break;
    }

    // in the parent process
    default: {
      //  if background is set, report background pid;
      if (uCommand.background) {
        printf("background pid is %d\n", spawnPid);

        return spawnPid;
      }


      waitpid(spawnPid, &status, 0);

      return WEXITSTATUS(status);
      //printf("PARENT(%d): child(%d) terminated. Exiting\n", getpid(), spawnPid);
    }  
  }
}



/*
  Change to specified directory;

  Returns 0 on success, -1 on error.
*/
int changeDirectory(char *userArg){
  if (!userArg) {
    userArg = getenv("HOME");
  }
  //printf("User arg is: %s\n", userArg);

  return chdir(userArg);
}


/*
  Lists the contents of the Directory `ls`

  if `ls` outputs content, use `open` to write only

  *Unnecessarily implemented by mistake*
*/
int listDirectory(struct command uCommand){
  static int stdoutCopy = -1;
  if (stdoutCopy == -1) {
    stdoutCopy = dup(1);
  }

  // if user included > create output file
  if (uCommand.uOutput) {
    int fd = open(uCommand.uOutput, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    // communicate failures;
    if (fd == -1) {
      perror("open() failed on uOutput\n");
      return EXIT_FAILURE;
    }

    if (dup2(fd, 1) < 0) {
      perror("Unable to redirect stdout\n");
      close(fd);
      return EXIT_FAILURE;
    }

    close(fd);
  }

  DIR *currentDir = opendir(".");
  struct dirent *aDir = NULL;
  
  //go through all entries
  while ((aDir = readdir(currentDir))) {
    printf("%s\n", aDir->d_name);
  }

  if (uCommand.uOutput) {
    if (dup2(stdoutCopy, 1) < 0) {
      perror("Unable to restore stdout\n");
      exit(EXIT_FAILURE);
    }
  }

  //close(stdoutCopy);
  return EXIT_SUCCESS;
}


/*
  checkArguments checks to parse user-entered argument.

  Returns a `command` Struct obj. filled with:
  command name, arguments, number of arguments, redirects/backgrounding
  on Success.

  exits with status failure for reasosn specified, otherwise.
*/
struct command checkArguments(char *userInput) {
  //init command object for user command and allocate
  struct command uCommand = {0};
  uCommand.argument = calloc(513, sizeof(char *));
  if (!uCommand.argument) {
    fputs("Failed to allocate `argument`, exiting program\n", stdout);
    exit(EXIT_FAILURE);
  }

  //  for use with strtok_r
  char *savePtr = NULL;

  char *token = strtok_r(userInput, " \n", &savePtr);
  int x;

  // process user command & args, [maximum of 512 args]
  // tokenize redirects & backgrounding;
  for (x = 0; x < 512 && token; x++) {
    if (token[0] == '>' || token[0] == '<') {
      char operator = token[0];

      token = strtok_r(NULL, " \n", &savePtr);
      if (!token) {
        fputs("No File specified for redirect\n", stdout);
        exit(EXIT_FAILURE);
      }

      if (operator == '>') {
        uCommand.uOutput = strdup(token);
      } else {
        uCommand.uInput = strdup(token);
      }

      // continue when we've tokenized redirects/backgrounding;
      token = strtok_r(NULL, " \n", &savePtr);
      continue;
    } else if (token[0] == '&') {
      uCommand.background = 1;
      token = strtok_r(NULL, " \n", &savePtr);
      continue;
    }

    // the first token is the `command`
    uCommand.argument[uCommand.numArguments] = strndup(token, strlen(token));
    if (!uCommand.argument[uCommand.numArguments]) {
      fputs("Failed to allocate user command, exiting program\n", stdout);
      exit(EXIT_FAILURE);
    }
    uCommand.numArguments++;

    token = strtok_r(NULL, " \n", &savePtr);
  }
  //printf("%d of args\n",uCommand.numArguments);

  return uCommand;
}


/*
  printArguments to debug `checkArguments`

  for testing purposes
*/
void printArguments(struct command uCommand) {
  if (uCommand.uInput) {
    printf("User Input: %s\n", uCommand.uInput);
  }
  if (uCommand.uOutput) {
    printf("User Output: %s\n", uCommand.uOutput);
  }
  printf("User Background: %d\n", uCommand.background);

  for (int i = 0; i < uCommand.numArguments; i++) {
    printf("Argument # %d is %s\n", i, uCommand.argument[i]);
  }
}


/*
  `expansion` for variable $$
  
  e.g. `foo$$$$` converts `foo179179`

  command lines have MAX LENGTH of 2048 chars & 512 for max args
*/
void expansion(char *userInput){
  // 2048*2 = 4096+1
  char fixedInput[4097] = {0};
  int pid = getpid();

  char pidStr[10];
  sprintf(pidStr, "%d", pid);
  int pidLen = strlen(pidStr);

  // x tracking where we are in userInput
  // y tracking where we are in fixedInput
  int x = 0, y = 0;
  while (userInput[x]) {
    if (userInput[x] == '$' && userInput[x+1] == '$') {
      strncpy(&fixedInput[y], pidStr, pidLen);
      y += pidLen;
      x += 2;
      continue;
    }
    
    // increment both after copying regular characters
    fixedInput[y++] = userInput[x++];
  }

  // void *memcpy(void *dest, const void *src, size_t n);
  memcpy(userInput, fixedInput, 4097);
}


/*
  Ths program will:
    1. provide a prompt for running commands;
    2. handle blank lines & comments (which begin with '#');
    3. provide expansion for variable `$$`;
    4. executes 3 cmds `exit`, `cd`, `status` via code built into shell;
    5. executes other cmds by creating newProcesses with `exec` family of fNs;
    6. supports Input & Output redirectiono;
    7. supports running cmds in fg & bg processes
    8. has custom handlers for `SIGINT` & `SIGTSTP`

*/
int main(int argc, char *argv[]){
  // init struct obj for user's command
  struct command userCommand;
  // disable buffering on stdout
  setbuf(stdout, NULL);

  // to track our background process IDs
  int backgroundPids[10] = {0};
  int backgroundPidIndex = 0;
  int backgroundStatus = 0;

  // for handling with custom handlers
  int backgroundDisabled = 0;
  struct sigaction SIGTSTP_action = {0};

  // fillling out SIGTSTP_action struct & registering handle_SIGTSTP as signal handler
  SIGTSTP_action.sa_handler = handle_SIGTSTP;
  // block all catchable signals while handle_SIGTSTP runs
  sigfillset(&SIGTSTP_action.sa_mask);
  // no flags set
  SIGTSTP_action.sa_flags = 0;

  //installing our signal handler
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);

  char userInput[4097] = {0};
  int status = 0;

  //  ignoring in shell/parent process;
  ignoreSigint();

  while (strncmp(userInput, "exit", 4)) {
    if (TSTP) {
      backgroundDisabled = !backgroundDisabled;
      TSTP = 0;

      if (backgroundDisabled) {
        printf("Entering foreground-only mode (& is now ignored)\n");
      } else {
        printf("Exiting foreground-only mode\n");
      }
    }

    for (int i = 0; i < backgroundPidIndex ; i++) {
      if (backgroundPids[i]) {
        int terminated = waitpid(backgroundPids[i], &backgroundStatus, WNOHANG);

        // when background process terminates, msg showing pId & exit status to print
        if (terminated) {
          printf("background pid %d is done: exit value %d", 
              backgroundPids[i], WEXITSTATUS(backgroundStatus));

          if (WIFSIGNALED(backgroundStatus)) {
            printf(", terminated due to receiveing Signal %d", 
                WTERMSIG(backgroundStatus));
          }

          fputs("\n", stdout);

          backgroundPids[i] = 0;
        }
      }
    }

    fputs("$ ", stdout);
    // avoiding using scanf and using `gets` here
    fgets(userInput, 2049, stdin);


    expansion(userInput);
    userCommand = checkArguments(userInput);
    //printArguments(userCommand);

    if (backgroundDisabled) {
      userCommand.background = 0;
    }

    // check user-entered commands;
    if (userCommand.numArguments == 0 || userCommand.argument[0][0] == '#' ) {
      continue;
    } else if (!strncmp(userCommand.argument[0], "exit", 4)) {
      return EXIT_SUCCESS;
    //} else if (!strncmp(userCommand.argument[0], "ls", 2)) {
      //status = listDirectory(userCommand);
    } else if (!strncmp(userCommand.argument[0], "status", 6)) {
      printf("%d\n", status);
      status = EXIT_SUCCESS;
    } else if (!strncmp(userCommand.argument[0], "cd", 2)) {
      status = changeDirectory(userCommand.argument[1]); 
      if (status) {
        perror("change directory error");
      }
    } else {
      status = runProcess(userCommand);
      fflush(stdout);

      if (userCommand.background) {
        backgroundPids[backgroundPidIndex++] = status;

        status = 0;
      }
    }

    // remember to free dynamically allocated memory;
    for (int i = 0; userCommand.argument[i]; i++) {
      free(userCommand.argument[i]);
    }
    free(userCommand.argument);
  }
  
  return EXIT_SUCCESS;
}
