#ifndef HANDLER_H
#define HANDLER_H

#include "pthread.h"
#include "boxgen.h"
#include <stdlib.h>

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

/*****************************************************************************/
#define SMX_BOX_CREATE( box )\
    malloc( sizeof( struct box_##box##_s ) );

/*****************************************************************************/
#define SMX_BOX_INIT( box_name, arg )\
    pthread_t th_ ## box_name = smx_box_run( box_ ## box_name, arg )

/**
 *
 */
pthread_t smx_box_run( void*( void* ), void* );

/*****************************************************************************/
#define SMX_BOX_CLEANUP( box_name )\
    pthread_join( th_ ## box_name, NULL )


/*****************************************************************************/
#define SMX_BOX_DESTROY( box )\
    free( box )

/*****************************************************************************/
#define SMX_CHANNEL_CREATE()\
    smx_channel_create()

/**
 *
 */
smx_channel_t* smx_channel_create( void );

/*****************************************************************************/
#define SMX_CHANNEL_DESTROY( ch )\
    smx_channel_destroy( ch )

/**
 *
 */
void smx_channel_destroy( void* );

/*****************************************************************************/
#define SMX_CHANNEL_READ( h, box_name, ch_name )\
    smx_channel_read( ( ( box_ ## box_name ## _t* )h )->port_ ## ch_name )

/**
 * @brief Read the data from an input port
 *
 * Allows to access the channel and read data. The channel ist protected by
 * mutual exclusion. The macro SMX_CHANNEL_IN( h, box, port ) provides a
 * convenient interface to access the ports by name.
 *
 * @param smx_channel_t*    pointer to the channel
 * @return void*            pointer to the data structure
 */
void* smx_channel_read( smx_channel_t* );

/*****************************************************************************/
#define SMX_CHANNEL_WRITE( h, box_name, ch_name, data )\
    smx_channel_write( ( ( box_ ## box_name ## _t* )h )->port_ ## ch_name,\
            data )

/**
 * @brief Write data to an output port
 *
 * Allows to access the channel and write data. The channel ist protected by
 * mutual exclusion. The macro SMX_CHANNEL_OUT( h, box, port, data ) provides a
 * convenient interface to access the ports by name.
 *
 * @param smx_channel_t*    pointer to the channel
 * @param void*             pointer to the data structure
 */
void smx_channel_write( smx_channel_t*, void* );

/*****************************************************************************/
#define SMX_CHANNELS_CREATE( num )\
    malloc( sizeof( smx_channel_t* ) * num )

/*****************************************************************************/
#define SMX_CHANNELS_DESTROY( chs )\
    free( chs )

/*****************************************************************************/
#define SMX_CONNECT( box, ch, ch_name )\
    box->port_##ch_name = ch

#endif // HANDLER_H
