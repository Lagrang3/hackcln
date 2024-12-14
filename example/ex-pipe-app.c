#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
	int x;
	while (1) {
		scanf("%d", &x);
		if (x < 0) {
			printf("ENDING EXECUTION!\n");
			fflush(stdout);
			break;
		}
		printf("%d^2 = %d\n", x, x * x);
		fflush(stdout);
	}
	// close(STDOUT_FILENO);
	// close(STDIN_FILENO);
	return 0;
}
