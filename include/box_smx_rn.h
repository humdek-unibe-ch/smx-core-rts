/**
 * Routing node box implementation for the runtime system library of Streamix
 *
 * @file    box_smx_rn.h
 * @author  Simon Maurer
 * @license
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this file,
 *  You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "smxch.h"
#include "smxnet.h"

#ifndef BOX_SMX_RN_H
#define BOX_SMX_RN_H

/**
 * Connect a routing node to a channel
 *
 * @param ch    the target channel
 * @param rn    a pointer to the net handler
 */
void smx_connect_rn( smx_channel_t* ch, smx_net_t* rn );

/**
 * @brief Destroy copy sync structure
 *
 * @param rn   a pointer to the net handler
 */
void smx_net_destroy_rn( smx_net_t* rn );

/**
 * @brief Initialize copy synchronizer structure
 *
 * @param rn   a pointer to the net handler
 */
void smx_net_init_rn( smx_net_t* rn );

/**
 * @brief the box implementattion of a routing node (former known as copy sync)
 *
 * A routing node reads from any port where data is available and copies
 * it to every output. The read order is first come first serve with peaking
 * wheter data is available. The cp sync is only blocking on read if no input
 * channel has data available. The copied data is written to the output channel
 * in order how they appear in the list. Writing is blocking. All outputs must
 * be written before new input is accepted.
 *
 * In order to provide fairness the routing node remembers the last port index
 * from which a message was read. The next time the rn is executed it will
 * search for available messages starting from the last port index +1. This
 * means that a routing node is not pure.
 *
 * @param h     a pointer to the net handler
 * @param state a pointer to the persistent state structure
 * @return        returns the state of the box
 */
int smx_rn( void* h, void* state );

/**
 * Initialises the routing node. The state is allocated with an integer which
 * is used to remember the last port index from which a message was read.
 *
 * @param h     pointer to the net handler
 * @param state pointer to the state variable
 * @return      0 on success, -1 on failure
 */
int smx_rn_init( void* h, void** state );

/**
 * Cleanup the routing node by freeing the state variable.
 *
 * @param h     pointer to the net handler
 * @param state pointer to the state variable
 */
void smx_rn_cleanup( void* h, void* state );

/**
 * This function is predefined and must not be changed. It will be passed to the
 * net thread upon creation and will be executed as soon as the thread is
 * started. This function calls a macro which is define in the RTS and handles
 * the initialisation, the main loop of the net and the cleanup.
 *
 * @param h
 *  A pointer to the net handler.
 * @return
 *  This function always returns NULL.
 */
void* start_routine_smx_rn( void* h );

#endif
