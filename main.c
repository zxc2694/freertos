#define USE_STDPERIPH_DRIVER
#include "stm32f10x.h"
#include "stm32_p103.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <string.h>

/* Filesystem includes */
#include "filesystem.h"
#include "fio.h"
#include "host.h"

#define MAX_SERIAL_STR 100
#define Command_Number 6

enum ALLcommand 
{
/*Add your command in here. */
    hello,
    echo,
    ps,
    help,
    host,
    cat,
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

	[hello]={.name="hello", .size=5 , .info="Show 'Hello! how are you?'"},
	[echo] ={.name="echo" , .size=4 , .info="Show your input"},
	[ps]   ={.name="ps"   , .size=2 , .info="Report current processes"},
	[help] ={.name="help" , .size=4 , .info="Show command list."},
	[host] ={.name="host" , .size=4 , .info="Transmit command to host."},
	[cat]  ={.name="cat"  , .size=3 , .info="Show on the stdout"}
};	


extern const char _sromfs;
static void setup_hardware();
volatile xSemaphoreHandle serial_tx_wait_sem = NULL;
volatile xQueueHandle serial_rx_queue = NULL;

/* IRQ handler to handle USART2 interruptss (both transmit and receive
 * interrupts). */
void USART2_IRQHandler()
{
	static signed portBASE_TYPE xHigherPriorityTaskWoken;
	char rx_msg;
	/* If this interrupt is for a transmit... */
	if (USART_GetITStatus(USART2, USART_IT_TXE) != RESET) {
		/* "give" the serial_tx_wait_sem semaphore to notfiy processes
		 * that the buffer has a spot free for the next byte.
		 */
		xSemaphoreGiveFromISR(serial_tx_wait_sem, &xHigherPriorityTaskWoken);

		/* Diables the transmit interrupt. */
		USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
		/* If this interrupt is for a receive... */
	}
        else if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
                /* Receive the byte from the buffer. */
                rx_msg = USART_ReceiveData(USART2);

                /* Queue the received byte. */
                if(!xQueueSendToBackFromISR(serial_rx_queue, &rx_msg, &xHigherPriorityTaskWoken)) {
                        /* If there was an error queueing the received byte,
                         * freeze. */
                        while(1);
                }
        }

	else {
		/* Only transmit and receive interrupts should be enabled.
		 * If this is another type of interrupt, freeze.
		 */
		while(1);
	}

	if (xHigherPriorityTaskWoken) {
		taskYIELD();
	}
}

void send_byte(char ch)
{
	/* Wait until the RS232 port can receive another byte (this semaphore
	 * is "given" by the RS232 port interrupt when the buffer has room for
	 * another byte.
	 */
	while (!xSemaphoreTake(serial_tx_wait_sem, portMAX_DELAY));

	/* Send the byte and enable the transmit interrupt (it is disabled by
	 * the interrupt).
	 */
	USART_SendData(USART2, ch);
	USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
}

char receive_byte()
{
	char msg;

	/* Wait for a byte to be queued by the receive interrupts handler. */
	while (!xQueueReceive(serial_rx_queue, &msg, portMAX_DELAY));
	return msg;
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
	else{
		Print("Command not found, please input 'help'");
	}
}
void Shell()
{
	char str[MAX_SERIAL_STR];
	char ch;
	char pos[] = "zxc2694's RTOS~$ ";
	char newLine[] = "\n\r";
	int curr_char, done;
	while (1)
    {
        fio_write(1, pos, strlen(pos));
		curr_char = 0;
		done = 0;
		str[curr_char] = '\0';
		do
        {
			/* Receive a byte from the RS232 port (this call will
			 * block). */
            ch=receive_byte();

			if (curr_char >= MAX_SERIAL_STR-1 || (ch == '\r') || (ch == '\n'))
            {
				str[curr_char] = '\0';
				done = -1;
				/* Otherwise, add the character to the
				 * response string. */
			}
			else if(ch == 127)//press the backspace key
            {
                if(curr_char!=0)
                {
                    curr_char--;
                    fio_write(1,"\b \b", 3);
                }
            }
            else if(ch == 27)//press up, down, left, right, home, page up, delete, end, page down
            {
                ch=receive_byte();
                if(ch != '[')
                {
                    str[curr_char++] = ch;
                    fio_write(1, &ch, 1);
                }
                else
                {
                    ch=receive_byte();
                    if(ch >= '1' && ch <= '6')
                    {
                        ch=receive_byte();
                    }
                }
            }
			else
            {
				str[curr_char++] = ch;
				fio_write(1, &ch, 1);
			}
		} while (!done);
        fio_write(1, newLine, strlen(newLine));
        if(curr_char>0){
		/*This is my shell command*/
		ShellTask_Command(str);
	}
    }
}


int main()
{
	init_rs232();
	enable_rs232_interrupts();
	enable_rs232();
	
	fs_init();
	fio_init();
	register_romfs("romfs", &_sromfs);
	/* Create the queue used by the serial task.  Messages for write to
	 * the RS232. */
	vSemaphoreCreateBinary(serial_tx_wait_sem);
	serial_rx_queue = xQueueCreate(1, sizeof(char));

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
