/**
 * The runtime system library for Streamix
 *
 * @file    smxrts.c
 * @author  Simon Maurer
 */

#include <pthread.h>
#include <stdlib.h>
#include <zlog.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#ifndef HANDLER_H
#define HANDLER_H

// TYPEDEFS -------------------------------------------------------------------
typedef struct net_smx_rn_s net_smx_rn_t;             /**< ::net_smx_rn_s */
typedef struct net_smx_tf_s net_smx_tf_t;             /**< ::net_smx_tf_s */
typedef struct smx_channel_s smx_channel_t;           /**< ::smx_channel_s */
typedef struct smx_channel_end_s smx_channel_end_t;   /**< ::smx_channel_end_s */
typedef struct smx_collector_s smx_collector_t;       /**< ::smx_collector_s */
typedef struct smx_fifo_s smx_fifo_t;                 /**< ::smx_fifo_s */
typedef struct smx_fifo_item_s smx_fifo_item_t;       /**< ::smx_fifo_item_s */
typedef struct smx_guard_s smx_guard_t;               /**< ::smx_guard_s */
typedef struct smx_msg_s smx_msg_t;                   /**< ::smx_msg_s */
typedef struct smx_net_s smx_net_t;                   /**< ::smx_net_s */
typedef struct smx_timer_s smx_timer_t;               /**< ::smx_timer_s */
typedef enum smx_channel_type_e smx_channel_type_t;   /**< #smx_channel_type_e */
typedef enum smx_channel_state_e smx_channel_state_t; /**< #smx_channel_state_e */

// ENUMS ----------------------------------------------------------------------
/**
 * @brief Streamix channel (buffer) types
 */
enum smx_channel_type_e
{
    SMX_FIFO,           /**< a simple FIFO */
    SMX_FIFO_D,         /**< a FIFO with decoupled output */
    SMX_FIFO_DD,        /**< a FIFO with decoupled output connected to a tf */
    SMX_D_FIFO,         /**< a FIFO with decoupled input */
    SMX_D_FIFO_D,       /**< a FIFO with decoupled input and output */
    SMX_D_FIFO_DD,      /**< a FIFO with decoupled input and output (tf) */
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
 * @brief Constants to indicate wheter a thread should terminate or continue
 */
enum smx_thread_state_e
{
    SMX_NET_RETURN = 0,     /**< decide automatically wheather to end or go on */
    SMX_NET_CONTINUE,       /**< continue to call the box implementation fct */
    SMX_NET_END             /**< end thread */
};

// STRUCTS --------------------------------------------------------------------
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
};

/**
 * The end of a channel
 */
struct smx_channel_end_s
{
    zlog_category_t*    cat;      /**< zlog category of a channel end */
    smx_channel_state_t state;    /**< state of the channel end */
    pthread_mutex_t     ch_mutex; /**< mutual exclusion */
    pthread_cond_t      ch_cv;    /**< conditional variable to trigger producer */
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
    int     count;               /**< counts occupied space */
    int     length;              /**< size of the FIFO */
    pthread_mutex_t fifo_mutex;  /**< mutual exclusion */
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

/**
 * @brief A Streamix port structure
 *
 * The structure contains handlers that can be used to manipulate data.
 * This handler is provided by the box implementation.
 */
struct smx_msg_s
{
    unsigned long id;               /**< the unique message id */
    void* data;                     /**< pointer to the data */
    int   size;                     /**< size of the data */
    void* (*copy)( void*, size_t ); /**< pointer to a fct making a deep copy */
    void  (*destroy)( void* );      /**< pointer to a fct that frees data */
    void* (*unpack)( void* );       /**< pointer to a fct that unpacks data */
};

/**
 * Common fields of a streamix net.
 */
struct smx_net_s
{
    unsigned int        id;         /**< a unique net id */
    zlog_category_t*    cat;        /**< the log category */
    void*               sig;        /**< the net port signature */
    xmlNodePtr          conf;
};

/**
 * @brief A Streamix timer structure
 *
 * A timer collects alle temporal firewalls of the same rate
 */
struct smx_timer_s
{
    int                 fd;         /**< timer file descriptor */
    struct itimerspec   itval;      /**< iteration specifiaction */
    net_smx_tf_t*       ports;      /**< list of temporal firewalls */
    int                 count;      /**< number of port pairs */
};

/**
 * @brief The signature of a copy synchronizer
 */
struct net_smx_rn_s
{
    struct {
        smx_channel_t** ports;      /**< an array of channel pointers */
        int count;                  /**< the number of input ports */
        smx_collector_t* collector; /**< ::smx_collector_s */
    } in;                           /**< input channels */
    struct {
        smx_channel_t** ports;      /**< an array of channel pointers */
        int count;                  /**< the number of output ports */
    } out;                          /**< output channels */
};

/**
 * @brief The signature of a temporal firewall
 */
struct net_smx_tf_s
{
    smx_channel_t*      in;         /**< input channel */
    smx_channel_t*      out;        /**< output channel */
    net_smx_tf_t*       next;       /**< pointer to the next element */
};

// USER MACROS -----------------------------------------------------------------
/**
 *
 */
/**!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
#define SMX_CHANNEL_READ( h, box_name, ch_name )\
    smx_channel_read( ( ( net_ ## box_name ## _t* )\
                SMX_SIG( h ) )->in.port_ ## ch_name )

/**
 *
 */
/**!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
#define SMX_CHANNEL_WRITE( h, box_name, ch_name, data )\
    smx_channel_write( ( ( net_ ## box_name ## _t* )\
                SMX_SIG( h ) )->out.port_ ## ch_name, data )

/**
 *
 */
/**!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
#define SMX_LOG( h, level, format, ... )\
    zlog_ ## level( ( ( smx_net_t* )h )->cat, format, ##__VA_ARGS__ )

/**
 *
 */
#define SMX_MSG_CREATE( data, dsize, fcopy, ffree, funpack )\
    smx_msg_create( data, dsize, fcopy, ffree, funpack )

/**
 *
 */
#define SMX_MSG_DESTROY( msg )\
    smx_msg_destroy( msg, 1 )

/**
 *
 */
#define SMX_MSG_UNPACK( msg )\
    smx_msg_unpack( msg )

/**
 *
 */
/**!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
#define SMX_NET_GET_ID( h ) ( ( smx_net_t* )h )->id;

/**
 *
 */
/**!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
#define SMX_NET_GET_CONF( h ) ( ( smx_net_t* )h )->conf;

// RTS MACROS ------------------------------------------------------------------
#define SMX_CHANNEL_CREATE( id, len, type, name )\
    smx_channel_t* ch_ ## id = smx_channel_create( len, type, id, #name )

#define SMX_CHANNEL_DESTROY( id )\
    smx_channel_destroy( ch_ ## id )

#define SMX_CONNECT( net_id, ch_id, net_name, box_name, ch_name, mode )\
    smx_connect( SMX_SIG_PORT( net_ ## net_id, box_name, ch_name, mode ),\
            ch_ ## ch_id, net_id, ch_id, #net_name, #ch_name, #mode )

#define SMX_CONNECT_ARR( net_id, ch_id, net_name, box_name, ch_name, mode )\
    smx_cat_add_channel_ ## mode( ch_ ## ch_id,\
            STRINGIFY( ch_n ## net_name ## _c ## ch_name ## _ ## ch_id ) );\
    smx_connect_arr( SMX_SIG_PORTS( net_ ## net_id, box_name, mode ),\
            SMX_SIG_PORT_COUNT( net_ ## net_id, box_name, mode ),\
            ch_ ## ch_id, net_id, ch_id, #net_name, #ch_name, #mode )

#define SMX_CONNECT_GUARD( id, iats, iatns )\
    smx_connect_guard( ch_ ## id, smx_guard_create( iats, iatns, ch_ ## id ) )

#define SMX_CONNECT_RN( net_id, ch_id )\
    smx_connect_rn( ch_ ## ch_id, net_ ## net_id )

#define SMX_CONNECT_TF( timer_id, ch_in_id, ch_out_id, ch_name )\
    smx_cat_add_channel_in( ch_ ## ch_in_id,\
            STRINGIFY( ch_nsmx_tf_c ## ch_name ## _ ## ch_in_id ) );\
    smx_cat_add_channel_out( ch_ ## ch_out_id,\
            STRINGIFY( ch_nsmx_tf_c ## ch_name ## _ ## ch_out_id ) );\
    smx_tf_connect( SMX_SIG( timer_ ## timer_id ), ch_ ## ch_in_id,\
            ch_ ## ch_out_id )

#define SMX_LOG_CH( cat, level, format, ...)\
    zlog_ ## level( cat, format, ##__VA_ARGS__ )

#define SMX_NET_CREATE( id, net_name, box_name )\
    smx_net_t* net_ ## id = smx_net_create( id, #net_name,\
            STRINGIFY( net_n ## net_name ## _ ## id ),\
            malloc( sizeof( struct net_ ## box_name ## _s ) ), &conf )

/**!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!111*/
#define SMX_NET_DESTROY( id, box_name )\
    free( ( ( net_ ## box_name ## _t* )SMX_SIG( net_ ## id ) )->in.ports );\
    free( ( ( net_ ## box_name ## _t* )SMX_SIG( net_ ## id ) )->out.ports );\
    free( SMX_SIG( net_ ## id ) );\
    free( net_ ## id )

/**!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!111*/
#define SMX_NET_INIT( id, box_name, indegree, outdegree )\
    ( ( net_ ## box_name ## _t* )SMX_SIG( net_ ## id ) )->in.count = 0;\
    ( ( net_ ## box_name ## _t* )SMX_SIG( net_ ## id ) )->in.ports\
        = malloc( sizeof( smx_channel_t* ) * indegree );\
    ( ( net_ ## box_name ## _t* )SMX_SIG( net_ ## id ) )->out.count = 0;\
    ( ( net_ ## box_name ## _t* )SMX_SIG( net_ ## id ) )->out.ports\
        = malloc( sizeof( smx_channel_t* ) * outdegree )

#define SMX_NET_RUN( id, net_name, box_name )\
    pthread_t th_net_ ## id = smx_net_run( box_ ## box_name, net_ ## id )

#define SMX_NET_RN_DESTROY( id )\
    smx_net_rn_destroy( ( SMX_SIG( net_ ## id ) ) )

#define SMX_NET_RN_INIT( id )\
    smx_net_rn_init( SMX_SIG( net_ ## id ) )

#define SMX_NET_WAIT_END( id )\
    pthread_join( th_net_ ## id, NULL )

#define SMX_PROGRAM_INIT_RUN() ;

#define SMX_PROGRAM_CLEANUP()\
    smx_program_cleanup( &conf )

#define SMX_PROGRAM_INIT()\
    xmlDocPtr conf = NULL;\
    smx_program_init( &conf )

#define SMX_SIG( h ) smx_get_signature( h )

#define SMX_SIG_PORT( h, box_name, port_name, mode )\
    ( SMX_SIG( h ) == NULL ) ? NULL : &( ( net_ ## box_name ## _t* )SMX_SIG( h ) )\
            ->mode.port_ ## port_name

#define SMX_SIG_PORT_COUNT( h, box_name, mode )\
    ( SMX_SIG( h ) == NULL ) ? NULL : &( ( net_ ## box_name ## _t* )SMX_SIG( h ) )\
            ->mode.count

#define SMX_SIG_PORTS( h, box_name, mode )\
    ( SMX_SIG( h ) == NULL ) ? NULL : ( ( net_ ## box_name ## _t* )SMX_SIG( h ) )\
            ->mode.ports

#define SMX_TF_CREATE( id, sec, nsec )\
    smx_net_t* timer_ ## id = smx_net_create( id, STRINGIFY( smx_tf ),\
            STRINGIFY( net_nsmx_tf ## _ ## id ), smx_tf_create( sec, nsec ),\
            &conf )

#define SMX_TF_DESTROY( id )\
    smx_tf_destroy( timer_ ## id );\

#define SMX_TF_RUN( id )\
    pthread_t th_timer_ ## id = smx_net_run( start_routine_tf, timer_ ## id )

#define SMX_TF_WAIT_END( id )\
    pthread_join( th_timer_ ## id, NULL )

/**!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1*/
#define START_ROUTINE_NET( h, net_name, box_name )\
    start_routine_net( box_name, box_name ## _init, box_name ## _cleanup, h,\
            ( ( net_ ## box_name ## _t* )SMX_SIG( h ) )->in.ports,\
            ( ( net_ ## box_name ## _t* )SMX_SIG( h ) )->in.count,\
            ( ( net_ ## box_name ## _t* )SMX_SIG( h ) )->out.ports,\
            ( ( net_ ## box_name ## _t* )SMX_SIG( h ) )->out.count )

#define SMX_NET_EXTERN( box_name )\
    extern int box_name( void*, void* );\
    extern int box_name ## _init( void*, void** );\
    extern void box_name ## _cleanup( void* )

#define STRINGIFY(x) #x

// FUNCTIONS ------------------------------------------------------------------
/**
 * Add a zlog category for a channel source end
 *
 * @param ch    the channel id
 * @param name  the name of the zlog category
 */
void smx_cat_add_channel_in( smx_channel_t* ch, const char* name );

/**
 * Add a zlog category for a channel sink end
 *
 * @param ch    the channel id
 * @param name  the name of the zlog category
 */
void smx_cat_add_channel_out( smx_channel_t* ch, const char* name );

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
 * @param len   length of a FIFO
 * @param type  type of the buffer
 * @param id    unique identifier of the channel
 * @param name  name of the channel
 * @return      pointer to the created channel or NULL if something went wrong
 */
smx_channel_t* smx_channel_create( int len, smx_channel_type_t type,
        int id, const char* name );

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
 * @brief Read the data from an input port
 *
 * Allows to access the channel and read data. The channel ist protected by
 * mutual exclusion. The macro SMX_CHANNEL_READ( h, net, port ) provides a
 * convenient interface to access the ports by name.
 *
 * @param ch    pointer to the channel
 * @return      pointer to a message structure ::smx_msg_s or NULL if something
 *              went wrong.
 */
smx_msg_t* smx_channel_read( smx_channel_t* ch );

/**
 * @brief Returns the number of available messages in channel
 *
 * @param ch    pointer to the channel
 * @return      number of available messages in channel
 */
int smx_channel_ready_to_read( smx_channel_t* ch );

/**
 * @brief Write data to an output port
 *
 * Allows to access the channel and write data. The channel ist protected by
 * mutual exclusion. The macro SMX_CHANNEL_WRITE( h, net, port, data ) provides
 * a convenient interface to access the ports by name.
 *
 * @param ch    pointer to the channel
 * @param msg   pointer to the a message structure
 * @return      1 if message was overwritten, 0 otherwise
 */
int smx_channel_write( smx_channel_t* ch, smx_msg_t* msg );

/**
 * Connect a channel to a net by name matching.
 *
 * @param dest        a pointer to the destination
 * @param src         a pointer to the source
 * @param dest_id     the id of the destination
 * @param src_id      the id of the source
 * @param dest_name   a string literal of the destination name
 * @param src_name    a string literal of the source name
 * @param mode        the direction of the connection
 */
void smx_connect( smx_channel_t** dest, smx_channel_t* src, int dest_id,
        int src_id, const char* dest_name, const char* src_name,
        const char* mode );

/**
 * Connect a channel to a net by index.
 *
 * @param dest        a pointer to the destination
 * @param idx         a pointer to the destination index of the port array
 * @param src         a pointer to the source
 * @param dest_id     the id of the destination
 * @param src_id      the id of the source
 * @param dest_name   a string literal of the destination name
 * @param src_name    a string literal of the source name
 * @param mode        the direction of the connection
 */
void smx_connect_arr( smx_channel_t** dest, int* idx, smx_channel_t* src,
        int dest_id, int src_id, const char* dest_name, const char* src_name,
        const char* mode );

/**
 * Connect a guard to a channel
 *
 * @param ch    the target channel
 * @param guard the guard to be connected
 * @return      0 on success, -1 on failure
 */
int smx_connect_guard( smx_channel_t* ch, smx_guard_t* guard );

/**
 * Connect a routing node to a channel
 *
 * @param ch    the target channel
 * @param guard the rn to be connected
 * @return      0 on success, -1 on failure
 */
int smx_connect_rn( smx_channel_t* ch, smx_net_t* rn );

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
 * @param ch    pointer to channel struct of the FIFO
 * @param fifo  pointer to a FIFO channel
 * @return      pointer to a message structure
 */
smx_msg_t* smx_fifo_read( smx_channel_t* ch, smx_fifo_t* fifo );

/**
 * @brief read from a Streamix FIFO_D channel
 *
 * Read from a channel that is decoupled at the output (the consumer is
 * decoupled at the input). This means that the msg at the head of the FIFO_D
 * will potentially be duplicated.
 *
 * @param ch    pointer to channel struct of the FIFO
 * @param fifo  pointer to a FIFO_D channel
 * @return      pointer to a message structure
 */
smx_msg_t* smx_fifo_d_read( smx_channel_t* ch , smx_fifo_t* fifo );

/**
 * @brief read from a Streamix FIFO_DD channel
 *
 * Read from a channel that is decoupled at the output and connected to a
 * temporal firewall. The read is non-blocking but no duplication of messages
 * is done. If no message is available NULL is returned.
 *
 * @param ch    pointer to channel struct of the FIFO
 * @param fifo  pointer to a FIFO_D channel
 * @return      pointer to a message structure
 */
smx_msg_t* smx_fifo_dd_read( smx_channel_t* ch, smx_fifo_t* fifo );

/**
 * @brief write to a Streamix FIFO channel
 *
 * @param ch    pointer to channel struct of the FIFO
 * @param fifo  pointer to a FIFO channel
 * @param msg   pointer to the data
 * @return      0 on success, 1 otherwise
 */
int smx_fifo_write( smx_channel_t* ch, smx_fifo_t* fifo, smx_msg_t* msg );

/**
 * @brief write to a Streamix D_FIFO channel
 *
 * Write to a channel that is decoupled at the input (the produced is decoupled
 * at the output). This means that the tail of the D_FIFO will potentially be
 * overwritten.
 *
 * @param ch    pointer to channel struct of the FIFO
 * @param fifo  pointer to a D_FIFO channel
 * @param msg   pointer to the data
 * @return      0 on success, 1 otherwise
 */
int smx_d_fifo_write( smx_channel_t* ch, smx_fifo_t* fifo, smx_msg_t* msg );

/**
 * Get the net signature from the nat handler.
 *
 * @param h     pointer to the net handler
 * @return      pointer to the net signature or NULL on failure.
 */
void* smx_get_signature( smx_net_t* h );

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
 * @param ch pointer to the channel structure
 * @return      0 on success, 1 otherwise
 */
int smx_guard_write( smx_channel_t* ch );

/**
 * @brief imposes a rate-control on decoupled write operations
 *
 * A message is discarded if it did not reach the specified minimal inter-
 * arrival time (messages are not buffered and delayed, it's only a very simple
 * implementation)
 *
 * @param ch    pointer to the channel structure
 * @param msg   pointer to the message structure
 *
 * @return      -1 if message was discarded, 0 otherwise
 */
int smx_d_guard_write( smx_channel_t* ch, smx_msg_t* msg );

/**
 * @brief make a deep copy of a message
 *
 * @param msg   pointer to the message structure to copy
 * @return      pointer to the newly created message structure
 */
smx_msg_t* smx_msg_copy( smx_msg_t* msg );

/**
 * @brief Create a message structure
 *
 * Allows to create a message structure and attach handlers to modify the data
 * in the message structure. If defined, the init function handler is called
 * after the message structure is created.
 *
 * @param data              a pointer to the data to be added to the message
 * @param size              the size of the data
 * @param copy( data,size ) a pointer to a function perfroming a deep copy of
 *                          the data in the message structure. The function
 *                          takes a void pointer as an argument that points to
 *                          the data structure to copy and the size of the data
 *                          structure. The function must return a void pointer
 *                          to the copied data structure.
 * @param destroy( data )   a pointer to a function freeing the memory of the
 *                          data in the message structure. The function takes a
 *                          void pointer as an argument that points to the data
 *                          structure to free.
 * @param unpack( data )    a pointer to a function that unpacks the message
 *                          data. The function takes a void pointer as an
 *                          argument that points to the message payload and
 *                          returns a void pointer that points to the unpacked
 *                          message payload.
 * @return                  a pointer to the created message structure
 */
smx_msg_t* smx_msg_create( void* data, size_t size, void* copy( void*, size_t ),
        void destroy( void* ), void* unpack( void* ) );

/**
 * @brief Default copy function to perform a shallow copy of the message data
 *
 * @param data      a void pointer to the data structure
 * @param size      the size of the data
 * @return          a void pointer to the data
 */
void* smx_msg_data_copy( void* data, size_t size );

/**
 * @brief Default destroy function to destroy the data inside a message
 *
 * @param data  a void pointer to the data to be freed (shallow)
 */
void smx_msg_data_destroy( void* data );

/**
 * @brief Default unpack function for the message payload
 *
 * @param data  a void pointer to the message payload.
 * @return      a void pointer to the unpacked message payload.
 */
void* smx_msg_data_unpack( void* data );

/**
 * @brief Destroy a message structure
 *
 * Allows to destroy a message structure. If defined (see smx_msg_create()), the
 * destroy function handler is called before the message structure is freed.
 *
 * @param msg   a pointer to the message structure to be destroyed
 * @param deep  a flag to indicate whether the data shoudl be deleted as well
 *              if msg->destroy() is NULL this flag is ignored
 */
void smx_msg_destroy( smx_msg_t* msg, int deep );

/**
 * @brief Unpack the message payload
 *
 * @param msg   a pointer to the message structure to be destroyed
 * @return      a void pointer to the payload
 */
void* smx_msg_unpack( smx_msg_t* msg );

/**
 * Create a new net instance. This includes
 *  - creating a zlog category
 *  - assigning the net-specifix XML configuartion
 *  - assigning the net signature
 *
 * @param id        a unique net identifier
 * @param cat_name  the name of the zlog category
 * @param sig       a pointer to the net signature
 */
smx_net_t* smx_net_create( unsigned int id, const char* name,
        const char* cat_name, void* sig, xmlDocPtr* conf );

/**
 * @brief Destroy copy sync structure
 *
 * @param cp    pointer to the cp sync structure
 */
void smx_net_rn_destroy( net_smx_rn_t* cp );

/**
 * @brief Initialize copy synchronizer structure
 *
 * @param cp    pointer to the copy sync structure
 * @return      0 on success, -1 otherwise
 */
int smx_net_rn_init( net_smx_rn_t* cp );

/**
 * @brief create pthred of net
 *
 * @param box_impl( arg )   function pointer to the box implementation
 * @param h                 pointer to the net handler
 * @return                  a pthread id
 */
pthread_t smx_net_run( void* box_impl( void* arg ), void* h );

/**
 * @brief Update the state of the box
 *
 * Update the state of the box to indicate wheter computaion needs to scontinue
 * or terminate. The state can either be forced by the box implementation (see
 * \p state) or depends on the state of the triggering producers.
 * Note that non-triggering producers may still be alive but the thread will
 * still terminate if all triggering producers are terminated. This is to
 * prevent a while(1) type of behaviour because no blocking will occur to slow
 * the thread execution.
 *
 * @param h         pointer to the net handler
 * @param chs_in    a list of input channels
 * @param len_in    number of input channels
 * @param chs_out   a list of output channels
 * @param len_out   number of output channels
 * @param state     state set by the box implementation. If set to
 *                  SMX_NET_CONTINUE, the box will not terminate. If set to
 *                  SMX_NET_END, the box will terminate. If set to
 *                  SMX_NET_RETURN (or 0) this function will determine wheter
 *                  a box terminates or not
 * @return          SMX_NET_CONTINUE if there is at least one triggeringr
 *                  producer alive. SMX_BOX_TERINATE if all triggering
 *                  prodicers are terminated.
 */
int smx_net_update_state( void* h, smx_channel_t** chs_in, int len_in,
        smx_channel_t** chs_out, int len_out, int state );

/**
 * @brief Set all channel states to end and send termination signal to all
 * output channels.
 *
 * @param h         pointer to the net handler
 * @param chs_in    a list of input channels
 * @param len_in    number of input channels
 * @param chs_out   a list of output channels
 * @param len_out   number of output channels
 */
void smx_net_terminate( void* h, smx_channel_t** chs_in, int len_in,
        smx_channel_t** chs_out, int len_out );

/**
 * Function to be called if system is out of memory.
 */
void smx_out_of_memory();

/**
 * @brief Perfrom some cleanup tasks
 *
 * Close the log file
 *
 * @param conf   a pointer to the configurcation structure
 */
void smx_program_cleanup( xmlDocPtr* conf );

/**
 * @brief Perfrom some initialisation tasks
 *
 * Initialize logs and log the beginning of the program
 *
 * @param conf   a pointer to the configurcation structure
 */
void smx_program_init( xmlDocPtr* conf );

/**
 * @brief the box implementattion of a routing node (former known as copy sync)
 *
 * A copy synchronizer reads from any port where data is available and copies
 * it to every output. The read order is first come first serve with peaking
 * wheter data is available. The cp sync is only blocking on read if no input
 * channel has data available. The copied data is written to the output channel
 * in order how they appear in the list. Writing is blocking. All outputs must
 * be written before new input is accepted.
 *
 * @param handler   a pointer to the signature
 * @return          returns the state of the box
 */
int smx_rn( void* h, void* state );
int smx_rn_init( void* h, void** state );
void smx_rn_cleanup( void* state );

/**
 * @brief grow the list of temporal firewalls and connect channels
 *
 * @param timer     pointer to a timer structure
 * @param ch_in     input channel to the temporal firewall
 * @param ch_out    output channel from the temporal firewall
 */
void smx_tf_connect( smx_timer_t* timer, smx_channel_t* ch_in,
        smx_channel_t* ch_out );

/**
 * @brief create a periodic timer structure
 *
 * @param sec   time interval in seconds
 * @param nsec  time interval in nano seconds
 * @return      pointer to the created timer structure
 */
smx_timer_t* smx_tf_create( int sec, int nsec);

/**
 * @brief destroy a timer structure and the list of temporal firewalls inside
 *
 * @param tt    pointer to the temporal firewall
 */
void smx_tf_destroy( smx_net_t* tt );

/**
 * @brief enable periodic tt timer
 *
 * @param h     the net handler
 * @param timer pointer to a timer structure
 */
void smx_tf_enable( void* h, smx_timer_t* timer );

/**
 * Read all input channels of a temporal firewall and propagate the messages to
 * the corresponding outputs of the temporal firewall.
 *
 * @param tt     a pointer to the timer
 * @param ch_in  a pointer to an array of input channels
 * @param ch_out a pointer to an array of output channels
 */
void smx_tf_propagate_msgs( smx_timer_t* tt, smx_channel_t** ch_in,
        smx_channel_t** ch_out );

void smx_tf_read_inputs( smx_msg_t** msg, smx_timer_t* tt,
        smx_channel_t** ch_in, smx_channel_t** ch_out );

/**
 * @brief blocking wait on timer
 *
 * Waits on the specified time interval. An error message is printed if the
 * deadline was missed.
 *
 * @param h     the net handler
 * @param timer pointer to a timer structure
 */
void smx_tf_wait( void* h, smx_timer_t* timer );

/**
 * @brief write to all output channels of a temporal firewall
 *
 * @param msg       a pointer to the message array. The array has a length that
 *                  matches the number of output channels of the temporal
 *                  firewall
 * @param tt        a pointer to the timer
 * @param ch_in     a pointer to an array of input channels
 * @param ch_out    a pointer to an array of output channels
 */
void smx_tf_write_outputs( smx_msg_t**, smx_timer_t*, smx_channel_t**,
        smx_channel_t** );

/**
 * @brief the start routine of a thread associated to a box
 *
 * @param impl( arg )       pointer to the net implementation function
 * @param init( arg )       pointer to the net intitialisation function
 * @param cleanup( arg )    pointer to the net cleanup function
 * @param h                 pointer to the net handler
 * @param chs_in            list of input channels
 * @param cnt_in            count of input ports
 * @param chs_out           list of output channels
 * @param cnt_out           counter of output port
 * @return                  returns NULL
 */
void* start_routine_net( int impl( void*, void* ), int init( void*, void** ),
        void cleanup( void* ), void* h, smx_channel_t** chs_in, int cnt_in,
        smx_channel_t** chs_out, int cnt_out );

/**
 * @brief start routine for a timer thread
 *
 * @param h pointer to a timer structure
 * @return  NULL
 */
void* start_routine_tf( void* h );


#endif // HANDLER_H
