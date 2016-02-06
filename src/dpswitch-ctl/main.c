#include <stdio.h>
#include <stdlib.h>

#define FIFO_FILE "dpswitch_ctl"

int main(int argc, char *argv[])
{
        FILE *fp;

        if (argc != 2) {
                printf("USAGE: dpswitch_ctl [args]\n");
                exit(1);
        }

        if((fp = fopen(FIFO_FILE, "w")) == NULL) {
                perror("fopen");
                exit(1);
        }
        fputs(argv[1], fp);
        fclose(fp);

        char readbuf[80];

		fp = fopen(FIFO_FILE, "r");
    	fread(readbuf, 1, 80, fp);
    	printf("%s\n", readbuf);

        fclose(fp);
        return(0);
}
