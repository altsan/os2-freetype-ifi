#
# FreeType 2 configuration rules for the OS/2 + VisualAge C++
#
# These rules build a subsystem-safe library for linking into an Installable
# Font Interface presentation driver like FreeType/2.  Besides using the
# OS/2 system functions for I/O, special Presentation Manager APIs are used
# for memory management which enable memory objects to be accessed by all
# PM processes.

# Copyright 1996-2000 by
# David Turner, Robert Wilhelm, and Werner Lemberg.
#
# This file is part of the FreeType project, and may only be used, modified,
# and distributed under the terms of the FreeType project license,
# LICENSE.TXT.  By continuing to use, modify, or distribute this file you
# indicate that you have read the license and understand and accept it
# fully.


# use the runtime-less system interfaces
FTSYS_SRC = $(TOP_DIR)/builds/os2/ifisystem.c

# include OS/2-specific definitions
include $(TOP_DIR)/builds/os2/os2-def.mk

# include gcc-specific definitions
include $(TOP_DIR)/builds/compiler/visualage.mk

# include linking instructions
include $(TOP_DIR)/builds/link_dos.mk


# EOF
