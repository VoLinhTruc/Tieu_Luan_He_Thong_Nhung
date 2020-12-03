#include <lpc213x.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

void initClocks(void);
void initTimer0(void);
void delay_ms(unsigned int counts);

void initUART0(void);
void U0Send(char data);
void U0SendString(char* StringPtr);
char U0Receive(void);
void U0ReceiveString(char* result, int num_of_char);

void initADC (void);
unsigned int ADCRead(void);

void analogTask(void *p);
void uartTask(void *p);
void gpioTask(void *p);

xQueueHandle analogUartQueue;
xQueueHandle analogGpioQueue;

// ------------------------------------------------------------------------------------

int main(void)
{
  initClocks(); // Set CCLK=60Mhz and PCLK=60Mhz 
	initUART0();
	initADC();
	
	IO0DIR |= (1<<14);
	IO0DIR |= (1<<15);

	analogUartQueue=xQueueCreate(1,sizeof(char*));
	analogGpioQueue=xQueueCreate(1,sizeof(char*));
	
	if(analogUartQueue!=NULL) 
	{
		xTaskCreate(analogTask,"analog",128,NULL,1,NULL);
		xTaskCreate(uartTask,"read",128,NULL,1,NULL);
		xTaskCreate(gpioTask,"read",128,NULL,1,NULL);
		
		vTaskStartScheduler();
	}
}

// ------------------------------------------------------------------------------------

void initClocks(void)
{
  PLLCON = 0x01;
  PLLCFG = 0x24; 
  PLLFEED = 0xAA;
  PLLFEED = 0x55;
  
  while(!(PLLSTAT & 0x00000400));
    
  PLLCON = 0x03;
  PLLFEED = 0xAA;
  PLLFEED = 0x55;
  VPBDIV = 0x01;
}

// --------------------------------------------------------------

void initTimer0(void)
{
    T0CTCR = 0x0; 
    T0PR = 59999;

    T0TCR = 0x02;
}

// --------------------------------------------------------------

void delay_ms(unsigned int counts) //Using Timer0
{
		T0TC = 0;
    T0TCR = 0x02;
    T0TCR = 0x01;
   
    while(T0TC < counts);
   
    T0TCR = 0x00;
}

// --------------------------------------------------------------

void initUART0(void)
{
  PINSEL0 = 0x5;
  U0LCR = 0x83;
  U0DLL = 134;
  U0DLM = 1;   
  U0LCR &= 0x0F; 
}

// --------------------------------------------------------------

void U0Send(char data)
{
  while (!(U0LSR & (1<<5))){};

  U0THR = data;
}

// --------------------------------------------------------------

void U0SendString(char* StringPtr){
  while(*StringPtr != 0x00)
  {
    U0Send(*StringPtr);
    StringPtr++;
  }
}

// --------------------------------------------------------------

char U0Receive()
{
 char ch; 
		while (!(U0LSR & 1)){};
    ch = U0RBR;                               
 return ch;
}

// --------------------------------------------------------------

void U0ReceiveString(char* result, int num_of_char)
{
	int i = 0;
	for(i = 0; i < num_of_char; i++)
	{
		result[i] = U0Receive();
	}
}

// --------------------------------------------------------------

void initADC (void)
{
  PINSEL1 = 0x01000000; // P0.28, AD0.1
}

// --------------------------------------------------------------

unsigned int ADCRead()
{
  unsigned int value;
  AD0CR = 0x01200302 ;

  
  while(!((value = AD0GDR) & 0x80000000))
  {
		
  }
  return((value >> 6) & 0x3ff) ;
}

// --------------------------------------------------------------

#define NORMAL 0
#define ALERT 1

void analogTask(void *p)
{
	int upper_limit = 80;
  int current_temperature = 0;
  int current_state = NORMAL;
	char *str_to_uart;
	char *str_to_gpio;

  xQueueReceive(analogUartQueue, &str_to_uart, -1);
  sscanf(str_to_uart, "%d", &upper_limit);
  vPortFree(str_to_uart);
	
  while(1) 
	{
    current_temperature = ADCRead()/10;

    if(current_temperature > upper_limit)
    {
      if (current_state == NORMAL)
      {
        vTaskDelay(10);
        current_temperature = ADCRead()/10;

        if(current_temperature > upper_limit)
        {
          str_to_gpio = (char*)pvPortMalloc(2);
          sprintf(str_to_gpio, "%d", 1);
          xQueueSendToBack(analogGpioQueue, &str_to_gpio, 0);
          
          str_to_uart = (char*)pvPortMalloc(20);
          sprintf(str_to_uart, "There are fire\r\n");
          xQueueSendToBack(analogUartQueue, &str_to_uart, 0);

          current_state = ALERT;
        }
      }
    }
    else
    {
      if (current_state == ALERT)
      {
        str_to_gpio = (char*)pvPortMalloc(2);
        sprintf(str_to_gpio, "%d", 0);
        xQueueSendToBack(analogGpioQueue, &str_to_gpio, 0);
        
        current_state = NORMAL;
      }
    }
		
		vTaskDelay(1);
  }
}

// --------------------------------------------------------------

void uartTask(void *p)
{
  char *str;

  U0SendString("Get temperature\r\n");
  str = (char*)pvPortMalloc(4);
  U0ReceiveString(str, 3);
  str[3] = 0;
  xQueueSendToBack(analogUartQueue, &str,0);
  vTaskDelay(10);
	
  while(1) 
	{
    xQueueReceive(analogUartQueue, &str, -1);
    U0SendString(str);
    vPortFree(str);

		vTaskDelay(1);
  }
}

// --------------------------------------------------------------

void gpioTask(void *p)
{
	int value = 0;
  char *str;

  while(1) 
	{
    xQueueReceive(analogGpioQueue, &str, -1);
    sscanf(str, "%d", &value);

    if(value)
    {
      IO0PIN |= (1<<14);
      IO0PIN |= (1<<15);
    }
    else
    {
      IO0PIN &= ~(1<<14);
      IO0PIN &= ~(1<<15);
    }

    vPortFree(str);
		
		vTaskDelay(1);
  }
}

// --------------------------------------------------------------
