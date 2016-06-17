#include "boxes.h"
#include "handler.h"
#include <stdio.h>
#include <stdlib.h>

enum COM_STATE { SYN, SYN_ACK, ACK, DONE };

void* box_impl_a( void* handler )
{
    int state = SYN;
    int* data;
    while( state != DONE ) {
        switch( state ) {
            case SYN:
                data = ( int* )SMX_CHANNEL_READ( handler, a, syn );
                printf("in SYN: %d\n", *data );
                state = SYN_ACK;
                break;
            case SYN_ACK:
                *data -= 3;
                SMX_CHANNEL_WRITE( handler, a, syn_ack, ( void* )data );
                state = ACK;
                break;
            case ACK:
                data = ( int* )SMX_CHANNEL_READ( handler, a, ack );
                printf("in ACK: %d\n", *data );
                state = DONE;
                break;
            default:
                state = DONE;
        }
    }
    free( data );
    pthread_exit( NULL );
}

void* box_impl_b( void* handler )
{
    int state = SYN;
    int* data = malloc( sizeof( int ) );
    while( state != DONE ) {
        switch( state ) {
            case SYN:
                *data = 42;
                SMX_CHANNEL_WRITE( handler, b, syn, ( void* )data );
                state = SYN_ACK;
                break;
            case SYN_ACK:
                data = ( int* )SMX_CHANNEL_READ( handler, b, syn_ack );
                printf("in SYN_ACK: %d\n", *data );
                state = ACK;
                break;
            case ACK:
                *data += 5;
                SMX_CHANNEL_WRITE( handler, b, ack, ( void* )data );
                state = DONE;
                break;
            default:
                state = DONE;
        }
    }
    pthread_exit( NULL );
}
