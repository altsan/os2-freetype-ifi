# NMAKE32/IBM CPP (VisualAge) Makefile for FT2CONF
#

CC     = icc.exe
RC     = rc.exe
LINK   = ilink.exe
IPFC   = ipfc.exe
CFLAGS = /Gm /Ss /Q+ /Wuse      # /Wrea /Wuni
RFLAGS = -x -n
LFLAGS = /NOE /PMTYPE:PM /NOLOGO
NAME   = ftconfig
MAIN   = ft2conf
OBJS   = $(MAIN).obj
MRI    = resource

# Help files to build.  Add other languages here and as build targets (below)
# as they become available.
HELPS  = $(NAME).hlp     \
            ftconfde.hlp \
            ftconfes.hlp \
            ftconfru.hlp

MD_AUTHOR  = "Alex Taylor"
MD_DESC    = "FreeType/2 v1.3x configuration GUI"

# Whether to include debug symbols
!ifdef DEBUG
    CFLAGS   = $(CFLAGS) /Ti /Tm
    LFLAGS   = $(LFLAGS) /DEBUG
!endif

all         : $(NAME).exe $(HELPS)

$(NAME).exe : $(OBJS) $(MAIN).h $(MRI).h $(MRI).res
                touch $(NAME).def
                makedesc -D$(MD_DESC) -N$(MD_AUTHOR) -V"^#define=SZ_VERSION,$(MAIN).h" $(NAME).def
                $(LINK) $(LFLAGS) $(OBJS) $(LIBS) $(NAME).def /OUT:$@
                $(RC) $(RFLAGS) $(MRI).res $@

$(MRI).res  : $(MRI).rc $(MRI).dlg $(MRI).h ft2gui.ico ftifi.bmp
                $(RC) -r -n $(MRI).rc $@

$(NAME).hlp: $(NAME)_en.ipf
                $(IPFC) $< $@

ftconfde.hlp: $(NAME)_de.ipf
                $(IPFC) $< $@

ftconfes.hlp: $(NAME)_es.ipf
                $(IPFC) $< $@

ftconfru.hlp: $(NAME)_ru.ipf
                $(IPFC) -D:007 -C:866 $< $@

clean       :
                -del $(OBJS) $(MRI).res $(NAME).exe $(HELPS) >NUL

