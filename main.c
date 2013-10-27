#define USE_STDPERIPH_DRIVER
#include "stm32f10x.h"
#include "stm32_p103.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Filesystem includes */
#include "io_set_serial.h"
#include "filesystem.h"
#include "fio.h"
#include "host.h"

#define MAX_SERIAL_STR 100
#define Command_Number 8

enum ALLcommand 
{
/*Add your command in here. */
    hello,
    echo,
    ps,
    help,
    host,
    cat,
    ls,
    mmtest,
};

struct STcommand
{
	char *name;
	int  size; 	
	char *info;
};
typedef struct STcommand CmdList;
CmdList CMD[]={
	   /*Add your command list in here. */

      [hello]   = {.name="hello"   , .size=5 , .info="Show 'Hello! how are you?'"},
      [echo]    = {.name="echo"    , .size=4 , .info="Show your input"},
      [ps]      = {.name="ps"      , .size=2 , .info="Report current processes"},
      [help]    = {.name="help"    , .size=4 , .info="Show command list."},
      [host]    = {.name="host"    , .size=4 , .info="Transmit command to host."},
      [cat]     = {.name="cat"     , .size=3 , .info="Show on the stdout"},
      [ls]      = {.name="ls"      , .size=2 , .info="Show directory under"},
      [mmtest]  = {.name="mmtest"  , .size=6 , .info="Report Memory Management test"},
};	



void mmtest_command(char *str)
{
	char set=1;
	char mm_str[MAX_SERIAL_STR];	
	Print("You will run & free allocated memory... [Y / N]?");
	Print_nextLine();
	Print("(ps) Input [Ctrl+c] to leave cycle when you select [Y].");	
	while(set){
		set=0;
		Read_Input(mm_str,MAX_SERIAL_STR);
		if(!strncmp(mm_str,"Y", 1) || !strncmp(mm_str,"y", 1)){
			mmtest_fio_function(str);
		}
		else if(!strncmp(mm_str,"N", 1) || !strncmp(mm_str,"n", 1)){
			Print("Leave mmtest!");
		}
		else{
			set=1;
			Print("Please input 'Y' & 'N'....");
		}
	}
}

void cat_command(char *str)
{
    char i;
    char c[20];
    char path[20]="/romfs/";
    char buff[100],tmp[7];
    buff[0]='\0';
    int count;
    if(strlen(str)==CMD[cat].size){
		Print("Please input: cat <file> (EX:cat test.txt)");
	}
	else if(str[CMD[cat].size]==' '){
		for(i=CMD[cat].size+1 ; i<strlen(str) ; i++){
				tmp[i- (CMD[cat].size+1) ]=str[i];
			}			
			strcat(path,tmp);
    		        int fd = fs_open(path, 0, O_RDONLY);
			if(fd<0){
      			        Print("No such this file.");
       			 }
       			 else{
          		        count = fio_read(fd, buff, sizeof(buff));
				char buff2[count-1];
				for(i=0;i<count-1;i++)
				buff2[i]=buff[i];
				Print(buff2);
 			} 
	}
	else{
		Print("Error!");
	}

}

void ls_command(char *str)
{
    char ls_buff[128];
    int fileNum;
    ls_buff[0]='\0';
    fileNum=getAllFileName("/romfs/",ls_buff);
    Print(ls_buff);
}

void host_command(char *str)
{
	char host_tmp[strlen(str)];
	char i=0;
	if(strlen(str)==CMD[host].size){
		Print("Please input: host <command>");
	}
	else if(str[CMD[host].size]==' '){
		for(i=CMD[host].size+1;i<strlen(str);i++){
				host_tmp[i- (CMD[host].size+1) ]=str[i];
			}
			host_system(host_tmp, strlen(host_tmp));
			Print("OK! You have transmitted the command to semihost.");
	}
	else{
		Print("Error!");
	}
}

void ShellTask_Command(char *str)
{		
	char tmp[20];
	char i;
	char c[40];
	if(!strncmp(str,"hello", 5)) {           
		Print("Hello! how are you?");
	}
	else if(!strncmp(str,"echo", 4)){
		if(strlen(str)==4){
			Print_nextLine();
		}
		if(str[4]==' '){
			for(i=5;i<strlen(str);i++){
				tmp[i-5]=str[i];
			}
			Print(tmp);
		}
	}
	else if(!strncmp(str,"ps",2)){
		char title[]="Name\t\t\b\bState\t\b\b\bPriority\t\bStack\t\bNum";
		Puts(title);
		char catch[50];
		vTaskList(catch);
		Print(catch);
	}
	else if(!strncmp(str,"help", 4)) {           
		sprintf(c,"You can use %d command in the freeRTOS",Command_Number);
		Print(c);
		Print_nextLine();
		for(i=0;i<Command_Number;i++){
		Puts(CMD[i].name);
		Puts("\t-- ");
		Puts(CMD[i].info);
		Print_nextLine();
		}
	}
	else if(!strncmp(str,"host",4)){
		host_command(str);
	}
	else if(!strncmp(str,"cat",3)){
		cat_command(str);
	}
	else if(!strncmp(str,"ls",2)){
		ls_command(str);
	}
	else if(!strncmp(str,"mmtest",6)){
		mmtest_command(str);
	}
	else{
		Print("Command not found, please input 'help'");
	}
}
void Shell()
{
	char str[MAX_SERIAL_STR];
	char pos[] = "zxc2694's RTOS~$ ";
	char newLine[] = "\n\r";
	while (1)
    {
        fio_write(1, pos, strlen(pos));
	Read_Input(str,MAX_SERIAL_STR);
	/*This is my shell command*/
	ShellTask_Command(str);
    }
}


int main()
{
	Init_Serial();

	/* Create a task to receive char from the RS232 port. */
	xTaskCreate(Shell,
	            (signed portCHAR *) "Shell",
	            512 /* stack size */, NULL, tskIDLE_PRIORITY + 5, NULL);
	
	/* Start running the tasks. */
	vTaskStartScheduler();

	return 0;
}

void vApplicationTickHook()
{
}
