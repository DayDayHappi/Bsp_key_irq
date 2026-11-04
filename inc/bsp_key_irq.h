#ifndef BSP_KEY_IRQ_H
#define BSP_KEY_IRQ_H

//********************************INCLUDES********************************** */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

//********************************DEFINES*********************************** */
#define KEY_QUEUE_DEPTH      10u

#define KEY_FILTER_TIME_MS   20
#define KEY_SHORT_TIME_MS    500     /* reserved: no separate medium-press tier yet */
#define KEY_LONG_TIME_MS     1000

/* Thresholds are converted to ticks once, so the state machine always compares
 * tick against tick. Comparing a raw tick against a *_MS constant is only
 * correct when configTICK_RATE_HZ happens to be exactly 1000. */
#define KEY_FILTER_TICKS     pdMS_TO_TICKS( KEY_FILTER_TIME_MS )
#define KEY_LONG_TICKS       pdMS_TO_TICKS( KEY_LONG_TIME_MS )

typedef enum
{
    KEY_OK                = 0,
    KEY_TimeOut           = 1,
    KEY_ErrorParam        = 2,
    KEY_Resource_Error    = 3,
    KEY_Default           = 0xFF,
}key_status_t;

typedef enum
{
    KEY_Falling           = 0,
    KEY_Rising            = 1,
    KEY_edge_Default      = 0xff,
}key_edge_t;

typedef enum
{
    KEY_Short_Press       = 0,
    KEY_Long_Press        = 1,
    KEY_Press_Default     = 0xff,
}key_press_t;

//  Case status is provided to the upper-layer app
typedef struct
{
    key_press_t           key_press_status;
}key_press_event_t;

// internal data structure for button logic decision-making
typedef struct
{
    key_edge_t            key_edge;
    TickType_t            tick;          /* follows configUSE_16_BIT_TICKS */
}key_edge_event_t;


//********************************DECLARING********************************* */
/* Create the queues. Call once, before the scheduler starts and before the key
 * GPIO interrupt is enabled, otherwise the ISR has nowhere to send its edges. */
key_status_t   key_queue_init(void);

/* Task entry point, pass to xTaskCreate(). */
void           key_task_func(void *pvParameters);

/* ISR path: report one edge. The return value goes to portYIELD_FROM_ISR(). */
BaseType_t     key_isr_send_edge( key_edge_t edge );

/* Task-context path, same FIFO semantics as the ISR path. Use these from an
 * ordinary task (e.g. a unit test harness) instead of xQueueSendFromISR(),
 * which is only legal inside an ISR context. */
key_status_t   key_send_edge( key_edge_t edge );
key_status_t   key_send_edge_at( key_edge_t edge, TickType_t tick );  /* injected tick */

/* App path: wait up to timeout ticks for one press event. */
key_status_t   key_get_event( key_press_event_t *evt, TickType_t timeout );

/* Number of edges dropped because the internal queue was full. */
uint32_t       key_get_drop_count(void);

#endif  // end of BSP_KEY_IRQ_H
