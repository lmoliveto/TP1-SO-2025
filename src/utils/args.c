#include "args.h"

extern int opterr;
extern int optind;

static int find_param(Parameter params[], char flag) {
    for (int i = 0; params[i].opt_flag != '\0'; i++) {
        if (params[i].opt_flag == flag) {
            return i;
        }
    }
    
    errno = EINVAL;
    fprintf(stderr, "Invalid option, Parameter structure not provided: %c\n", flag);
    exit(EXIT_FAILURE);
}

void parse_arguments(int argc, char * argv[], Parameter params[], int param_count, const char * optstring, unsigned int max_str_size) {
    char c;
    opterr = 0; // https://stackoverflow.com/a/24331449
    optind = 0;

    while ((c = getopt(argc, argv, optstring)) != -1 && c != ((unsigned char) -1) ) {
        c = tolower(c);

        if (c < 'a' || c > 'z') {
            errno = EINVAL;
            fprintf(stderr, "Invalid option: %c\n", c);
            exit(EXIT_FAILURE);
        }

        Parameter p = params[find_param(params, c)];

        switch (p.type) {
            case ARG_TYPE_INT:
                if (optarg == NULL) break;
                *((int *)p.addr) = atoi(optarg);
                break;
            case ARG_TYPE_STRING:
                if (optarg == NULL) break;

                if (strlen(optarg) >= max_str_size - 1) {
                    errno = EINVAL;
                    fprintf(stderr, "String too long for option '%c'\n", c);
                    exit(EXIT_FAILURE);
                }

                strncpy((char *)p.addr, optarg, max_str_size - 1);
                ((char *)p.addr)[strlen(optarg)] = '\0';

                break;

            case ARG_VAR_INT:
            case ARG_VAR_STRING:
                int i = 0;
                optind--;
                for (; optind < argc && argv[optind][0] != '-'; optind++) {
                    void * addr;

                    if (p.get_vararg_addr != NULL) {
                        addr = p.get_vararg_addr(i++);
                        if (addr == NULL) {
                            errno = EINVAL;
                            fprintf(stderr, "Invalid address for option '%c'\n", c);
                            exit(EXIT_FAILURE);
                        }
                    } else {
                        addr = (p.addr);
                    }

                    if (p.type == ARG_VAR_INT) {
                        *((int *)addr) = atoi(argv[optind]);
                    } else if (p.type == ARG_VAR_STRING) {
                        if (strlen(optarg) >= max_str_size - 1) {
                            errno = EINVAL;
                            fprintf(stderr, "String too long for option '%c'\n", c);
                            exit(EXIT_FAILURE);
                        }

                        strncpy((char *)addr, argv[optind], max_str_size - 1);
                        ((char *)addr)[strlen(optarg)] = '\0';
                    }
                };
                break ;
                
            default:
                errno = EINVAL;
                fprintf(stderr, "Invalid argument type for option '%c'\n", c);
                exit(EXIT_FAILURE);
        }
    }
}
