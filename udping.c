/*
* This is a functional getopt that already has default values estabilshed. We could move this into a helper function if you'd prefer, 
* although I'm not sure how we would implement calling methods that way. 
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h> // Include bool type

int main(int argc, char **argv) {
    char cflag[30] = "0x7fffffff";
    float iflag = 1.0;
    int pflag = 33333;
    int sflag = 12;
    bool nflag = false; 
    char *cvalue = NULL;
    int index;
    int c;

    opterr = 0;
    // c:i:p:s:n contains the flags we will use but I don't recall what the colons mean (they're supposedly used to specify required flags)
    while ((c = getopt(argc, argv, "c:i:p:s:n")) != -1) { 
        switch (c) {
            case 'c':
                snprintf(cflag, sizeof(cflag), "%s", optarg); 
                break;
            case 'i':
                iflag = atof(optarg); // Convert optarg to float
                break;
            case 'p':
                pflag = atoi(optarg); 
                break;
            case 's':
                sflag = atoi(optarg); 
                break;
            case 'n':
                nflag = true; 
                break;
            case '?': // In case we get 
                if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                return 1;
            default:
                abort();
        }
    }

    printf("cflag = %s\n
            iflag = %.1f\n
            pflag = %d\n
            sflag = %d\n
            nflag = %d\n",
           cflag, iflag, pflag, sflag, nflag);

    for (index = optind; index < argc; index++)
        printf("Non-option argument %s\n", argv[index]);

    return 0;
}