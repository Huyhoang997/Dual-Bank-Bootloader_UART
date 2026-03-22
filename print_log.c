#include "stdarg.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

char buffer[50];

void mPrintf(const char *format,...) 
{
	va_list args;
	va_start (args, format);
	vsnprintf(buffer , sizeof(buffer), format, args);
	va_end (args);
	HAL_UART_Transmit(&huart1, (uint8_t*)buffer, strlen(buffer), 1000);
}