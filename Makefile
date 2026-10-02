# RBTray -- NMAKE single-architecture Makefile (Release only)
#
# Builds in the current directory (no x86/x64 subdirs):
#   RBTray.exe + RBHook.dll (+ .obj/.res/.pdb/.exp/.lib byproducts)
#
# Architecture is native: it comes from YOUR compiler environment,
# not from this file. The linker infers /MACHINE from cl's objects,
# so do not hardcode /MACHINE:X86 or /MACHINE:X64 here.
#   x86 Native Tools prompt -> 32-bit binaries
#   x64 Native Tools prompt -> 64-bit binaries
# (32-bit hook DLL only sees 32-bit apps, 64-bit only 64-bit apps,
# so run the pair matching your OS/app mix. See AGENTS.md.)
#
# Settings mirror RBTray.sln Release (VS2010 project format, do not
# upgrade ToolsVersion to get these flags -- they are spelled out here):
#   Unicode (/DUNICODE /D_UNICODE), /MT, /W3, /O2 /Oi /Gy /GL + /LTCG,
#   /Zi + /DEBUG, /INCREMENTAL:NO, /OPT:REF /OPT:ICF, /SUBSYSTEM:WINDOWS.
#
# Usage (VS2010 or later, from a Native Tools prompt):
#   nmake           build RBTray.exe + RBHook.dll
#   nmake clean     remove build outputs
#   nmake rebuild   clean + build
#   nmake help      show this help
#
# Older Windows: defaults target Windows 2000 and later (headers pinned to
# 0x0500, /SUBSYSTEM:WINDOWS,5.0). A real Win2000 binary also needs a
# VS2008-or-older toolchain: the VS2010+ static CRT calls EncodePointer,
# missing on 2000. No x64 Win2000 exists, so build x64 with
#   nmake SUBSYS_VER=5.02
# (Server 2003 / XP x64 floor; XP x86 can use SUBSYS_VER=5.01).
#
# Requires cl/link/rc on PATH (e.g. vcvarsall.bat x86  or  x64).
# RBHook.dll must remain next to RBTray.exe at runtime (LoadLibrary).

CC = cl
RC = rc
LINK = link

CFLAGS_COMMON = /nologo /MT /W3 /O2 /Oi /Gy /GL /Zi /DWIN32 /DNDEBUG /D_WINDOWS /DUNICODE /D_UNICODE /DWINVER=0x0500 /D_WIN32_WINNT=0x0500 /D_WIN32_IE=0x0500
CFLAGS_EXE = $(CFLAGS_COMMON) /c
CFLAGS_DLL = $(CFLAGS_COMMON) /D_USRDLL /DRBHOOK_EXPORTS /c

RCFLAGS = /nologo

!IFNDEF SUBSYS_VER
# 5.0 = Windows 2000 floor (x86). Override per-arch from the command line.
SUBSYS_VER = 5.0
!ENDIF

LDFLAGS_COMMON = /NOLOGO /INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF /LTCG /SUBSYSTEM:WINDOWS,$(SUBSYS_VER)
LDFLAGS_EXE = $(LDFLAGS_COMMON)
LDFLAGS_DLL = $(LDFLAGS_COMMON) /DLL

LDLIBS_EXE = shell32.lib user32.lib RBHook.lib
LDLIBS_DLL = kernel32.lib user32.lib

all : RBHook.dll RBTray.exe

RBTray.obj : RBTray.cpp RBTray.h resource.h
	$(CC) $(CFLAGS_EXE) /FdRBTray.pdb RBTray.cpp

RBHook.obj : RBHook.cpp RBTray.h
	$(CC) $(CFLAGS_DLL) /FdRBHook.pdb RBHook.cpp

RBTray.res : RBTray.rc resource.h RBTray.ico
	$(RC) $(RCFLAGS) /foRBTray.res RBTray.rc

RBTray.exe : RBTray.obj RBTray.res RBHook.dll
	$(LINK) $(LDFLAGS_EXE) /OUT:RBTray.exe RBTray.obj RBTray.res $(LDLIBS_EXE)

RBHook.dll : RBHook.obj
	$(LINK) $(LDFLAGS_DLL) /OUT:RBHook.dll RBHook.obj $(LDLIBS_DLL)

clean :
	-del /f RBTray.obj RBHook.obj RBTray.res RBTray.exe RBHook.dll RBTray.lib RBHook.lib RBTray.exp RBHook.exp *.pdb *.ilk 2>NUL

rebuild : clean all

help :
	@echo RBTray single-arch NMAKE targets:
	@echo   nmake           - build RBTray.exe + RBHook.dll (native arch)
	@echo   nmake clean     - remove build outputs
	@echo   nmake rebuild   - clean + build
	@echo   nmake SUBSYS_VER=5.02 - x64 floor (no Win2000 x64 exists)
	@echo Run from an x86 or x64 Native Tools prompt; arch follows cl.
