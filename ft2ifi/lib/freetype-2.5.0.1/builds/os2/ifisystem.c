/***************************************************************************/
/*                                                                         */
/*  ifiystem.c                                                             */
/*                                                                         */
/*    OS/2 IFI-specific FreeType low-level system interface (body).        */
/*    This interface is intended for use by presentation drivers which     */
/*    require the following special considerations:                        */
/*     1. The subsystem runtime is used (this is a special subset of       */
/*        the C library with a number of restrictions)                     */
/*     2. Special Presentation Manager routines are required for memory    */
/*        management, in order to ensure that allocated memory is          */
/*        accessible to all processes as well as to the system itself.     */
/*    This interface is used by the FreeType/2 IFI driver; in principle    */
/*    it might also be useful for other presentation drivers like          */
/*    printer drivers, etc.                                                */
/*    It is NOT for normal application use. OS/2 applications which        */
/*    make use of FreeType directly can use the generic interface.         */
/*                                                                         */
/*  Copyright 1996-2001, 2002, 2006, 2008, 2009, 2010 by                   */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  IFI modifications copyright 2013 Alex Taylor; based in part upon work  */
/*  by Michal Necasek (copyright 1998) and KO Myung-Hun (copyright 2003).  */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/

  /*************************************************************************/
  /*                                                                       */
  /* This file contains the default interface used by FreeType to access   */
  /* low-level, i.e. memory management, i/o access as well as thread       */
  /* synchronisation.  It can be replaced by user-specific routines if     */
  /* necessary.                                                            */
  /*                                                                       */
  /*************************************************************************/


#include <ft2build.h>
#include FT_CONFIG_CONFIG_H
#include FT_INTERNAL_DEBUG_H
#include FT_INTERNAL_STREAM_H
#include FT_SYSTEM_H
#include FT_ERRORS_H
#include FT_TYPES_H

#define INCL_DEV
#include <os2.h>
#include <pmddi.h>


  /*************************************************************************/
  /*                                                                       */
  /*                       MEMORY MANAGEMENT INTERFACE                     */
  /*                                                                       */
  /*************************************************************************/

  /*************************************************************************/
  /*                                                                       */
  /* It is not necessary to do any error checking for the                  */
  /* allocation-related functions.  This will be done by the higher level  */
  /* routines like ft_mem_alloc() or ft_mem_realloc().                     */
  /*                                                                       */
  /*************************************************************************/


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    ft_alloc                                                           */
  /*                                                                       */
  /* <Description>                                                         */
  /*    The memory allocation function.                                    */
  /*                                                                       */
  /* <Input>                                                               */
  /*    memory :: A pointer to the memory object.                          */
  /*                                                                       */
  /*    size   :: The requested size in bytes.                             */
  /*                                                                       */
  /* <Return>                                                              */
  /*    The address of newly allocated block.                              */
  /*                                                                       */
  FT_CALLBACK_DEF( void* )
  ft_alloc( FT_Memory  memory,
            long       size )
  {
    void *buf;
    FT_UNUSED( memory );

    if ( SSAllocMem( (void *)&buf, size, 0 ))
    {
        return NULL;
    }
    return buf;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    ft_realloc                                                         */
  /*                                                                       */
  /* <Description>                                                         */
  /*    The memory reallocation function.                                  */
  /*                                                                       */
  /* <Input>                                                               */
  /*    memory   :: A pointer to the memory object.                        */
  /*                                                                       */
  /*    cur_size :: The current size of the allocated memory block.        */
  /*                                                                       */
  /*    new_size :: The newly requested size in bytes.                     */
  /*                                                                       */
  /*    block    :: The current address of the block in memory.            */
  /*                                                                       */
  /* <Return>                                                              */
  /*    The address of the reallocated memory block.                       */
  /*                                                                       */
  FT_CALLBACK_DEF( void* )
  ft_realloc( FT_Memory  memory,
              long       cur_size,
              long       new_size,
              void*      block )
  {
    void *new_block;
    void *cpaddr;
    int   head,
          rc;

    if ( new_size <= cur_size )
      return block;

    new_block = ft_alloc( memory, new_size );
    if ( !new_block ) {
        return NULL;
    }

    if (( cpaddr = ft_memcpy( new_block, block, cur_size )) == NULL )
    {
      SSFreeMem( (PVOID)new_block );
      return NULL;
    }

    SSFreeMem( (PVOID)block );
    return new_block;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    ft_free                                                            */
  /*                                                                       */
  /* <Description>                                                         */
  /*    The memory release function.                                       */
  /*                                                                       */
  /* <Input>                                                               */
  /*    memory  :: A pointer to the memory object.                         */
  /*                                                                       */
  /*    block   :: The address of block in memory to be freed.             */
  /*                                                                       */
  FT_CALLBACK_DEF( void )
  ft_free( FT_Memory  memory,
           void*      block )
  {
    int rc;
    FT_UNUSED( memory );

    if ( !block )
        return;

    rc = SSFreeMem( (PVOID)block );
    return;
  }


  /*************************************************************************/
  /*                                                                       */
  /*                     RESOURCE MANAGEMENT INTERFACE                     */
  /*                                                                       */
  /*************************************************************************/


  /*************************************************************************/
  /*                                                                       */
  /* The macro FT_COMPONENT is used in trace mode.  It is an implicit      */
  /* parameter of the FT_TRACE() and FT_ERROR() macros, used to print/log  */
  /* messages during execution.                                            */
  /*                                                                       */
#undef  FT_COMPONENT
#define FT_COMPONENT  trace_io

  /* We use the macro STREAM_FILE for convenience to extract the       */
  /* system-specific stream handle from a given FreeType stream object */
#define STREAM_FILE( stream )  ( (HFILE)stream->descriptor.pointer )


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    ft_ansi_stream_close                                               */
  /*                                                                       */
  /* <Description>                                                         */
  /*    The function to close a stream.                                    */
  /*                                                                       */
  /* <Input>                                                               */
  /*    stream :: A pointer to the stream object.                          */
  /*                                                                       */
  FT_CALLBACK_DEF( void )
  ft_ansi_stream_close( FT_Stream  stream )
  {
    DosClose( STREAM_FILE( stream ));
    stream->descriptor.pointer = NULL;
    stream->size               = 0;
    stream->base               = 0;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    ft_ansi_stream_io                                                  */
  /*                                                                       */
  /* <Description>                                                         */
  /*    The function to open a stream.                                     */
  /*                                                                       */
  /* <Input>                                                               */
  /*    stream :: A pointer to the stream object.                          */
  /*                                                                       */
  /*    offset :: The position in the data stream to start reading.        */
  /*                                                                       */
  /*    buffer :: The address of buffer to store the read data.            */
  /*                                                                       */
  /*    count  :: The number of bytes to read from the stream.             */
  /*                                                                       */
  /* <Return>                                                              */
  /*    The number of bytes actually read.  If `count' is zero (this is,   */
  /*    the function is used for seeking), a non-zero return value         */
  /*    indicates an error.                                                */
  /*                                                                       */
  FT_CALLBACK_DEF( unsigned long )
  ft_ansi_stream_io( FT_Stream       stream,
                     unsigned long   offset,
                     unsigned char*  buffer,
                     unsigned long   count )
  {
    HFILE file;
    ULONG cbActual;

    if ( !count && offset > stream->size )
      return 1;

    file = STREAM_FILE( stream );

    if ( stream->pos != offset ) {
      if ( DosSetFilePtr( file, offset, FILE_BEGIN , &cbActual ))
        return (count ? 0 : 1 );
    }
    if ( !count )
       return 0;

    DosRead( file, buffer, count, &cbActual );
    return (unsigned long)cbActual;
  }


  /* documentation is in ftstream.h */

  FT_BASE_DEF( FT_Error )
  FT_Stream_Open( FT_Stream    stream,
                  const char*  filepathname )
  {
    HFILE  file;
    ULONG  ulAction;
    APIRET rc;

    if ( !stream )
      return FT_Err_Invalid_Stream_Handle;

    rc = DosOpen( (char *)filepathname, &file, &ulAction, 0,
                  0, OPEN_ACTION_OPEN_IF_EXISTS,
                  OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY, NULL );

    if ( rc )
    {
      FT_ERROR(( "FT_Stream_Open:"
                 " could not open `%s'\n", filepathname ));

      return FT_Err_Cannot_Open_Resource;
    }

    DosSetFilePtr( file, 0, FILE_END, (ULONG *)&(stream->size) );
    DosSetFilePtr( file, 0, FILE_BEGIN, &ulAction );

    stream->descriptor.pointer = (void*)file;
    stream->pathname.pointer   = (char*)filepathname;
    stream->pos                = 0;

    stream->read  = ft_ansi_stream_io;
    stream->close = ft_ansi_stream_close;

    FT_TRACE1(( "FT_Stream_Open:" ));
    FT_TRACE1(( " opened `%s' (%d bytes) successfully\n",
                filepathname, stream->size ));

    return FT_Err_Ok;
  }


#ifdef FT_DEBUG_MEMORY

  extern FT_Int
  ft_mem_debug_init( FT_Memory  memory );

  extern void
  ft_mem_debug_done( FT_Memory  memory );

#endif


  /* documentation is in ftobjs.h */

  FT_BASE_DEF( FT_Memory )
  FT_New_Memory( void )
  {
    FT_Memory  memory;

    if ( ! SSAllocMem( (void *)&memory, sizeof( FT_Memory ), 0 ))
    {
      memory->user    = 0;
      memory->alloc   = ft_alloc;
      memory->realloc = ft_realloc;
      memory->free    = ft_free;
#ifdef FT_DEBUG_MEMORY
      ft_mem_debug_init( memory );
#endif
    }

    return memory;
  }


  /* documentation is in ftobjs.h */

  FT_BASE_DEF( void )
  FT_Done_Memory( FT_Memory  memory )
  {
#ifdef FT_DEBUG_MEMORY
    ft_mem_debug_done( memory );
#endif
    SSFreeMem( (PVOID)memory );
  }


/* END */
