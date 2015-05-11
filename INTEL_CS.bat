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
@REM -- Intel Copyright Notice -- 
@REM  
@REM @par 
@REM Copyright (c) 2002-2011 Intel Corporation All Rights Reserved. 
@REM  
@REM @par 
@REM The source code contained or described herein and all documents 
@REM related to the source code ("Material") are owned by Intel Corporation 
@REM or its suppliers or licensors.  Title to the Material remains with 
@REM Intel Corporation or its suppliers and licensors. 
@REM  
@REM @par 
@REM The Material is protected by worldwide copyright and trade secret laws 
@REM and treaty provisions. No part of the Material may be used, copied, 
@REM reproduced, modified, published, uploaded, posted, transmitted, 
@REM distributed, or disclosed in any way except in accordance with the 
@REM applicable license agreement . 
@REM  
@REM @par 
@REM No license under any patent, copyright, trade secret or other 
@REM intellectual property right is granted to or conferred upon you by 
@REM disclosure or delivery of the Materials, either expressly, by 
@REM implication, inducement, estoppel, except in accordance with the 
@REM applicable license agreement. 
@REM  
@REM @par 
@REM Unless otherwise agreed by Intel in writing, you may not remove or 
@REM alter this notice or any other notice embedded in Materials by Intel 
@REM or Intel's suppliers or licensors in any way. 
@REM  
@REM @par 
@REM For further details, please see the file README.TXT distributed with 
@REM this software. 
@REM  
@REM @par 
@REM -- End Intel Copyright Notice -- 
@REM  
@REM

@if "%_ECHOON%" == ""   echo off


if /i "%_TGTCPU%"== "x86" goto ValidCPU
    echo.
    echo.
    echo *** ERROR
    echo *** ERROR
    echo ***
    echo ***
    echo *** ERROR: you have set _TGTCPU to %_TGTCPU%, this platform only supports x86 
    echo ***
    echo *** Please close this window and fix
    echo ***
    echo ***
    echo *** ERROR
    echo *** ERROR
    pause
    goto :EOF
:ValidCPU

if /i "%_TGTPROJ%" == "uldr" set _TGTPROJ=uldr
if /i "%_TGTPROJ%" == "cebase" set _TGTPROJ=cebase
goto IsCebase


:IsCebase
    rem CEPC's commonly don't use touch
    set BSP_NOTOUCH=1

:CommonSettings

rem Use a software RTC, this will perform faster, but will drift as GTC has a rounding error:
rem DWORD dwOALTicksPerMs = TIMER_FREQ / 1000;
rem DWORD dwOALTicksPerMs = 1193.18
rem GTC could be corrected to take this into account, but would then leap 2 ms from time to time
rem Of the two evils, the BSP has choosen to have GTC drift by ~150ppm
set IMGSOFTRTC=1

rem CEPC's commonly use a cursor
set BSP_NOCURSOR=


rem CEPC's commonly have a VESA compatible display 
rem Note: The drivers will only be included if Display support is included in the image
set BSP_DISPLAY_FLAT=1


@REM enable DirectDraw and DShow
rem set SYSGEN_DDRAW=1
rem set SYSGEN_DSHOW_DISPLAY=1
rem set SYSGEN_DSHOW_OVMIXER=1
rem set SYSGEN_GDI_ALPHABLEND=1

rem Some BIOS don't like to have their PCI-PCI Bridge config'd by us
set BSP_NOCFGPCIPCIBRIDGE=1

@REM WEC7 is able to automatically detect extra RAM on-board (512MB-3GB)
@REM Unless it is required to fix the memory size of an image, generally,
@REM there is no need to set the following flags   
@REM set IMGRAM256=
set IMGRAM512=

@REM External PCI(e) KITL Ethernet driver.
@REM Note: By default, standard RTL8139 Ethernet is supported
set BSP_KITL_POLL_MODE=1
set BSP_KITL_INTELGBE_E1000=1

@REM Enable Serial Debug at OAL with 
@REM External PCI-e MOSHIP UART Debug Card.  
@REM Note: Enable this will disable on-board COM debug port
@REM Note: Only turn this on if you required an external UART debug card.
set BSP_OAL_SERIALDEBUG_MOSCHIP=

@REM
@REM OS / Middleware Selection. 
@REM
set BSP_WEA=

set WINCEOEM = 1

@REM =========================================================
@REM Supported Platform Selection 
@REM =========================================================
set BSP_NM10=1

    

    @REM =================================================
    @REM Set the flags for NM10 with following
    @REM hardware configurations:-
    @REM 1) High Definition Audio
    @REM 2) SATA
    @REM 3) USB UHCI & EHCI controllers
    @REM 4) 16650-compatible COM port
    @REM =================================================
    if not "%BSP_NM10%"=="1" goto :not_NM10
        @REM Turn-on Multiple Processor  (MP Support) in the build option (IMGMPSUPPORT=1)
        @REM Atom processor has HT support in its core.
        @REM This flag is needed to make sure memory management by kernel
        @REM across different logical processor works correctly. 

        set BSP_ATOM_Cedar=1
        set BSP_UEFI=
        @REM PS/2 keyboard & mouse input. 
        @REM Note: PS/2 keyboard and mouse is not supported in this release
        set BSP_KEYBD_8042=
        set BSP_NOKEYBD=
        set BSP_NOMOUSE=

        @REM NM10 Dev Kit has 16650-compatible Debug COM Port 
        @REM Please make sure the BIOS is setting its IOBase=0x3F8 (IRQ=4) 
        set BSP_SERIAL1=
    
        @REM USB Host Controller Settings 
        @REM Note: Currently, Intel Atom Processor with NM10 board works with public WEC7 USB drivers.
        @REM Reminder: To set catalog for USB Host HID (Keyboard+Mouse & Mass Storage) etc. 
        set BSP_USB_UHCI=1
        set BSP_USB_EHCI=1
		set BSP_USB_XHCI=1

        @REM High Definition Audio
        @REM Note: Set BSP_NOAUDIO to disable audio     
        set BSP_NOAUDIO=
        @REM ICHHDA supports onboard ALC268 codec and AD1986A on Mott Canyon Card
        @REM Note: Audio is not supported in this release
        set BSP_WAVEDEV_ICHHDA=1
        @REM Use Port A(Headphone Out) in onboard ALC262 codec
        @REM Note: Do not use Port A(Headphone Out) on "Intel Atom E3800" platform
        set USE_ALC262_PORTA=
        @REM PATA (IDE) Host Controller
        set BSP_STORAGE_I82371=1
        
        @REM IOH Driver
        set BSP_IOH_GPIO=1
        set BSP_IOH_I2C=1
	set BSP_IOH_SPI=1
	set BSP_IOH_HSUART=1
        
  @REM SparkLAN-Ralink RT2870 USB WIFI dongle 
		@REM Support USB wifi
		set CEPB_INTELE1E_PCIE=1
		set STATIC_IP=1
		set BSP_WNIC_RT2870=1
@REM Enable DMA for baytrail platform
		set BSP_LPSS_DMA=1

    :not_NM10
@REM ==========================================================================================================
@REM Supported Platform Selection New, need to merge with the previous platform selection in the later time
@REM ==========================================================================================================
@REM Multy Platform Settings, customers can set this global variables to decide what platform shall be compiled
@REM Set BSP_PLATFORM=0 means BAYTRAIL platform
@REM Set BSP_PLATFORM=1 means HSWWELL platform
@REM It is the BSP level or top level of the settings of platforms
@REM Developer can add new platform supporting here
@REM ==========================================================================================================
        set BSP_PLATFORM=0
        
        
@REM ==========================================================================================================
@REM CUSTOMER SHOULD NOT EDIT THIS PARTS!!!
@REM It is the beginning of this part.
@REM It is the driver components level or the second level of the settings of platforms
@REM ==========================================================================================================
    if not "%BSP_PLATFORM%"=="0" goto :not_BAYTRAIL
        set BAYTRAIL_GPIO=1
        @REM to be continue
    :not_BAYTRAIL 
    if not "%BSP_PLATFORM%"=="1" goto :not_HASWELL
        set HASWELL_GPIO=1
        set USE_ALC262_PORTA=1
		set HASWELL_SD=1
        @REM to be continue
    :not_HASWELL
    

@REM ==========================================================================================================
@REM CUSTOMER SHOULD NOT EDIT THIS PARTS!!!
@REM It is the end of this part.
@REM ==========================================================================================================


