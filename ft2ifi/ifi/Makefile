#       Makefile for FTIFI using IBM VisualAge C++
#
# Explanation of compiler switches used:
#  -C        compile only, do not link
#  -Sp1      pack structures on byte boundaries (quite important)
#  -Ss       accept double slash (//) comments
#  -Ge-      build a DLL
#  -Rn       build a subsystem DLL; means that a special C library is
#            linked and some calls cannot be used
#  -Fo<name> create object file <name>
#  -O+       optimizations on
#  -G4       optimize for 486 (should be better for my 6x86MX, produces smaller
#            code than Pentium optimization)
# Linker switches used:
#  /DE       include debug info in executable
#  /NOE      no extended dictionary search
#  /E:2      exepack (for Warp 3 and higher)
#  /A:32     align pages of code on 32-byte boundaries (makes smaller file)
#  /DBGPACK  pack debug info

# uncomment ICCR and LNKR to build a release version
ICCR=-O+ -G4
LNKR=/PACKC /PACKD /M /A:32 /E:2
# uncomment FAKEBOLD to build a Fake Bold Support version.
# uncomment ICCD and LNKD to build a debug version. Note that debug and
# release version is not mutually exclusive in this case.
# ICCD=-Ti+ -DDEBUG
# LNKD=/DE /DBGPACK /M

IFINAME=ft2ifi

MD_AUTHOR  = "Alexander Taylor"
!ifdef ICCD
MD_DESC    = "FT2IFI - OpenType PM font driver based on FreeType 2 (DEBUG)"
!else
MD_DESC    = "FT2IFI - OpenType PM font driver based on FreeType 2"
!endif
MD_VERSION = "2.0.0"

FTSRC=..\lib\freetype-2.5.0.1
FTLIB=..\lib\freetype.lib


$(IFINAME).dll:       $*.obj $*.def ft2kern.obj $(FTLIB)
   makedesc -D$(MD_DESC) -N$(MD_AUTHOR) -V$(MD_VERSION) $*.def $*.def
   ilink /NOE $(LNKD) $(LNKR) $*.obj ft2kern.obj $(FTLIB) libconv.lib $*.def /OUT:$@

$(IFINAME).obj:       ft2ifi.c ft2ifi.h ft2kern.c ft2kern.h ft2mem.h ft2debug.h Makefile
   icc $(ICCD) $(ICCR) -C -Ss -Ge- -Wpar -Rn -Fo$(IFINAME) -I$(FTSRC)\ifi -I$(FTSRC)\builds\os2 -I$(FTSRC)\include ft2ifi.c

ft2kern.obj:         ft2kern.c ft2kern.h Makefile
   icc $(ICCD) $(ICCR) -C -Ss -Ge- -Wpar -Rn -Fo$@ -I$(FTSRC)\ifi -I$(FTSRC)\builds\os2 -I$(FTSRC)\include ft2kern.c

clean:
  -del $(IFINAME).obj
  -del ft2kern.obj
  -del $(IFINAME).map

distclean:          clean
  -del $(IFINAME).dll
