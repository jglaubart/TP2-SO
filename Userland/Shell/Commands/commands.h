#ifndef COMMANDS_H
#define COMMANDS_H

// Includes ------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <tests.h>
#include <sys.h>
#include <syscalls.h>
// ---------------------------------

int _clear(int argc, char **argv);
int _echo(int argc, char **argv);
int _font(int argc, char **argv);
int _help(int argc, char **argv);

int _exception_divzero(int argc, char **argv);
int _exception_invop(int argc, char **argv);
int _man(int argc, char **argv);
int _regs(int argc, char **argv);
int _snake(int argc, char **argv);

int _time(int argc, char **argv);
int _mem_stats(int argc, char **argv);
int _shell_kill(int argc, char **argv);
int _shell_nice(int argc, char **argv);
int _ps(int argc, char **argv);

// Tests
int _test_mm(int argc, char ** argv);
int _test_processes(int argc, char ** argv);
int _test_prio(int argc, char ** argv);
int _test_sync(int argc, char ** argv);
int _test_wait_children(int argc, char ** argv);

#endif
