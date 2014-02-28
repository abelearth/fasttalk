#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fasttalk.h"

char * feedback_table[] = {
"",
"System startup", //"Fastboot 3.3.0-dirty",
"fastboot# ",
"Bytes transferred = 536870912 (20000000 hex)",
"TFTP error:",
"262144 blocks written: OK",
"mmc write failed",
};

char * command_table[] = {
"\x3",
"tftp 90000000 emmc-pa\n",
"mmc write 0 90000000 0 40000\n",
"mmc write 0 98000000 40000 40000\n",
"mmc write 0 a0000000 80000 40000\n",
"mmc write 0 a8000000 c0000 40000\n",
"tftp 90000000 emmc-pb\n",
"mmc write 0 90000000 100000 40000\n",
"mmc write 0 98000000 140000 40000\n",
"mmc write 0 a0000000 180000 40000\n",
"mmc write 0 a8000000 1c0000 40000\n",
"tftp 90000000 emmc-pc\n",
"mmc write 0 90000000 200000 40000\n",
"mmc write 0 98000000 240000 40000\n",
"mmc write 0 a0000000 280000 40000\n",
"mmc write 0 a8000000 2c0000 40000\n",
"reset\n",
};


FEEDBACK_T parse_line(char* line, unsigned char len)
{
	FEEDBACK_T feed = 0;
	
	//throw 0 prefix line.
	while(*line == 0 && len > 0){
		line++;
		len--;
	}

	if(len == 0){
		return FT_UNKNOWN;
	}

	if(strstr(line, feedback_table[FT_WELCOME]) != NULL){
		feed = FT_WELCOME;
	} else if(strstr(line, feedback_table[FT_TFTP_SUCCESS]) != NULL){
		feed = FT_TFTP_SUCCESS;
	} else if(strstr(line, feedback_table[FT_TFTP_FAIL]) != NULL){
		feed = FT_TFTP_FAIL;
	} else if(strstr(line, feedback_table[FT_MMC_WRITE_SUCCESS]) != NULL){
		feed = FT_MMC_WRITE_SUCCESS;
	} else if(strstr(line, feedback_table[FT_MMC_WRITE_FAIL]) != NULL){
		feed = FT_MMC_WRITE_FAIL;
	}else if(strcmp(line, feedback_table[FT_SHELL_PROMPT]) == 0){
		feed = FT_SHELL_PROMPT;
	}else{
		feed = FT_UNKNOWN;
	}

	return feed;
	
}

unsigned int parse_buffer(char* buf, unsigned char len, unsigned int* out)
{
	unsigned char i = 0;
	unsigned char start = 0;
	unsigned char end = 0;
	
	unsigned int feeds = 0;
	unsigned int parsed = 0;

	while (i < len){
		if(buf[i] == '\n'){
			end = i;
			buf[i] = 0;
			if((end - start) > 0){
				feeds |= (1 << parse_line(buf + start, end - start));
			}
			start = i + 1;
		}
		i++;
	}

	parsed = end+1;

	if (strlen(feedback_table[FT_SHELL_PROMPT]) == (len - parsed)){
		if(strcmp(buf + parsed, feedback_table[FT_SHELL_PROMPT]) == 0){
			//shell prompt
			feeds |= (1 << FT_SHELL_PROMPT);
			parsed = len; 
		}
	}

	*out = feeds;

	if(parsed == 0){
		if(len == LINE_MAX_LENGTH){
			printf("Warning: no line break in whole receive buffer!\n");
			printf("Warning: Throw away!\n");
			parsed = len;
		}
	}

	return parsed;
}

int fasttalk(int fd, int out, FASTTALK_STEP step, FASTTALK_STEP *new_step ) 
{
	*new_step = step;

	if((1<<FT_WELCOME) & out){
		if(step == STEP_BOOT){
			step = STEP_SHELL_START;
			write(fd, command_table[CT_CTRL_C], strlen(command_table[CT_CTRL_C]));
		}		
	}

	if((1<<FT_TFTP_FAIL) & out){
		return -1;
	}

	if((1<<FT_MMC_WRITE_FAIL) & out){
		return -1;
	}

	if((1<<FT_TFTP_SUCCESS) & out){
		if(step == STEP_TFTP_PA){
			step = STEP_WRITE_PA_0;
		}else if(step == STEP_TFTP_PB){
			step = STEP_WRITE_PB_0;
		}else if(step == STEP_TFTP_PC){
			step = STEP_WRITE_PC_0;
		}
	}

	if((1<<FT_MMC_WRITE_SUCCESS) & out){
		if(step == STEP_WRITE_PA_0){
			step = STEP_WRITE_PA_1;
		}else if(step == STEP_WRITE_PA_1){
			step = STEP_WRITE_PA_2;
		}else if(step == STEP_WRITE_PA_2){
			step = STEP_WRITE_PA_3;
		}else if(step == STEP_WRITE_PA_3){
			step = STEP_TFTP_PB;
		}else if(step == STEP_WRITE_PB_0){
			step = STEP_WRITE_PB_1;
		}else if(step == STEP_WRITE_PB_1){
			step = STEP_WRITE_PB_2;
		}else if(step == STEP_WRITE_PB_2){
			step = STEP_WRITE_PB_3;
		}else if(step == STEP_WRITE_PB_3){
			step = STEP_TFTP_PC;
		}else if(step == STEP_WRITE_PC_0){
			step = STEP_WRITE_PC_1;
		}else if(step == STEP_WRITE_PC_1){
			step = STEP_WRITE_PC_2;
		}else if(step == STEP_WRITE_PC_2){
			step = STEP_WRITE_PC_3;
		}else if(step == STEP_WRITE_PC_3){
			step = STEP_RESET;
		}
	}

	if((1<<FT_SHELL_PROMPT) & out){
		switch (step) {
			case STEP_SHELL_START:
				step = STEP_TFTP_PA;
				write(fd, command_table[CT_TFTP_PA], strlen(command_table[CT_TFTP_PA]));
				break;
			case STEP_TFTP_PA:
				write(fd, command_table[CT_TFTP_PA], strlen(command_table[CT_TFTP_PA]));
				break;
			case STEP_WRITE_PA_0:
				write(fd, command_table[CT_WRITE_PA_0], strlen(command_table[CT_WRITE_PA_0]));
				break;
			case STEP_WRITE_PA_1:
				write(fd, command_table[CT_WRITE_PA_1], strlen(command_table[CT_WRITE_PA_1]));
				break;
			case STEP_WRITE_PA_2:
				write(fd, command_table[CT_WRITE_PA_2], strlen(command_table[CT_WRITE_PA_2]));
				break;
			case STEP_WRITE_PA_3:
				write(fd, command_table[CT_WRITE_PA_3], strlen(command_table[CT_WRITE_PA_3]));
				break;
			case STEP_TFTP_PB:
				write(fd, command_table[CT_TFTP_PB], strlen(command_table[CT_TFTP_PB]));
				break;
			case STEP_WRITE_PB_0:
				write(fd, command_table[CT_WRITE_PB_0], strlen(command_table[CT_WRITE_PB_0]));
				break;
			case STEP_WRITE_PB_1:
				write(fd, command_table[CT_WRITE_PB_1], strlen(command_table[CT_WRITE_PB_1]));
				break;
			case STEP_WRITE_PB_2:
				write(fd, command_table[CT_WRITE_PB_2], strlen(command_table[CT_WRITE_PB_2]));
				break;
			case STEP_WRITE_PB_3:
				write(fd, command_table[CT_WRITE_PB_3], strlen(command_table[CT_WRITE_PB_3]));
				break;
			case STEP_TFTP_PC:
				write(fd, command_table[CT_TFTP_PC], strlen(command_table[CT_TFTP_PC]));
				break;
			case STEP_WRITE_PC_0:
				write(fd, command_table[CT_WRITE_PC_0], strlen(command_table[CT_WRITE_PC_0]));
				break;
			case STEP_WRITE_PC_1:
				write(fd, command_table[CT_WRITE_PC_1], strlen(command_table[CT_WRITE_PC_1]));
				break;
			case STEP_WRITE_PC_2:
				write(fd, command_table[CT_WRITE_PC_2], strlen(command_table[CT_WRITE_PC_2]));
				break;
			case STEP_WRITE_PC_3:
				write(fd, command_table[CT_WRITE_PC_3], strlen(command_table[CT_WRITE_PC_3]));
				break;
			case STEP_RESET:
				write(fd, command_table[CT_RESET], strlen(command_table[CT_RESET]));
				return 1;
				break;
			default:
				break;
		}
	}

	*new_step = step;

	return 0;
}
