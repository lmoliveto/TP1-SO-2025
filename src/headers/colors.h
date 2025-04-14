#ifndef COLORS_H
#define COLORS_H

#define ANSI_CLEAR_SCREEN "\033[H\033[J"

// ANSI Colors
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_HIGH_INTENSITY_RED "\x1b[91m"
#define ANSI_HIGH_INTENSITY_GREEN "\x1b[92m"
#define ANSI_HIGH_INTENSITY_BLUE "\x1b[94m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_GRAY    "\x1b[90m"

extern const char * colors[];

#endif