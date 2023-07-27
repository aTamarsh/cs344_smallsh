![smallsh](https://i.imgur.com/HCC6Wlg.png)


`smallsh` implements a subset of features of well-known shells, such as bash.

This program does the following:
1. Provides a prompt for running commands
2. Handles blank lines and comments, which are lines beginning with the `#` character
3. Provides expansion for the variable `$$`
4. Executes 3 commands `exit`, `cd`, and `status` via code built into the shell.
5. Executes other commands by creating new processes using a function from the `exec` family of functions
6. Supports input and output redirection
7. Supports running commands in foreground and background processes
8. Implements custom handlers for 2 signals, SIGINT and SIGTSTP


### Running this program

Makefile included, please use `make` to compile

Run program using `./smallsh`; or,
Run with script using `bash p3testscript`
