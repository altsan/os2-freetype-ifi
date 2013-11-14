#ifndef FT2MEM_H
#define FT2MEM_H

unsigned long safe_alloc( void **object, unsigned long length );
unsigned long safe_free( void *object );

#endif /* FT2MEM_H */
