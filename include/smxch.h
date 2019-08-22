/**
 * Channel and FIFO definitions for the runtime system library of Streamix
 *
 * @file    smxch.h
 * @author  Simon Maurer
 * @license
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this file,
 *  You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <pthread.h>
#include <zlog.h>
#include "smxmsg.h"

#ifndef SMXCH_H
#define SMXCH_H

#define SMX_MAX_CHS 10000

typedef struct smx_channel_s smx_channel_t;           /**< ::smx_channel_s */
typedef struct smx_channel_end_s smx_channel_end_t;   /**< ::smx_channel_end_s */
typedef enum smx_channel_err_e smx_channel_err_t;     /**< #smx_channel_err_e */
typedef enum smx_channel_type_e smx_channel_type_t;   /**< #smx_channel_type_e */
typedef enum smx_channel_state_e smx_channel_state_t; /**< #smx_channel_state_e */
typedef struct smx_collector_s smx_collector_t;       /**< ::smx_collector_s */
typedef struct smx_fifo_s smx_fifo_t;                 /**< ::smx_fifo_s */
typedef struct smx_fifo_item_s smx_fifo_item_t;       /**< ::smx_fifo_item_s */
typedef struct smx_guard_s smx_guard_t;               /**< ::smx_guard_s */

/**
 * The error state of a channel end
 */
enum smx_channel_err_e
{
    SMX_CHANNEL_ERR_NONE,          /**< no error */
    SMX_CHANNEL_ERR_UNINITIALISED, /**< the channel was never initialised */
    SMX_CHANNEL_ERR_NO_DATA,       /**< unexpectedly, the channel has no data */
    SMX_CHANNEL_ERR_NO_SPACE,      /**< unexpectedly, the channel has no space */
    SMX_CHANNEL_ERR_DL_MISS,       /**< connecting net missed its deadline */
    SMX_CHANNEL_ERR_NO_TARGET      /**< connecting net has terminated */
};

/**
 * @brief Streamix channel (buffer) types
 */
enum smx_channel_type_e
{
    SMX_FIFO,           /**< a simple FIFO */
    SMX_FIFO_D,         /**< a FIFO with decoupled output */
    SMX_D_FIFO,         /**< a FIFO with decoupled input */
    SMX_D_FIFO_D        /**< a FIFO with decoupled input and output */
};

/**
 * @brief Channel state
 *
 * This allows to indicate wheter a producer connected to the channel has
 * terminated and wheter data is available to read. The second point is
 * important in combination with copy synchronizers.
 */
enum smx_channel_state_e
{
    SMX_CHANNEL_UNINITIALISED, /**< decoupled channel was never written to */
    SMX_CHANNEL_PENDING,       /**< channel is waiting for a signal */
    SMX_CHANNEL_READY,         /**< channel is ready to read from */
    SMX_CHANNEL_END            /**< net connected to channel end has terminated */
};

/**
 * @brief A generic Streamix channel
 */
struct smx_channel_s
{
    int                 id;         /**< the id of the channel */
    smx_channel_type_t  type;       /**< type of the channel */
    const char*         name;       /**< name of the channel */
    smx_fifo_t*         fifo;       /**< ::smx_fifo_s */
    smx_guard_t*        guard;      /**< ::smx_guard_s */
    smx_collector_t*    collector;  /**< ::smx_collector_s, collect signals */
    smx_channel_end_t*  sink;       /**< ::smx_channel_end_s */
    smx_channel_end_t*  source;     /**< ::smx_channel_end_s */
    zlog_category_t*    cat;        /**< zlog category of a channel end */
    pthread_mutex_t     ch_mutex;   /**< mutual exclusion */
};

/**
 * The end of a channel
 */
struct smx_channel_end_s
{
    smx_channel_state_t state;    /**< state of the channel end */
    smx_channel_err_t   err;      /**< error on the channel end */
    pthread_cond_t      ch_cv;    /**< conditional variable to trigger producer */
    unsigned long       count;    /**< access counter */
};

/**
 * @brief Collect channel counts
 *
 * This is used to nondeterministically merge channels with a copy synchronyzer
 * that has multiple inputs.
 */
struct smx_collector_s
{
    pthread_mutex_t     col_mutex;  /**< mutual exclusion */
    pthread_cond_t      col_cv;     /**< conditional variable to trigger box */
    int                 count;      /**< collection of channel counts */
    smx_channel_state_t state;      /**< state of the channel */
};

/**
 * @brief Streamix fifo structure
 *
 * The fifo structure is blocking on write if all buffers are occupied and
 * blocking on read if all buffer spaces are empty. The blocking pattern
 * can be changed by decoupling either the input, the output or both.
 */
struct smx_fifo_s
{
    smx_fifo_item_t*  head;      /**< pointer to the heda of the FIFO */
    smx_fifo_item_t*  tail;      /**< pointer to the tail of the FIFO */
    smx_msg_t*        backup;    /**< ::smx_msg_s, msg space for decoupling */
    int     overwrite;           /**< counts number of overwrite operations */
    int     copy;                /**< counts number of copy operations */
    int     count;               /**< counts occupied space */
    int     length;              /**< size of the FIFO */
};

/**
 * @brief A single FIFO item of a circular double-linked-list
 */
struct smx_fifo_item_s
{
    smx_msg_t*       msg;        /**< ::smx_msg_s */
    smx_fifo_item_t* next;       /**< pointer to the next item */
    smx_fifo_item_t* prev;       /**< pointer to the previous item */
};

/**
 * @brief timed guard to limit communication rate
 */
struct smx_guard_s
{
    int             fd;     /**< file descriptor pointing to timer */
    struct timespec iat;    /**< minumum inter-arrival-time */
};

#define SMX_LOG_CH( ch, level, format, ... )\
    SMX_LOG_INTERN( level, ch->cat, format,  ##__VA_ARGS__ )

/**
 * Change the state of a channel collector. The state is only changed if the
 * current state is differnt than the new state and than the end state.
 *
 * @param ch    pointer to the channel
 * @param state the new state
 */
void smx_channel_change_collector_state( smx_channel_t* ch,
        smx_channel_state_t state );

/**
 * Change the read state of a channel. The state is only changed if the
 * current state is differnt than the new state and than the end state.
 *
 * @param ch    pointer to the channel
 * @param state the new state
 */
void smx_channel_change_read_state( smx_channel_t* ch,
        smx_channel_state_t state );

/**
 * Change the write state of a channel. The state is only changed if the
 * current state is differnt than the new state and than the end state.
 *
 * @param ch    pointer to the channel
 * @param state the new state
 */
void smx_channel_change_write_state( smx_channel_t* ch,
        smx_channel_state_t state );

/**
 * @brief Create Streamix channel
 *
 * @param ch_cnt    pointer to the channel counter (is increased by one after
 *                  channel creation)
 * @param len       length of a FIFO
 * @param type      type of the buffer
 * @param id        unique identifier of the channel
 * @param name      name of the channel
 * @param cat_name  name of the channel zlog category
 * @return          a pointer to the created channel or NULL
 */
smx_channel_t* smx_channel_create( int* ch_cnt, int len,
        smx_channel_type_t type, int id, const char* name,
        const char* cat_name );

/**
 * Create a channel end.
 *
 * @return a pointer to a ne channel end or NULL if something went wrong
 */
smx_channel_end_t* smx_channel_create_end();

/**
 * @brief Destroy Streamix channel structure
 *
 * @param ch    pointer to the channel to destroy
 */
void smx_channel_destroy( smx_channel_t* ch );

/**
 * @brief Destroy Streamix channel end structure
 *
 * @param end   pointer to the channel end to destroy
 */
void smx_channel_destroy_end( smx_channel_end_t* end );

/**
 * @brief Read the data from an input port
 *
 * Allows to access the channel and read data. The channel ist protected by
 * mutual exclusion. The macro SMX_CHANNEL_READ( h, net, port ) provides a
 * convenient interface to access the ports by name.
 *
 * @param h     pointer to the net handler
 * @param ch    pointer to the channel
 * @return      pointer to a message structure ::smx_msg_s or NULL if something
 *              went wrong.
 */
smx_msg_t* smx_channel_read( void* h, smx_channel_t* ch );

/**
 * @brief Returns the number of available messages in channel
 *
 * @param ch    pointer to the channel
 * @return      number of available messages in channel or -1 on failure
 */
int smx_channel_ready_to_read( smx_channel_t* ch );

/**
 * @brief Returns the number of available space in a channel
 *
 * @param ch    pointer to the channel
 * @return      number of available space in a channel or -1 on failure
 */
int smx_channel_ready_to_write( smx_channel_t* ch );

/**
 * Send the termination signal to a channel sink
 *
 * @param ch    pointer to the channel
 */
void smx_channel_terminate_sink( smx_channel_t* ch );

/**
 * Send the termination signal to a channel source
 *
 * @param ch    pointer to the channel
 */
void smx_channel_terminate_source( smx_channel_t* ch );

/**
 * @brief Write data to an output port
 *
 * Allows to access the channel and write data. The channel ist protected by
 * mutual exclusion. The macro SMX_CHANNEL_WRITE( h, net, port, data ) provides
 * a convenient interface to access the ports by name.
 *
 * @param h     pointer to the net handler
 * @param ch    pointer to the channel
 * @param msg   pointer to the a message structure
 * @return      0 on success, -1 otherwise
 */
int smx_channel_write( void* h, smx_channel_t* ch, smx_msg_t* msg );

/**
 * Create a collector structure and initialize it.
 *
 * @return a pointer to the created collector strcuture or NULL.
 */
smx_collector_t* smx_collector_create();

/**
 * Destroy and deinit a collector structure.
 *
 * @param collector a pointer to the collector structure to be destroyed.
 */
void smx_collector_destroy( smx_collector_t* collector );

/**
 * Send the termination signal to the collector
 *
 * @param ch    pointer to the channel
 *
 */
void smx_collector_terminate( smx_channel_t* ch );

/**
 * Connect a channel to a net by name matching.
 *
 * @param dest        a pointer to the destination
 * @param src         a pointer to the source
 * @param net_id      the id of the net
 * @param net_name    the name of the net
 * @param mode        the direction of the connection
 * @param count       pointer to th econnected port counter
 */
void smx_connect( smx_channel_t** dest, smx_channel_t* src, int net_id,
        const char* net_name, const char* mode, int* count );

/**
 * Connect a guard to a channel
 *
 * @param ch    the target channel
 * @param guard the guard to be connected
 */
void smx_connect_guard( smx_channel_t* ch, smx_guard_t* guard );

/**
 * @brief Create Streamix FIFO channel
 *
 * @param length    length of the FIFO
 * @return          pointer to the created FIFO
 */
smx_fifo_t* smx_fifo_create( int length );

/**
 * @brief Destroy Streamix FIFO channel structure
 *
 * @param fifo  pointer to the channel to destroy
 */
void smx_fifo_destroy( smx_fifo_t* fifo );

/**
 * @brief read from a Streamix FIFO channel
 *
 * @param h     pointer to the net handler
 * @param ch    pointer to channel struct of the FIFO
 * @param fifo  pointer to a FIFO channel
 * @return      pointer to a message structure
 */
smx_msg_t* smx_fifo_read( void* h, smx_channel_t* ch, smx_fifo_t* fifo );

/**
 * @brief read from a Streamix FIFO_D channel
 *
 * Read from a channel that is decoupled at the output (the consumer is
 * decoupled at the input). This means that the msg at the head of the FIFO_D
 * will potentially be duplicated.
 *
 * @param h     pointer to the net handler
 * @param ch    pointer to channel struct of the FIFO
 * @param fifo  pointer to a FIFO_D channel
 * @return      pointer to a message structure
 */
smx_msg_t* smx_fifo_d_read( void* h, smx_channel_t* ch , smx_fifo_t* fifo );

/**
 * @brief read from a Streamix FIFO_DD channel
 *
 * Read from a channel that is decoupled at the output and connected to a
 * temporal firewall. The read is non-blocking but no duplication of messages
 * is done. If no message is available NULL is returned.
 *
 * @param h     pointer to the net handler
 * @param ch    pointer to channel struct of the FIFO
 * @param fifo  pointer to a FIFO_D channel
 * @return      pointer to a message structure
 */
smx_msg_t* smx_fifo_dd_read( void* h, smx_channel_t* ch, smx_fifo_t* fifo );

/**
 * @brief write to a Streamix FIFO channel
 *
 * @param h     pointer to the net handler
 * @param ch    pointer to channel struct of the FIFO
 * @param fifo  pointer to a FIFO channel
 * @param msg   pointer to the data
 * @return      0 on success, 1 otherwise
 */
int smx_fifo_write( void* h, smx_channel_t* ch, smx_fifo_t* fifo,
        smx_msg_t* msg );

/**
 * @brief write to a Streamix D_FIFO channel
 *
 * Write to a channel that is decoupled at the input (the produced is decoupled
 * at the output). This means that the tail of the D_FIFO will potentially be
 * overwritten.
 *
 * @param h     pointer to the net handler
 * @param ch    pointer to channel struct of the FIFO
 * @param fifo  pointer to a D_FIFO channel
 * @param msg   pointer to the data
 * @return      0 on success, 1 otherwise
 */
int smx_d_fifo_write( void* h, smx_channel_t* ch, smx_fifo_t* fifo,
        smx_msg_t* msg );

/**
 * Given a port name return a pointer to the port.
 *
 * @param ports     an array of ports to be searched
 * @param count     the number of ports to search
 * @param name      the name to search for
 * @return          the pointer to a port on success, NULL otherwise
 */
smx_channel_t* smx_get_channel_by_name( smx_channel_t** ports, int count,
        const char* name );

/**
 * Get the read error on a channel.
 *
 * @param ch
 *  Pointer to the channel
 * @return
 *  The error value indicationg the problem
 */
smx_channel_err_t smx_get_read_error( smx_channel_t* ch );

/**
 * Get the write error on a channel.
 *
 * @param ch
 *  Pointer to the channel
 * @return
 *  The error value indicationg the problem
 */
smx_channel_err_t smx_get_write_error( smx_channel_t* ch );

/**
 * @brief create timed guard structure and initialise timer
 *
 * @param iats  minimal inter-arrival time in seconds
 * @param iatns minimal inter-arrival time in nano seconds
 * @param ch    pointer to the channel
 * @return      pointer to the created guard structure
 */
smx_guard_t* smx_guard_create( int iats, int iatns, smx_channel_t* ch );

/**
 * @brief destroy the guard structure
 *
 * @param guard     pointer to the guard structure
 */
void smx_guard_destroy( smx_guard_t* guard );

/**
 * @brief imposes a rate-controld on write operations
 *
 * A producer is blocked until the minimum inter-arrival-time between two
 * consecutive messges has passed
 *
 * @param h     pointer to the net handler
 * @param ch pointer to the channel structure
 * @return      0 on success, 1 otherwise
 */
int smx_guard_write( void* h, smx_channel_t* ch );

/**
 * @brief imposes a rate-control on decoupled write operations
 *
 * A message is discarded if it did not reach the specified minimal inter-
 * arrival time (messages are not buffered and delayed, it's only a very simple
 * implementation)
 *
 * @param h     pointer to the net handler
 * @param ch    pointer to the channel structure
 * @param msg   pointer to the message structure
 *
 * @return      -1 if message was discarded, 0 otherwise
 */
int smx_d_guard_write( void* h, smx_channel_t* ch, smx_msg_t* msg );

#endif
