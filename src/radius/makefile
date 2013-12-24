# makefile
# Created by IBM WorkFrame/2 MakeMake at 14:47:34 on 26 Feb 1997
#
# The actions included in this make file are:
#  Compile::C++ Compiler
#  Link::Linker

.SUFFIXES: .c .obj 

.all: \
    .\radiusd.exe

.c.obj:
    @echo " Compile::C++ Compiler "
    icc.exe /Ti /Gm /C %s

{.}.c.obj:
    @echo " Compile::C++ Compiler "
    icc.exe /Ti /Gm /C %s

.\radiusd.exe: \
    .\util.obj \
    .\version.obj \
    .\tree.obj \
    .\acct.obj \
    .\attrprint.obj \
    .\CRYPT.obj \
    .\crypt_ut.obj \
    .\dict.obj \
    .\getpass.obj \
    .\kill.obj \
    .\md5.obj \
    .\radiusd.obj \
    .\shrmemry.obj \
    .\timer.obj \
    .\users.obj \
    {$(LIB)}so32dll.lib \
    {$(LIB)}tcp32dll.lib
    @echo " Link::Linker "
    icc.exe @<<
     /B" "
     /Feradiusd.exe 
     so32dll.lib 
     tcp32dll.lib 
     .\util.obj
     .\version.obj
     .\tree.obj
     .\acct.obj
     .\attrprint.obj
     .\CRYPT.obj
     .\crypt_ut.obj
     .\dict.obj
     .\getpass.obj
     .\kill.obj
     .\md5.obj
     .\radiusd.obj
     .\shrmemry.obj
     .\timer.obj
     .\users.obj
<<

.\util.obj: \
    .\util.c \
    {.;$(INCLUDE);}radius.h \
    {.;$(INCLUDE);}conf.h \
    {.;$(INCLUDE);}pwd.h \
    {.;$(INCLUDE);}util.h

.\users.obj: \
    .\users.c \
    {.;$(INCLUDE);}radius.h \
    {.;$(INCLUDE);}conf.h \
    {.;$(INCLUDE);}pwd.h \
    {.;$(INCLUDE);}util.h

.\timer.obj: \
    .\timer.c \
    {.;$(INCLUDE);}timer.h

.\shrmemry.obj: \
    .\shrmemry.c

.\radiusd.obj: \
    .\radiusd.c \
    {.;$(INCLUDE);}radius.h \
    {.;$(INCLUDE);}conf.h \
    {.;$(INCLUDE);}pwd.h \
    {.;$(INCLUDE);}util.h \
    {.;$(INCLUDE);}crypt.h

.\md5.obj: \
    .\md5.c \
    {.;$(INCLUDE);}md5.h

.\kill.obj: \
    .\kill.c

.\getpass.obj: \
    .\getpass.c

.\dict.obj: \
    .\dict.c \
    {.;$(INCLUDE);}radius.h \
    {.;$(INCLUDE);}conf.h \
    {.;$(INCLUDE);}pwd.h \
    {.;$(INCLUDE);}util.h

.\crypt_ut.obj: \
    .\crypt_ut.c \
    {.;$(INCLUDE);}ufc-crypt.h \
    {.;$(INCLUDE);}patchlevel.h \
    {.;$(INCLUDE);}pl.h \
    {.;$(INCLUDE);}ufc.h

.\CRYPT.obj: \
    .\CRYPT.C \
    {.;$(INCLUDE);}ufc-crypt.h

.\attrprint.obj: \
    .\attrprint.c \
    {.;$(INCLUDE);}radius.h \
    {.;$(INCLUDE);}conf.h \
    {.;$(INCLUDE);}pwd.h \
    {.;$(INCLUDE);}util.h

.\acct.obj: \
    .\acct.c \
    {.;$(INCLUDE);}radius.h \
    {.;$(INCLUDE);}conf.h \
    {.;$(INCLUDE);}pwd.h \
    {.;$(INCLUDE);}tree.h \
    {.;$(INCLUDE);}util.h \
    {.;$(INCLUDE);}shrMemry.h

.\tree.obj: \
    .\tree.c \
    {.;$(INCLUDE);}tree.h \
    {.;$(INCLUDE);}shrMemry.h

.\version.obj: \
    .\version.c \
    {.;$(INCLUDE);}radius.h \
    {.;$(INCLUDE);}conf.h \
    {.;$(INCLUDE);}pwd.h \
    {.;$(INCLUDE);}util.h
