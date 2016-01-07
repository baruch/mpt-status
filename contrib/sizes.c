#include <stdio.h>
#include <stdlib.h>

/* This was once used to help debugging 32/64 bit issues with mpt-status. */

int main(void) {
	printf("int: %d\n"
		"char ptr: %d\n"
		"void ptr: %d\n"
		"long: %d\n"
		"char: %d\n",
		sizeof(int),
		sizeof(char *),
		sizeof(void *),
		sizeof(long),
		sizeof(char)
		);
	exit(EXIT_SUCCESS);
}
