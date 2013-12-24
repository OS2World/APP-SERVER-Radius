# makefile
# Created by IBM WorkFrame/2 MakeMake at 20:47:42 on 4 Sept 1997
#
# The actions included in this make file are:
#  Compile::C++ Compiler
#  Link::Linker

.SUFFIXES: .c .cpp .obj 

.all: \
    .\runstats.exe

.cpp.obj:
    @echo " Compile::C++ Compiler "
    icc.exe /Ti /Gm /C %s

{.}.cpp.obj:
    @echo " Compile::C++ Compiler "
    icc.exe /Ti /Gm /C %s

.c.obj:
    @echo " Compile::C++ Compiler "
    icc.exe /Ti /Gm /C %s

{.}.c.obj:
    @echo " Compile::C++ Compiler "
    icc.exe /Ti /Gm /C %s

.\runstats.exe: \
    .\runstats.obj \
    .\tree.obj \
    .\shrmemry.obj
    @echo " Link::Linker "
    icc.exe @<<
     /B" /debug "
     /Ferunstats.exe 
     .\tree.obj
     .\runstats.obj
     .\shrmemry.obj
<<

.\runstats.obj: \
    .\runstats.cpp \
    {.;$(INCLUDE);}shrMemry.h \
    {.;$(INCLUDE);}tree.h \
    {.;$(INCLUDE);}clients.h

.\shrmemry.obj: \
    .\shrmemry.c

.\tree.obj: \
    {.;$(INCLUDE);}tree.h \
    .\tree.c
