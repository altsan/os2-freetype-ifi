#include <os2.h>
#include "ft2mem.h"

/* ---------------------------------------------------------------------------
 * Memory management wrappers
 */

unsigned long safe_alloc( void **object, unsigned long length )
{
#ifdef IFI_DRIVER
    return SSAllocMem( object, length, 0 );
#else
    return DosAllocMem( object, length, PAG_READ | PAG_WRITE | PAG_COMMIT );
#endif
}


unsigned long safe_free( void *object )
{
#ifdef IFI_DRIVER
    return SSFreeMem( object );
#else
    return DosFreeMem( object );
#endif
}
