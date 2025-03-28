#include <stdio.h>
#include <string.h>
#include "contiki.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "sys/clock.h"

/* Event structure */
struct event {
    clock_time_t time;  	 // Time of the event
    linkaddr_t addr;		 // Address of the node that reported the event
};

#define MAX_NUMBER_OF_EVENTS 3  // Maximum number of events to track
#define EVENT_TIMEOUT (30 * CLOCK_SECOND)  // 30-second timeout for events

static struct event event_history[MAX_NUMBER_OF_EVENTS];  // Event history array
static int event_count = 0;  // Current number of events in the history
static clock_time_t last_event_time = 0; // Last event timestamp

/*---------------------------------------------------------------------------*/
/* Function to check if an address is already in the event history */
static int is_address_in_history(const linkaddr_t *addr) {
    for (int i = 0; i < event_count; i++) {
   	 if (linkaddr_cmp(&event_history[i].addr, addr)) {
   		 return 1;  // Address found in history
   	 }
    }
    return 0;  // Address not found in history
}

/*---------------------------------------------------------------------------*/
/* Function to print the event history */
static void print_event_history() {
    printf("Event History:\n");
    for (int i = 0; i < event_count; i++) {
   	 printf("Event %d: Time = %lu, Node = %d\n", i, event_history[i].time, event_history[i].addr.u8[0]);
    }
}

/*---------------------------------------------------------------------------*/
/* Function to add a new event to the history */
static void add_event(const linkaddr_t *addr) {
    clock_time_t now = clock_time();
    last_event_time = now; // Update last event timestamp
    printf("Current time: %lu\n", now);

    // Remove expired events
    for (int i = 0; i < event_count; i++) {
   	 if (now - event_history[i].time > EVENT_TIMEOUT) {
   		 printf("Removing expired event from node %d\n", event_history[i].addr.u8[0]);
   		 // Shift remaining events to the left
   		 for (int j = i; j < event_count - 1; j++) {
       		 event_history[j] = event_history[j + 1];
   		 }
   		 event_count--;
   		 i--;  // Adjust index after shifting
   	 }
    }

    // Add new event if the address is not already in the history
    if (!is_address_in_history(addr) && event_count < MAX_NUMBER_OF_EVENTS) {
   	 printf("Adding new event from node %d\n", addr->u8[0]);
   	 event_history[event_count].time = now;
   	 linkaddr_copy(&event_history[event_count].addr, addr);
   	 event_count++;
    }

    print_event_history();  // Debug: Print event history
}

/*---------------------------------------------------------------------------*/
/* Function to check if an alarm should be triggered */
static void check_alarm() {
    printf("Checking alarm...\n");
    if (event_count >= MAX_NUMBER_OF_EVENTS) {
   	 printf("Alarm triggered! Turning on yellow LED.\n");
   	 leds_on(LEDS_YELLOW);  // Trigger alarm (turn on yellow LED)
    }
}

/*---------------------------------------------------------------------------*/
/* Function to check inactivity and turn off alarm */
static void check_inactivity() {
    clock_time_t now = clock_time();
    if (event_count > 0 && (now - last_event_time > EVENT_TIMEOUT)) {
   	 printf("No activity for 30 seconds. Turning off alarm.\n");
   	 leds_off(LEDS_YELLOW);  // Turn off alarm (turn off yellow LED)
    }
}

/*---------------------------------------------------------------------------*/
/* Callback function for received packets */
static void recv(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
    printf("Received: %s - from %d\n", (char*) data, src->u8[0]);
    leds_toggle(LEDS_GREEN);  // Toggle green LED on packet reception

    // Add the event to the history
    add_event(src);

    // Check if an alarm should be triggered
    check_alarm();
}

/*---------------------------------------------------------------------------*/
/* Main process */
PROCESS(clicker_ng_process, "Clicker NG Process");
AUTOSTART_PROCESSES(&clicker_ng_process);

PROCESS_THREAD(clicker_ng_process, ev, data)
{
    static struct etimer inactivity_timer;
    static char payload[] = "hej";  // Payload to broadcast

    PROCESS_BEGIN();

    /* Initialize NullNet */
    nullnet_buf = (uint8_t *)&payload;
    nullnet_len = sizeof(payload);
    nullnet_set_input_callback(recv);

    /* Activate the button sensor */
    SENSORS_ACTIVATE(button_sensor);

    /* Start the inactivity timer */
    etimer_set(&inactivity_timer, EVENT_TIMEOUT);

    while (1) {
   	 PROCESS_WAIT_EVENT();

   	 if (ev == sensors_event && data == &button_sensor) {
   		 leds_toggle(LEDS_RED);  // Toggle red LED on button press

   		 /* Add the local event to the history */
   		 add_event(&linkaddr_node_addr);

   		 /* Check if an alarm should be triggered */
   		 check_alarm();

   		 /* Broadcast the payload */
   		 memcpy(nullnet_buf, &payload, sizeof(payload));
   		 nullnet_len = sizeof(payload);
   		 NETSTACK_NETWORK.output(NULL);
   	 }
  	 
   	 /* Periodically check for inactivity */
   	 if (etimer_expired(&inactivity_timer)) {
   		 check_inactivity();
   		 etimer_reset(&inactivity_timer);
   	 }
    }

    PROCESS_END();
}


