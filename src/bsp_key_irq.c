//********************************INCLUDES********************************** */
#include "bsp_key_irq.h"

//********************************DEFINES*********************************** */


 QueueHandle_t key_Queue1, key_Queue2;
 
  // Create a queue capable of containing 10 uint32_t values.
  key_Queue1 = xQueueCreate( 10, sizeof( key_event ) );
  if( key_Queue1 == NULL )
  {
      // Queue was not created and must not be used.
  }

key_status_t key_task_func()
{
    // variable in stack
    key_event_t key_event           = {0};
    uint8_t     key_flag            = 0;
    uint32_t    tick_temp           = 0;
    uint32_t    tick_differences    = 0;
    key_Queue2 = xQueueCreate( 10, sizeof( key_event ) );
    if( key_Queue2 == NULL )
    {
      // Queue was not created and must not be used.
    }
    for(;;)
    {
        if( key_Queue1 != 0 )
        //1.  To check whether there is a new queue data in key_queue *  
        {
            if( xQueueReceive( key_Queue1, &( key_event ), ( TickType_t ) 10 ) )
            {
                    //1.1 if there is a new data pushed in  key_queue, update the status 
                    //    machine.
                    //1.1.1.1 if first data is rising edge capture it and report it
                    if( ( KEY_Rising == key_event.key_edge ) && ( 0 == key_flag )  )
                    {
                        printf("[key][error]first edge is rising\r\n");
                    }
                    //1.1.1.2 if first data is falling edge,push it into key_queue,and update 
                    //status machine
                    if( (KEY_Falling == key_event.key_edge) && ( 0 == key_flag ) )
                    {
                        key_flag += 1;
                        tick_temp = key_event.tick;
                    }
                    //1.1.1.3 if sceond data is rising edge,push it into key_queue,and update
                    //status machine
                    if( (KEY_Rising == key_event.key_edge) && ( 1 == key_flag ) )
                    {
                        //1.1.1.3.1 To calcultor difference between rising and falling edge.
                        key_flag = 0;
                        tick_differences = key_event.tick - tick_temp;
                        //1.1.1 if the tick between two events less than 20ms,than this data is
                        //       invalid
                        if(tick_differences < KEY_FILTER_TIME_MS )
                        {
                            printf("[key][filter]invalid data\r\n");
                            continue;
                        }                   
                        // If  differences less than 500ms than this is a short pressed
                        if(tick_differences < KEY_SHORT_TIME_MS )
                        {
                            printf("this is a short pressed\r\n");
                        }
                        //1.1.1.3.2 To calcultor difference between rising and falling edge.if 
                        // differences more than 600ms than this is a long pressed
                        if(tick_differences > KEY_LONG_TIME_MS )
                        {
                            printf("this is a long pressed\r\n");
                        }
                    }
            }
        }

    }

}

//********************************DECLARING********************************* */