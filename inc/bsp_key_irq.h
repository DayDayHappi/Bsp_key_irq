#ifndef __BSP_KEY_IRQ_H__
#define __BSP_KEY_IRQ_H__

//********************************INCLUDES********************************** */


//********************************DEFINES*********************************** */
#define KEY_FILTER_TIME_MS   20  
#define KEY_SHORT_TIME_MS    500
#define KEY_LONG_TIME_MS     1000 

typedef enum
{
    KEY_OK                = 0,
    KEY_TimeOut           = 1,
    KEY_ErrorParam        = 2,
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

typedef struct
{
    key_edge_t key_edge = KEY_Default;
    uint32_t   tick     = 0;
}key_event_t;




//********************************DECLARING********************************* */
extern QueueHandle_t key_Queue1, key_Queue2;


#endif  // end of __BSP_KEY_IRQ_H__