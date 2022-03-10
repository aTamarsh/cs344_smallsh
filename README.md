# CS344: Operating Systems - smallsh
### by Tamarsh Abeysekera, Oregon State University

`smallsh` implements a subset implements a subset of features of well-known shells, such as bash.

This program does the following:
1. Provides a prompt for running commands
2. Handles blank lines and comments, which are lines beginning with the `#` character
3. Provides expansion for the variable `$$`
4. Executes 3 commands `exit`, `cd`, and `status` via code built into the shell.
5. Executes other commands by creating new processes using a function from the `exec` family of functions
6. Supports input and output redirection
7. Supports running commands in foreground and background processes
8. Implements custom handlers for 2 signals, SIGINT and SIGTSTP
