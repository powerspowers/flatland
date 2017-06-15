# Microsoft Developer Studio Project File - Name="Jpeg" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=Jpeg - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Jpeg.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Jpeg.mak" CFG="Jpeg - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Jpeg - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "Jpeg - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "Jpeg"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Jpeg - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /TP /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "Jpeg - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "Jpeg - Win32 Release"
# Name "Jpeg - Win32 Debug"
# Begin Source File

SOURCE=.\Jcomapi.c
# End Source File
# Begin Source File

SOURCE=.\Jdapimin.c
# End Source File
# Begin Source File

SOURCE=.\Jdapistd.c
# End Source File
# Begin Source File

SOURCE=.\Jdatasrc.c
# End Source File
# Begin Source File

SOURCE=.\Jdcoefct.c
# End Source File
# Begin Source File

SOURCE=.\Jdcolor.c
# End Source File
# Begin Source File

SOURCE=.\Jddctmgr.c
# End Source File
# Begin Source File

SOURCE=.\Jdhuff.c
# End Source File
# Begin Source File

SOURCE=.\Jdinput.c
# End Source File
# Begin Source File

SOURCE=.\Jdmainct.c
# End Source File
# Begin Source File

SOURCE=.\Jdmarker.c
# End Source File
# Begin Source File

SOURCE=.\Jdmaster.c
# End Source File
# Begin Source File

SOURCE=.\Jdmerge.c
# End Source File
# Begin Source File

SOURCE=.\Jdphuff.c
# End Source File
# Begin Source File

SOURCE=.\Jdpostct.c
# End Source File
# Begin Source File

SOURCE=.\Jdsample.c
# End Source File
# Begin Source File

SOURCE=.\Jdtrans.c
# End Source File
# Begin Source File

SOURCE=.\Jerror.c
# End Source File
# Begin Source File

SOURCE=.\Jidctflt.c
# End Source File
# Begin Source File

SOURCE=.\Jidctfst.c
# End Source File
# Begin Source File

SOURCE=.\Jidctint.c
# End Source File
# Begin Source File

SOURCE=.\Jidctred.c
# End Source File
# Begin Source File

SOURCE=.\Jmemmgr.c
# End Source File
# Begin Source File

SOURCE=.\Jmemnobs.c
# End Source File
# Begin Source File

SOURCE=.\Jquant1.c
# End Source File
# Begin Source File

SOURCE=.\Jquant2.c
# End Source File
# Begin Source File

SOURCE=.\Jutils.c
# End Source File
# End Target
# End Project
