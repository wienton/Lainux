#ifndef RUN_COMMAND_H
#define RUN_COMMAND_H

int run_command_with_fallback(const char *cmd, const char *fallback);
int run_command(const char *cmd, int show_output);


#endif // run command header
