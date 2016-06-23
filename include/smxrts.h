#ifndef HANDLER_H
#define HANDLER_H

#include "pthread.h"

#define SMX_BOX_CREATE( box )\
    malloc( sizeof( struct box_impl_##box##_s ) );
#define SMX_BOX_DESTROY( box )\
    free( box )
#define SMX_CHANNEL_CREATE()\
    smx_channel_create()
#define SMX_CHANNEL_DESTROY( ch )\
    smx_channel_destroy( ch )
#define SMX_CONNECT( h, ptr, box_name, ch_name )\
    ( ( box_impl_##box_name##_t* )h )->port_##ch_name = ptr

#define SMX_CHANNEL_READ( h, box_name, ch_name )\
    smx_channel_read( ( ( box_impl_ ## box_name ## _t* )h )->port_ ## ch_name )
#define SMX_CHANNEL_WRITE( h, box_name, ch_name, data )\
    smx_channel_write( ( ( box_impl_ ## box_name ## _t* )h )->port_ ## ch_name,\
            data )

/**
 * @brief Streamix channel structure
 *
 * The channel structure holds the data that is sent from one box to another
 */
typedef struct smx_channel_s
{
    void*   data;                   /**< pointer to the data */
    int     ready;                  /**< flag to indicate if data is ready */
    pthread_mutex_t channel_mutex;  /**< mutual exclusion */
    pthread_cond_t  channel_cv;     /**< conditional variable to trigger box */
} smx_channel_t;

/**
 * @brief Read the data from an input port
 *
 * Allows to access the channel and read data. The channel ist protected by
 * mutual exclusion. The macro SMX_CHANNEL_IN( h, box, port ) provides a
 * convenient interface to access the ports by name.
 *
 * @param void*     pointer to the box interface structure
 * @param int       index of the port to access
 * @return void*    pointer to the data structure
 */
void* smx_channel_read( smx_channel_t* );

/**
 * @brief Write data to an output port
 *
 * Allows to access the channel and write data. The channel ist protected by
 * mutual exclusion. The macro SMX_CHANNEL_OUT( h, box, port, data ) provides a
 * convenient interface to access the ports by name.
 *
 * @param void*     pointer to the box interface structure
 * @param int       index of the port to access
 * @param void*     pointer to the data structure
 */
void smx_channel_write( smx_channel_t*, void* );

smx_channel_t* smx_channel_create( void );
void smx_channel_destroy( void* );

#endif // HANDLER_H
