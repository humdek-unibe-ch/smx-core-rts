/* SPDX-License-Identifier: MPL-2.0 */
/**
 * @author  Simon Maurer
 *
 * Utility functions for the runtime system library of Streamix
 */

#include <errno.h>
#include <string.h>
#include "smxlog.h"
#include "smxutils.h"

/*****************************************************************************/
void* smx_malloc( size_t size )
{
    void* mem = malloc( size );
    if( mem == NULL )
        SMX_LOG_MAIN( main, fatal, "unable to allocate memory: %s",
                strerror( errno ) );
    return mem;
}
