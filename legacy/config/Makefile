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
LIBS   =
NLV    = en

!ifdef DEBUG
    CFLAGS   = $(CFLAGS) /Ti /Tm
    LFLAGS   = $(LFLAGS) /DEBUG
!endif

all         : $(NAME).exe   $(NAME).hlp

$(NAME).exe : $(OBJS) $(MAIN).h $(MRI).h $(MRI).res
                $(LINK) $(LFLAGS) $(OBJS) $(LIBS) /OUT:$@
                $(RC) $(RFLAGS) $(MRI).res $@

$(MRI).res  : $(MRI).rc $(MRI).dlg $(MRI).h ft2gui.ico ftsplash.bmp
                $(RC) -r -n $(MRI).rc $@

$(NAME).hlp: $(NAME)_$(NLV).ipf
                $(IPFC) $(NAME)_$(NLV).ipf $@

clean       :
                -del $(OBJS) $(MRI).res $(NAME).exe $(NAME).hlp >NUL

