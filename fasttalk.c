#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fasttalk.h"


#define EMMC_SIZE 0xEC000000
#define EMMC_IMAGE_FILE_LEN EMMC_SIZE 
#define IMAGE_PART_SIZE 0x8000000
#define IMAGE_PARTS_NUM ((EMMC_SIZE%IMAGE_PART_SIZE) == 0 ? (EMMC_SIZE/IMAGE_PART_SIZE):(EMMC_SIZE/IMAGE_PART_SIZE + 1))
#define LAST_PART_SIZE (EMMC_SIZE%IMAGE_PART_SIZE)
#define STUFF_NUM 0

static FASTTALK_STEP talk_step = STEP_BOOT;
static int file_count = 0;
static int stuff_count = 0;

const static char * feedback_table[] = {
"",
"System startup", //"Fastboot 3.3.0-dirty",
"fastboot# ",
"Bytes transferred = ",
"TFTP error:",
" blocks written: OK",
"mmc write failed",
};

const static char * command_table[] = {
"\x3",
"mw.b 90000000 0 8000000\n",
"tftp 90000000",
"mmc write 0 90000000",
"reset\n",
};

static FEEDBACK_T parse_line(char* line, unsigned char len)
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


static void send_cmd(int fd, const char* cmd)
{
	//printf("CMD:%s\n", cmd);
	write(fd, cmd, strlen(cmd));
}

int fasttalk(int fd, int out)
{
	char cmd[256];
	FASTTALK_STEP step;

	step = talk_step;


	if((1<<FT_TFTP_FAIL) & out){
		return -1;
	}

	if((1<<FT_MMC_WRITE_FAIL) & out){
		return -1;
	}

	if((1<<FT_WELCOME) & out){
		if(step == STEP_BOOT){
			step = STEP_SHELL_START;
			send_cmd(fd, command_table[CT_CTRL_C]);
		}		
	}

	if((1<<FT_TFTP_SUCCESS) & out){
		step = STEP_MMC_WRITE;
	}

	if((1<<FT_MMC_WRITE_SUCCESS) & out){
		if(step == STEP_MMC_WRITE){
			file_count++;
			if(file_count < IMAGE_PARTS_NUM){
				step = STEP_TFTP;
			}else{
				//No stuff, as image file is same size as emmc.
				//step = STEP_STUFF; 
				step = STEP_RESET;
			}
		}else if(step == STEP_STUFF_MMC_WRITE){

			stuff_count++;
			if(stuff_count < STUFF_NUM){
			}else {
				step = STEP_RESET;
			}
		}
	}

	if((1<<FT_SHELL_PROMPT) & out){
		switch (step) {
			case STEP_SHELL_START:
				step = STEP_TFTP;
				sprintf(cmd, "%s emmc-p%02d\n", command_table[CT_TFTP], file_count);
				send_cmd(fd, cmd);
				break;
			case STEP_TFTP:
				sprintf(cmd, "%s emmc-p%02d\n", command_table[CT_TFTP], file_count);
				send_cmd(fd, cmd);
				break;
			case STEP_MMC_WRITE:
				if(file_count+1 == IMAGE_PARTS_NUM){
					if(LAST_PART_SIZE == 0){
						sprintf(cmd, "%s %x 40000\n", command_table[CT_MMC_WRITE], file_count*0x40000);
					}else{
						sprintf(cmd, "%s %x %x\n", command_table[CT_MMC_WRITE], file_count*0x40000, LAST_PART_SIZE/0x200);
					}
				}else{
						sprintf(cmd, "%s %x 40000\n", command_table[CT_MMC_WRITE], file_count*0x40000);
				}
				send_cmd(fd, cmd);
				break;
			case STEP_STUFF_MW:
				send_cmd(fd, command_table[CT_MEM_SET]);
				usleep(100000);
				step = STEP_STUFF_MMC_WRITE;
				break;
			case STEP_STUFF_MMC_WRITE:
				sprintf(cmd, "%s %x 40000\n", command_table[CT_MMC_WRITE], EMMC_IMAGE_FILE_LEN + stuff_count*0x40000);
				send_cmd(fd, command_table[CT_MMC_WRITE]);
				break;
			case STEP_RESET:
				send_cmd(fd, command_table[CT_RESET]);
				return 1;
				break;
			default:
				break;
		}
	}

	talk_step = step;

	return 0;
}
