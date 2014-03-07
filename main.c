#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include "fasttalk.h"

/* baudrate settings are defined in <asm/termbits.h>, which is
   included by <termios.h> */
#define BAUDRATE B115200
/* change this definition for the correct port */
#define MODEMDEVICE "/dev/ttyUSB0"
#define _POSIX_SOURCE 1 /* POSIX compliant source */

#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE; 

void print_usage(void)
{
	printf("fasttalk <tty device name>\n");
}

int main(int argc, char **argv)
{
	int fd, c, res;
	struct termios oldtio,newtio;
	char buf[255];
	unsigned int feeds;
	unsigned char i;
	unsigned char parsed;
	unsigned char left;
	unsigned char data_len;
	int fret;
	
	if(argc != 2){
		print_usage();
		exit(0);
	}

	/* 
	   Open modem device for reading and writing and not as controlling tty
	   because we don't want to get killed if linenoise sends CTRL-C.
	 */
	fd = open(argv[1], O_RDWR | O_NOCTTY ); 
	if (fd <0) {perror(argv[1]); exit(-1); }

	tcgetattr(fd,&oldtio); /* save current port settings */
        
        bzero(&newtio, sizeof(newtio));
        //newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
        newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
        newtio.c_iflag = IGNPAR;
        newtio.c_oflag = 0;
        
        /* set input mode (non-canonical, no echo,...) */
        newtio.c_lflag = 0;
         
        newtio.c_cc[VTIME]    = 10;   /* inter-character timer unused */
        newtio.c_cc[VMIN]     = LINE_MAX_LENGTH;   /* blocking read until 5 chars received */
        
        tcflush(fd, TCIFLUSH);
        tcsetattr(fd,TCSANOW,&newtio);

	/*
	   terminal settings done, now handle input
	   In this example, inputting a 'z' at the beginning of a line will 
	   exit the program.
	 */
	left = 0;
	while (STOP==FALSE) {     /* loop until we have a terminating condition */
		/* read blocks program execution until a line terminating character is 
		   input, even if more than 255 chars are input. If the number
		   of characters read is smaller than the number of chars available,
		   subsequent reads will return the remaining chars. res will be set
		   to the actual number of characters actually read */
		res = read(fd, buf + left, LINE_MAX_LENGTH - left); 
		if(res > 0){
			data_len = left + res;
			buf[data_len]=0;             /* set end of string, so we can printf */
			//only print new received chars.
			printf("%s", buf+left);
			//printf("\n: all = %d, old = %d, new=%d\n", data_len, left, res);
			parsed = parse_buffer(buf, data_len, &feeds);
			//printf(":parsed=%d, left=%d\n", parsed, data_len - parsed);

			//prepare next time read.
			left = data_len - parsed;
			if(left > 0){
				for(i = 0; i < left; i++){
					buf[i] = buf[parsed + i];
				}
			}

			if(feeds != 0 && feeds != (1 << FT_UNKNOWN)){
				fret = fasttalk(fd, feeds);
				if(fret < 0) {
					printf("\n\nemmc burning is break!\n");
					goto LOCAL_EXIT;
				}else if (fret > 0){
					printf("\n\nFinish emmc burning successfully!\n");
					goto LOCAL_EXIT;
				}else{
					//printf("\nnew_step=%d\n", new_step);
				}
			}
		}
	}

LOCAL_EXIT:
	/* restore the old port settings */
	tcsetattr(fd,TCSANOW,&oldtio);
}
