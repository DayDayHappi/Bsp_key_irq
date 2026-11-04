//********************************INCLUDES********************************** */
#include "bsp_key_irq.h"
#include <stdio.h>

//********************************DEFINES*********************************** */

  /* The key_Queue_Internal transfers button-edge events from the ISR to
   * key_task_func() for processing.
   */
static QueueHandle_t key_Queue_Internal;
  /* key_Queue_External.
   * Outputs button-press events (short press, long press) to external apps.
   * Apps that need the button state receive messages from this queue.
   */
static QueueHandle_t key_Queue_External;

// to count the number of dropped events for watchdog or debug
static volatile uint32_t s_drop_count;

// the state machine only needs to know whether the key is currently held down
typedef struct
{
    BaseType_t            pressed;
    TickType_t            press_tick;
}key_sm_t;

//********************************LOCAL FUNC******************************* */
static BaseType_t key_edge_is_valid( key_edge_t edge )
{
    return ( ( KEY_Falling == edge ) || ( KEY_Rising == edge ) ) ? pdTRUE : pdFALSE;
}

/* Pure logic: consume one edge, produce at most one press event.
 * No queue, no blocking, no tick source -> directly testable.
 * Returns pdTRUE when *out_press has been filled in. */
static BaseType_t key_sm_process( key_sm_t *sm,
                                  const key_edge_event_t *evt,
                                  key_press_event_t *out_press )
{
    TickType_t duration;

    if( KEY_Falling == evt->key_edge )
    {
        if( pdFALSE == sm->pressed )
        {
            // valid press: remember when the key went down
            sm->pressed    = pdTRUE;
            sm->press_tick = evt->tick;
        }
        // else: a second falling edge while already pressed is contact bounce,
        //       keep the original press_tick and ignore it
        return pdFALSE;
    }

    if( pdFALSE == sm->pressed )
    {
        // a rising edge without a preceding falling edge: nothing to report
        printf("[key][error]first edge is rising\r\n");
        return pdFALSE;
    }

    sm->pressed = pdFALSE;

    /* unsigned subtraction, stays correct across tick wrap-around */
    duration = evt->tick - sm->press_tick;

    // shorter than the filter time -> contact bounce, not a real press
    if( duration < KEY_FILTER_TICKS )
    {
        printf("[key][filter]invalid data\r\n");
        return pdFALSE;
    }

    /* Two tiers, no gap: anything from the filter threshold up to the long
     * threshold is a short press. */
    out_press->key_press_status = ( duration < KEY_LONG_TICKS ) ? KEY_Short_Press
                                                                : KEY_Long_Press;
    return pdTRUE;
}

//********************************PUBLIC FUNC****************************** */
// A func to push data to key_Queue_Internal for ISR
BaseType_t key_isr_send_edge( key_edge_t edge )
{
    BaseType_t woken = pdFALSE;

    if( ( NULL == key_Queue_Internal ) || ( pdFALSE == key_edge_is_valid( edge ) ) )
    {
        return pdFALSE;
    }

    key_edge_event_t evt = {
        .key_edge = edge,
        .tick     = xTaskGetTickCountFromISR()
    };
    if( xQueueSendFromISR( key_Queue_Internal, &evt, &woken ) != pdPASS )
    {
        s_drop_count++;
    }
    return woken;               // goes straight into portYIELD_FROM_ISR()
}

key_status_t key_send_edge_at( key_edge_t edge, TickType_t tick )
{
    if( pdFALSE == key_edge_is_valid( edge ) )
    {
        return KEY_ErrorParam;
    }
    if( NULL == key_Queue_Internal )
    {
        return KEY_Resource_Error;
    }

    key_edge_event_t evt = {
        .key_edge = edge,
        .tick     = tick
    };
    /* Same FIFO order as the ISR path. Never blocks: a test harness should see
     * a full queue as a failure instead of hanging the task. */
    if( xQueueSend( key_Queue_Internal, &evt, ( TickType_t ) 0 ) != pdPASS )
    {
        s_drop_count++;
        return KEY_Resource_Error;
    }
    return KEY_OK;
}

key_status_t key_send_edge( key_edge_t edge )
{
    return key_send_edge_at( edge, xTaskGetTickCount() );
}

key_status_t key_get_event( key_press_event_t *evt, TickType_t timeout )
{
    if( NULL == evt )
    {
        return KEY_ErrorParam;
    }
    if( NULL == key_Queue_External )
    {
        return KEY_Resource_Error;
    }
    return ( xQueueReceive( key_Queue_External, evt, timeout ) == pdTRUE ) ? KEY_OK
                                                                          : KEY_TimeOut;
}

uint32_t key_get_drop_count(void)
{
    return s_drop_count;
}

//before os scheduler
key_status_t key_queue_init(void)
{
    /* Guard against a second call (e.g. from a re-init path): creating another
     * pair of queues would leak the previous handles. */
    if( NULL != key_Queue_Internal )
    {
        return KEY_OK;
    }

    key_Queue_Internal = xQueueCreate( KEY_QUEUE_DEPTH, sizeof( key_edge_event_t ) );
    if( NULL == key_Queue_Internal )
    {
        printf("key_Queue_Internal create failed\r\n");
        return KEY_Resource_Error;
    }

    key_Queue_External = xQueueCreate( KEY_QUEUE_DEPTH, sizeof( key_press_event_t ) );
    if( NULL == key_Queue_External )
    {
        printf("key_Queue_External create failed\r\n");
        vQueueDelete( key_Queue_Internal );
        key_Queue_Internal = NULL;
        return KEY_Resource_Error;
    }

    printf("key queues create successfully\r\n");
    return KEY_OK;
}

void key_task_func(void *pvParameters)
{
    (void)pvParameters;

    configASSERT( NULL != key_Queue_Internal );

    // variable in stack
    key_edge_event_t      key_edge_event             = {0};
    key_press_event_t     key_press_event            = {0};
    key_sm_t              key_sm                     = {0};

    for(;;)
    {
        //1. block until the ISR (or an injected edge) delivers something
        if( xQueueReceive( key_Queue_Internal, &key_edge_event, portMAX_DELAY ) != pdTRUE )
        {
            continue;
        }

        //2. update the status machine; at most one press event comes out
        if( pdFALSE == key_sm_process( &key_sm, &key_edge_event, &key_press_event ) )
        {
            continue;
        }

        //3. publish to the upper layer, never block the key task on a slow app
        if( xQueueSend( key_Queue_External, &key_press_event, ( TickType_t ) 0 ) != pdPASS )
        {
            s_drop_count++;
            printf("key_Queue_External send failed\r\n");
            continue;
        }

        printf( ( KEY_Short_Press == key_press_event.key_press_status )
                ? "this is a short pressed\r\n"
                : "this is a long pressed\r\n" );
    }
}
