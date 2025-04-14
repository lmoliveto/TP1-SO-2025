#ifndef ARGS_H
#define ARGS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

typedef enum {
    ARG_TYPE_INT,
    ARG_TYPE_STRING,
    ARG_VAR_INT,
    ARG_VAR_STRING
} ARG_TYPE;

/**
 * @brief `opt_flag` Character to look for as a flag in argv
 * 
 * `type` must be `ARG_TYPE_INT` or `ARG_TYPE_STRING`
 * 
 * `addr` is the location in memory where the value will be written to.
 * 
 * `get_vararg_addr` is a function that returns the address where the i-th argument must be stored, can be NULL if not ARG_VAR.
 */
typedef struct {
    char opt_flag;
    ARG_TYPE type;
    void * addr;
    void * (*get_vararg_addr) (int);
} Parameter;

/**
 * @brief Parses parameters and stores them in the provided array.
 * 
 * @param argc Number of arguments
 * 
 * @param argv Array of arguments
 * 
 * @param params Array of Parameter structs
 * 
 * @param param_count Number of parameters
 * 
 * @param optstring String with the options to parse -- forwared to getopt
 * 
 * @param max_str_size Maximum size of the string to be copied
 */
void parse_arguments(int argc, char * argv[], Parameter params[], int param_count, const char * optstring, unsigned int max_str_size);

#endif