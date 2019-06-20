/**
 * Net definitions for the runtime system library of Streamix
 *
 * @file    smxnet.h
 * @author  Simon Maurer
 */

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "smxnet.h"
#include "smxutils.h"
#include "smxprofiler.h"

/*****************************************************************************/
int smx_net_collector_check_avaliable( void* h, smx_collector_t* collector )
{
    int cur_count;
    pthread_mutex_lock( &collector->col_mutex );
    while( collector->state == SMX_CHANNEL_PENDING )
    {
        SMX_LOG_NET( h, debug, "waiting for message on collector" );
        pthread_cond_wait( &collector->col_cv, &collector->col_mutex );
    }
    cur_count = collector->count;
    if( collector->count > 0 )
    {
        collector->count--;
    }
    else
    {
        SMX_LOG_NET( h, debug, "collector state change %d -> %d", collector->state,
                SMX_CHANNEL_PENDING );
        collector->state = SMX_CHANNEL_PENDING;
    }
    pthread_mutex_unlock( &collector->col_mutex );
    return cur_count;
}

/*****************************************************************************/
smx_msg_t* smx_net_collector_read( void* h, smx_collector_t* collector,
        smx_channel_t** in, int count_in, int* last_idx )
{
    int cur_count, i, ch_count, ready_cnt;
    smx_msg_t* msg = NULL;
    smx_channel_t* ch = NULL;

    cur_count = smx_net_collector_check_avaliable( h, collector );

    if( cur_count > 0 )
    {
        ch_count = count_in;
        i = *last_idx;
        while( ch_count > 0)
        {
            i++;
            if( i >= count_in )
                i = 0;
            ch_count--;
            ready_cnt = smx_channel_ready_to_read( in[i] );
            if( ready_cnt > 0 )
            {
                ch = in[i];
                *last_idx = i;
                break;
            }
        }
        if( ch == NULL )
        {
            SMX_LOG_NET( h, error,
                    "something went wrong: no msg ready in collector (count: %d)",
                    cur_count );
            return NULL;
        }
        SMX_LOG_NET( h, info, "read from collector (new count: %d)",
                cur_count - 1 );
        smx_profiler_log_net( h, SMX_PROFILER_ACTION_READ_COLLECTOR );
        msg = smx_channel_read( h, ch );
    }
    return msg;
}

/*****************************************************************************/
smx_net_t* smx_net_create( int* net_cnt, unsigned int id, const char* name,
        const char* cat_name, void** conf, pthread_barrier_t* init_done )
{
    if( id >= SMX_MAX_NETS )
    {
        SMX_LOG_MAIN( main, fatal, "net count exeeds maximum %d", id );
        return NULL;
    }

    xmlNodePtr cur = NULL;
    smx_net_t* net = smx_malloc( sizeof( struct smx_net_s ) );
    if( net == NULL )
        return NULL;

    net->sig = smx_malloc( sizeof( struct smx_net_sig_s ) );
    if( net->sig == NULL )
    {
        free( net );
        return NULL;
    }
    net->sig->in.ports = NULL;
    net->sig->in.count = 0;
    net->sig->in.len = 0;
    net->sig->out.ports = NULL;
    net->sig->out.count = 0;
    net->sig->out.len = 0;

    net->id = id;
    net->init_done = init_done;
    net->cat = zlog_get_category( cat_name );
    net->conf = NULL;
    net->profiler = NULL;
    net->name = name;
    net->attr = NULL;

    cur = xmlDocGetRootElement( (xmlDocPtr)*conf );
    cur = cur->xmlChildrenNode;
    while(cur != NULL)
    {
        if(!xmlStrcmp(cur->name, (const xmlChar*)name))
        {
            net->conf = cur;
            break;
        }
        cur = cur->next;
    }

    (*net_cnt)++;
    SMX_LOG_MAIN( net, info, "create net instance %s(%d)", name, id );
    return net;
}

/*****************************************************************************/
void smx_net_destroy( smx_net_t* h )
{
    if( h != NULL )
    {
        if( h->sig != NULL )
        {
            if( h->sig->in.ports != NULL )
                free( h->sig->in.ports );
            if( h->sig->out.ports != NULL )
                free( h->sig->out.ports );
            free( h->sig );
        }
        free( h );
    }
}

/*****************************************************************************/
void smx_net_init( smx_net_t* h, int indegree, int outdegree )
{
    int i;
    if( h == NULL || h->sig == NULL )
        return;

    h->sig->in.len = indegree;
    h->sig->in.ports = smx_malloc( sizeof( smx_channel_t* ) * indegree );
    for( i = 0; i < indegree; i++ )
        h->sig->in.ports[i] = NULL;

    h->sig->out.len = outdegree;
    h->sig->out.ports = smx_malloc( sizeof( smx_channel_t* ) * outdegree );
    for( i = 0; i < indegree; i++ )
        h->sig->in.ports[i] = NULL;
}

/*****************************************************************************/
int smx_net_run( pthread_t* ths, int idx, void* box_impl( void* arg ), void* h,
        int prio )
{
    pthread_attr_t sched_attr;
    struct sched_param fifo_param;
    pthread_t thread;
    int min_fifo, max_fifo;

    if( idx >= SMX_MAX_NETS )
    {
        SMX_LOG_MAIN( main, fatal, "thread count exeeds maximum %d", idx );
        return -1;
    }

    pthread_attr_init( &sched_attr );
    if( prio > 0 )
    {
        min_fifo = sched_get_priority_min( SCHED_FIFO );
        max_fifo = sched_get_priority_max( SCHED_FIFO );
        fifo_param.sched_priority = min_fifo + prio;
        if( fifo_param.sched_priority > max_fifo )
        {
            SMX_LOG_NET( h, warn, "cannot use therad priority of %d, falling back\
                    to the max priority %d", fifo_param.sched_priority,
                    max_fifo );
            fifo_param.sched_priority = max_fifo;
        }
        pthread_attr_setinheritsched(&sched_attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&sched_attr, SCHED_FIFO);
        pthread_attr_setschedparam(&sched_attr, &fifo_param);
        SMX_LOG_NET( h, debug, "creating RT thread of priority %d",
                fifo_param.sched_priority );
    }
    if( ( errno = pthread_create( &thread, &sched_attr, box_impl, h ) ) != 0 )
    {
        SMX_LOG_NET( h, error, "failed to create a new thread: %s",
                strerror( errno ) );
        return -1;
    }
    ths[idx] = thread;
    return 0;
}

/*****************************************************************************/
void* smx_net_start_routine( smx_net_t* h, int impl( void*, void* ),
        int init( void*, void** ), void cleanup( void*, void* ) )
{
    int init_res;
    int state = SMX_NET_CONTINUE;
    void* net_state = NULL;
    xmlChar* profiler = NULL;

    if( h == NULL )
    {
        SMX_LOG_MAIN( main, fatal, "unable to start net: not initialised" );
        return NULL;
    }

    SMX_LOG_NET( h, notice, "init net" );

    if( h != NULL && h->conf != NULL && h->profiler != NULL )
    {
        profiler = xmlGetProp( h->conf, ( const xmlChar* )"profiler" );
        if( profiler != NULL &&
                ( 0 == strcmp( ( char* )profiler, "off" )
                  || 0 == strcmp( ( char* )profiler, "0" ) ) )
        {
            smx_channel_terminate_source( h->profiler );
            smx_collector_terminate( h->profiler );
            h->profiler = NULL;
        }
    }

    if( h->profiler != NULL )
        SMX_LOG_NET( h, notice, "profiler enabled" );

    init_res = init( h, &net_state );
    pthread_barrier_wait( h->init_done );

    if( init_res == 0)
    {
        SMX_LOG_NET( h, notice, "start net" );
        while( state == SMX_NET_CONTINUE )
        {
            SMX_LOG_NET( h, info, "start net loop" );
            smx_profiler_log_net( h, SMX_PROFILER_ACTION_START );
            state = impl( h, net_state );
            state = smx_net_update_state( h, state );
        }
    }
    else
        SMX_LOG_NET( h, error, "initialisation of net failed" );
    smx_net_terminate( h );
    SMX_LOG_NET( h, notice, "cleanup net" );
    cleanup( h, net_state );
    SMX_LOG_NET( h, notice, "terminate net" );
    return NULL;
}

/*****************************************************************************/
void smx_net_terminate( smx_net_t* h )
{
    if( h->sig == NULL || h->sig->in.ports == NULL || h->sig->out.ports == NULL )
    {
        SMX_LOG_MAIN( main, fatal, "net channels not initialised" );
        return;
    }

    int i;
    int len_in = h->sig->in.count;
    int len_out = h->sig->out.count;
    smx_channel_t** chs_in = h->sig->in.ports;
    smx_channel_t** chs_out = h->sig->out.ports;

    SMX_LOG_NET( h, notice, "send termination notice to neighbours" );
    for( i=0; i < len_in; i++ ) {
        if( chs_in[i] == NULL ) continue;
        smx_channel_terminate_sink( chs_in[i] );
    }
    for( i=0; i < len_out; i++ ) {
        if( chs_out[i] == NULL ) continue;
        smx_channel_terminate_source( chs_out[i] );
        smx_collector_terminate( chs_out[i] );
    }
    if( h->profiler != NULL )
    {
        smx_channel_terminate_source( h->profiler );
        smx_collector_terminate( h->profiler );
    }
}

/*****************************************************************************/
int smx_net_update_state( smx_net_t* h, int state )
{
    if( h->sig == NULL || h->sig->in.ports == NULL || h->sig->out.ports == NULL )
    {
        SMX_LOG_MAIN( main, fatal, "net channels not initialised" );
        return SMX_NET_END;
    }

    int i;
    int done_cnt_in = 0;
    int done_cnt_out = 0;
    int trigger_cnt = 0;
    int len_in = h->sig->in.count;
    int len_out = h->sig->out.count;
    smx_channel_t** chs_in = h->sig->in.ports;
    smx_channel_t** chs_out = h->sig->out.ports;

    // if state is forced by box implementation return forced state
    if( state != SMX_NET_RETURN ) return state;

    // check if a triggering input is still producing
    for( i=0; i<len_in; i++ )
    {
        if( chs_in[i] == NULL ) continue;
        if( ( chs_in[i]->type == SMX_FIFO )
                || ( chs_in[i]->type == SMX_D_FIFO ) )
        {
            trigger_cnt++;
            if( ( chs_in[i]->source->state == SMX_CHANNEL_END )
                    && ( chs_in[i]->fifo->count == 0 ) )
                done_cnt_in++;
        }
    }

    // check if consumer is available
    for( i=0; i<len_out; i++ )
    {
        if( chs_out[i] == NULL ) continue;
        if( chs_out[i]->sink->state == SMX_CHANNEL_END )
            done_cnt_out++;
    }

    // if all the triggering inputs are done, terminate the thread
    if( (trigger_cnt > 0) && (done_cnt_in >= trigger_cnt) )
    {
        SMX_LOG_NET( h, debug, "all triggering producers have terminated" );
        return SMX_NET_END;
    }

    // if all the outputs are done, terminate the thread
    if( (len_out) > 0 && (done_cnt_out >= len_out) )
    {
        SMX_LOG_NET( h, debug, "all consumers have terminated" );
        return SMX_NET_END;
    }

    return SMX_NET_CONTINUE;
}
