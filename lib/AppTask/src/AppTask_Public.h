#include <stdint.h>

/* App task parameters */
typedef struct _APP_TASK_PARAMS_T
{
    uint32_t ulTaskPriority;    /* Task priority */
} APP_TASK_PARAMS_T;

/* App task event */
typedef struct _APP_TASK_EVENT_T
{
    uint32_t        ulEvent;    /* Event type: APP_TASK_EVENTS_E */
    void*           pvData;     /* Event data */
    uint32_t        ulDataLen;  /* Event data length */
} APP_TASK_EVENT_T;

typedef enum _APP_TASK_EVENTS_E
{ 
    APP_TASK_EVENTS_EVENT_NONE = 0,
} APP_TASK_EVENTS_E;