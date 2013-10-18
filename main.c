#define USE_STDPERIPH_DRIVER
#include "stm32f10x.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <string.h>

/* Filesystem includes */
#include "filesystem.h"
#include "fio.h"

#define MAX_SERIAL_STR 100
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
void Shell()
{
	char str[MAX_SERIAL_STR];
	char ch;

	int curr_char;
	int done;
	char pos[] = "zxc2694's RTOS~$ ";
	char newLine[] = "\n\r";

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

			/* If the byte is an end-of-line type character, then
			 * finish the string and inidcate we are done.
			 */
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
		if(!strncmp(str,"hello", 5)) {           
		fio_write(1, "Hello! how are you?\n\r\0", 20);
		}
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
