#ifndef _FASTTALK_H_
#define _FASTTALK_H_

#define LINE_MAX_LENGTH 250

typedef enum _FEEDBACK_T_
{
	FT_UNKNOWN = 		0,
	FT_WELCOME = 		1,
	FT_SHELL_PROMPT = 	2,
	FT_TFTP_SUCCESS = 	3,
	FT_TFTP_FAIL = 		4,
	FT_MMC_WRITE_SUCCESS = 	5,
	FT_MMC_WRITE_FAIL = 	6,
}FEEDBACK_T;


typedef enum _COMMAND_T_
{
	CT_CTRL_C = 0,
	CT_MEM_SET,
	CT_TFTP,
	CT_MMC_WRITE,
	CT_RESET,
}COMMAND_T;

typedef enum {
	STEP_BOOT,
	STEP_SHELL_START,
	STEP_DOWN_BURN,
	STEP_TFTP,
	STEP_MMC_WRITE,
	STEP_STUFF_MW,
	STEP_STUFF_MMC_WRITE,
	STEP_RESET
}FASTTALK_STEP;

int fasttalk(int fd, int out);
unsigned int parse_buffer(char* buf, unsigned char len, unsigned int* out);

#endif
