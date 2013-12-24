# makefile
# Created by IBM WorkFrame/2 MakeMake at 18:17:06 on 26 Feb 1997
#
# The actions included in this make file are:
#  Compile::C++ Compiler
#  Link::Linker

.SUFFIXES: .c .cpp .obj 

.all: \
    .\whoson.exe

.c.obj:
    @echo " Compile::C++ Compiler "
    icc.exe /Ti /Gm /C %s

{G:\download\backroad\cprogs\radius\whoson}.c.obj:
    @echo " Compile::C++ Compiler "
    icc.exe /Ti /Gm /C %s

.cpp.obj:
    @echo " Compile::C++ Compiler "
    icc.exe /Ti /Gm /C %s

{G:\download\backroad\cprogs\radius\whoson}.cpp.obj:
    @echo " Compile::C++ Compiler "
    icc.exe /Ti /Gm /C %s

.\whoson.exe: \
    .\shrmemry.obj \
    .\whoson.obj \
    {$(LIB)}so32dll.lib \
    {$(LIB)}tcp32dll.lib
    @echo " Link::Linker "
    icc.exe @<<
     /B" /de"
     /Fewhoson.exe 
     so32dll.lib 
     tcp32dll.lib 
     .\shrmemry.obj
     .\whoson.obj
<<

.\shrmemry.obj: \
    G:\download\backroad\cprogs\radius\whoson\shrmemry.c

.\whoson.obj: \
    G:\download\backroad\cprogs\radius\whoson\whoson.cpp \
    {G:\download\backroad\cprogs\radius\whoson;$(INCLUDE);}radius.h \
    {G:\download\backroad\cprogs\radius\whoson;$(INCLUDE);}shrMemry.h \
    {G:\download\backroad\cprogs\radius\whoson;$(INCLUDE);}clients.h \
    {G:\download\backroad\cprogs\radius\whoson;$(INCLUDE);}conf.h \
    {G:\download\backroad\cprogs\radius\whoson;$(INCLUDE);}pwd.h \
    {G:\download\backroad\cprogs\radius\whoson;$(INCLUDE);}util.h
