#include "ft.h"

#include "FreeRTOS.h"
#include "task.h"

__attribute__((section(".freertos_heap"), aligned(8)))
u8 ucHeap[configTOTAL_HEAP_SIZE];

void vApplicationGetRandomHeapCanary(portPOINTER_SIZE_TYPE *pxHeapCanary) {
	*pxHeapCanary = (portPOINTER_SIZE_TYPE)ucHeap ^ 0x6b8b4567u;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name) {
	(void)task;
	(void)task_name;

	taskDISABLE_INTERRUPTS();
	while (1)
		;
}
