@REM
@REM Copyright (c) Microsoft Corporation.  All rights reserved.
@REM
@REM
@REM Use of this sample source code is subject to the terms of the Microsoft
@REM license agreement under which you licensed this sample source code. If
@REM you did not accept the terms of the license agreement, you are not
@REM authorized to use this sample source code. For the terms of the license,
@REM please see the license agreement between you and Microsoft or, if applicable,
@REM see the LICENSE.RTF on your install media or the root of your tools installation.
@REM THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
@REM
REM Platform specific sysgen settings

if /i not "%1"=="preproc" goto :Not_Preproc
    goto :EOF
:Not_Preproc

REM Pass1. Add new platform specific settings here.
if /i not "%1"=="pass1" goto :Not_Pass1

    if /i "%_TGTPROJ%" == "smartfon" goto :Smartfon
    if /i "%_TGTPROJ%" == "uldr" goto :Uldr
    goto :Common

:Smartfon

    REM OS Specific sysgen flags here
    set SYSGEN_ETHERNET=1
    set SYSGEN_USB=1

    set SYSGEN_MSFLASH_RAMFMD=1
    set SYSGEN_ATAPI_PCIO=1

    REM Pull in the null camera driver
    set SYSGEN_CAMERA_NULL=1

    REM Enable TFAT support
    set SYSGEN_TFAT=1

    REM Enable SD Bus Driver Support
    set SYSGEN_SDBUS=1
    set SYSGEN_SD_MEMORY=1
    set SYSGEN_SDHC_STANDARD=1

    goto :Common

:Uldr

    set SYSGEN_ATAPI_PCIO=1
    set SYSGEN_USB=1
    set SYSGEN_USB_STORAGE=1
    set SYSGEN_SDBUS=1
    set SYSGEN_SD_MEMORY=1
    set SYSGEN_SDHC_STANDARD=1

    goto :Common

:Common
    if "%SYSGEN_USBFN%"=="1"    set SYSGEN_USBFN_NET2280=1
    
    if not "%BSP_NM10%"=="1" goto :not_nm10
      set SYSGEN_USB=1
      set SYSGEN_USB_STORAGE=1
      set SYSGEN_USB_HID_CLIENTS=1
      set SYSGEN_AUDIO=1
      set SYSGEN_SDBUS=1
      set SYSGEN_SD_MEMORY=1
      set SYSGEN_SDHC_STANDARD=1
:not_nm10

    goto :EOF

:Not_Pass1

if /i not "%1"=="pass2" goto :Not_Pass2

    if "%SYSGEN_USBFN%"=="1"    set SYSGEN_USBFN_NET2280=1

    goto :EOF
:Not_Pass2

if /i not "%1"=="report" goto :Not_Report
    goto :EOF

:Not_Report
