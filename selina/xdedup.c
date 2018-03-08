#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <getopt.h>
#include "utils.h"
#ifndef __NR_xdedup
#error xdedup system call not defined
#endif
#define FLAG_N  0x01 // decimal 1
#define FLAG_D  0x02 // decimal 2
#define FLAG_P  0x04 // decimal 4

int main(int argc, char *const *argv)
{
	int c,index,rc,argcount=0;
	struct myargs *arglist = (struct myargs *) malloc(sizeof(struct myargs));
	arglist->flag=0;
	while ((c = getopt (argc, argv, "pnd")) != -1)
		switch (c)
		{
			case 'p':
				arglist->flag |= FLAG_P;
				break;
			case 'n':
				arglist->flag |= FLAG_N;
				break;
			case 'd':
				arglist->flag |= FLAG_D;
				break;
			default:
				printf("Input not supported\n");
				exit(0);
		}
	index = optind;
	argcount=argc-index;
	if(!(arglist->flag & FLAG_P))
	{
		if(argcount>2)
		{
			printf("Extra Arguments Supplied\n");
			return -E2BIG;
		}
		else if(argcount<2)
		{
			printf("Insufficient number of arguments supplied\n");
			return -EINVAL;
		}
	}
	else
	{
		if(argcount>3)
		{
			printf("Extra Arguments Supplied\n");
			return -E2BIG;
		}
		else if(argcount<3)
		{
			if(arglist->flag & FLAG_N) {
				if(argcount<2) {
					printf("Insufficient number of arguments supplied\n");
					return -EINVAL;
				}
			}
			else{
				printf("Insufficient number of arguments supplied\n");
				return -EINVAL;
			}
		}
	}
	arglist->f1=argv[index++];
	arglist->f2=argv[index++];
	if((arglist->flag & FLAG_P) && !(arglist->flag & FLAG_N)) {
		arglist->outf = argv[index++];
	}

	if(access(arglist->f1,F_OK | R_OK)==-1 || access(arglist->f2,F_OK | R_OK)==-1)
	{
		printf("One of the two files not found or we can't access it\n");
		return -EACCES;
	}

    printf("\n-----------Inside user space----------\n");
	rc = syscall(__NR_xdedup, (void *) arglist);
	if (rc == 0)
		printf("syscall returned %d\n", rc);
	else
		printf("syscall returned %d (errno=%d)\n", rc, errno);

	exit(rc);
}
