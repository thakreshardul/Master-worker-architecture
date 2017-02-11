#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

float factorial(float n);

void main(int argc, char *argv[])
{
	float x, n, result;
	struct stat buf;

	if (argc < 5)
	{
		printf("Please provide correct number of arguments\n");
		printf("-x <value of x>\n");
		printf("-n <value of n>\n");
		exit(EXIT_FAILURE);
	}
	
	if(strcmp(argv[1],"-x") == 0)
		x = atof(argv[2]);	
	if (strcmp(argv[3],"-n") == 0)
		n = atof(argv[4]);

	int status = fstat(STDOUT_FILENO, &buf);
	if (status == -1)
	{
		fprintf(stderr, "Error in fstat%s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
 
	result = (float)(pow(x,n) / factorial(n));
	
	if (S_ISFIFO(buf.st_mode)) //if worker is writing to pipe, the return value is 1
		write(STDOUT_FILENO, &result, sizeof(float));
	else if (S_ISFIFO(buf.st_mode) == 0)
		printf("x^n / n! : %f\n", result);
	
	
	exit(0);
}

float factorial(float n){
	if (n == 0 || n==1)
		return 1;
	else
		return (n * factorial(n - 1)); 
}