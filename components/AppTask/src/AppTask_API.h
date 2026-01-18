#include <stdint.h>
#include "AppTask_Public.h"


/* App task handler */
typedef struct _APP_TASK_RSC_T*     APP_TASK_H;


int32_t 
AppTask_Create(APP_TASK_PARAMS_T* ptParams, APP_TASK_H* phAppTask);
