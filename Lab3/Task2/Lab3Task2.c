#include "contiki.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/routing.h"
#include "dev/leds.h"

#define DEBUG DEBUG_PRINT
#include "net/ipv6/uip-debug.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_DBG

/*---------------------------------------------------------------------------*/
PROCESS(node_process, "RPL Node");
AUTOSTART_PROCESSES(&node_process);

/*---------------------------------------------------------------------------*/
/* Function to determine the node's role and update LEDs */
static void update_node_role() {
    int is_root = (node_id == 1);  // Root node has node_id == 1
    int has_parent = 0;            // Does the node have a parent?
    int has_children = 0;          // Does the node have children?

    // Check if the node has a parent (upstream route)
    if (NETSTACK_ROUTING.node_is_reachable()) {
        has_parent = 1;
    }

    // Check if the node has children (downstream routes)
    uip_ds6_route_t *route = uip_ds6_route_head();
    while (route) {
        if (uip_ds6_route_nexthop(route) != NULL) {
            has_children = 1;
            break;
        }
        route = uip_ds6_route_next(route);
    }

    // Debug prints
    LOG_INFO("Node %d: is_root=%d, has_parent=%d, has_children=%d\n", node_id, is_root, has_parent, has_children);

    // Update LEDs based on the node's role
    if (is_root) {
        // Root Node: Green LED
        leds_on(LEDS_GREEN);
        leds_off(LEDS_YELLOW | LEDS_RED);
    } else if (has_parent && has_children) {
        // Intermediate Node: Yellow LED (instead of blue)
        leds_on(LEDS_YELLOW);
        leds_off(LEDS_GREEN | LEDS_RED);
    } else if (has_parent && !has_children) {
        // Endpoint Node: Red LED
        leds_on(LEDS_RED);
        leds_off(LEDS_GREEN | LEDS_YELLOW);
    } else {
        // Outside Network: All LEDs OFF
        leds_off(LEDS_GREEN | LEDS_YELLOW | LEDS_RED);
    }
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
    static struct etimer et;

    PROCESS_BEGIN();

    // Check if this node is the root (coordinator)
    int is_coordinator = (node_id == 1);

    if (is_coordinator) {
        NETSTACK_ROUTING.root_start();  // Start RPL root
    }
    NETSTACK_MAC.on();  // Enable MAC layer

    // Initial LED update to reflect correct state at startup
    update_node_role();

    // Set up a periodic timer to update the node role and LEDs
    etimer_set(&et, CLOCK_SECOND * 10);  // Check every 10 seconds

    while (1) {
        // Print routing table information
        LOG_INFO("Routing entries: %u\n", uip_ds6_route_num_routes());
        uip_ds6_route_t *route = uip_ds6_route_head();
        while (route) {
            LOG_INFO("Route ");
            LOG_INFO_6ADDR(&route->ipaddr);
            LOG_INFO_(" via ");
            LOG_INFO_6ADDR(uip_ds6_route_nexthop(route));
            LOG_INFO_("\n");
            route = uip_ds6_route_next(route);
        }

        // Update the node role and LEDs
        update_node_role();

        // Wait for the timer to expire
        PROCESS_YIELD_UNTIL(etimer_expired(&et));
        etimer_reset(&et);  // Reset the timer
    }

    PROCESS_END();
}



