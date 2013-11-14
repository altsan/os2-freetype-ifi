/***************************************************************************/
/*                                                                         */
/*  ft2build.h                                                             */
/*                                                                         */
/*  Special ft2build.h for use by IFI driver.                              */
/*                                                                         */
/*    FreeType 2 build and setup macros.                                   */
/*    (OS/2 Installable Font Interface version)                            */
/*                                                                         */
/*  Copyright 1996-2001, 2006 by                                           */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
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
  /* This is a modified `ft2build.h' used for building OS/2 IFI drivers.   */
  /* It basically overrides the ftoptions.h and ftmodules.h to disable     */
  /* some components that we don't need.                                   */
  /*                                                                       */
  /*************************************************************************/

#define FT_CONFIG_OPTIONS_H  <ifioption.h>
#define FT_CONFIG_MODULES_H  <ifimodule.h>

/*
#ifndef __FT2_BUILD_GENERIC_H__
#define __FT2_BUILD_GENERIC_H__
*/

#include <freetype/config/ftheader.h>

/*
#endif / * __FT2_BUILD_GENERIC_H__ */


/* END */
