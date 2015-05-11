//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2002-2011 Intel Corporation All Rights Reserved. 
//  
// @par 
// The source code contained or described herein and all documents 
// related to the source code ("Material") are owned by Intel Corporation 
// or its suppliers or licensors.  Title to the Material remains with 
// Intel Corporation or its suppliers and licensors. 
//  
// @par 
// The Material is protected by worldwide copyright and trade secret laws 
// and treaty provisions. No part of the Material may be used, copied, 
// reproduced, modified, published, uploaded, posted, transmitted, 
// distributed, or disclosed in any way except in accordance with the 
// applicable license agreement . 
//  
// @par 
// No license under any patent, copyright, trade secret or other 
// intellectual property right is granted to or conferred upon you by 
// disclosure or delivery of the Materials, either expressly, by 
// implication, inducement, estoppel, except in accordance with the 
// applicable license agreement. 
//  
// @par 
// Unless otherwise agreed by Intel in writing, you may not remove or 
// alter this notice or any other notice embedded in Materials by Intel 
// or Intel's suppliers or licensors in any way. 
//  
// @par 
// For further details, please see the file README.TXT distributed with 
// this software. 
//  
// @par 
// -- End Intel Copyright Notice -- 
//  
//////////////////////////////////////////////////////////////////////////////
/*****************************************************************************

	THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
	ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
	THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
	PARTICULAR PURPOSE.

Module Name:

	Decibels.cpp

Abstract:

	Decibels to Amplification Factor Conversion Routines and look up table

*****************************************************************************/
//////////////////////////////////////////////////////////////////////////////

#include <windows.h>

#define USE_DB_CONV_LOOKUP

#ifdef USE_DB_CONV_LOOKUP
// the table is included for reference but the module uses math
// routines to do the conversions.  Time is never of the essence
// when doing these conversions in Quartz.
//
// This table covers -96.4 dB to -0.1 dB in 0.1 dB units
//
const DWORD tblDBTenthsToAmpFactor[] = {
    0x0000, // (-964/10)dB = 0.000015 * amplitude
    0x0001, // (-963/10)dB = 0.000015 * amplitude
    0x0001, // (-962/10)dB = 0.000015 * amplitude
    0x0001, // (-961/10)dB = 0.000016 * amplitude
    0x0001, // (-960/10)dB = 0.000016 * amplitude
    0x0001, // (-959/10)dB = 0.000016 * amplitude
    0x0001, // (-958/10)dB = 0.000016 * amplitude
    0x0001, // (-957/10)dB = 0.000016 * amplitude
    0x0001, // (-956/10)dB = 0.000017 * amplitude
    0x0001, // (-955/10)dB = 0.000017 * amplitude
    0x0001, // (-954/10)dB = 0.000017 * amplitude
    0x0001, // (-953/10)dB = 0.000017 * amplitude
    0x0001, // (-952/10)dB = 0.000017 * amplitude
    0x0001, // (-951/10)dB = 0.000018 * amplitude
    0x0001, // (-950/10)dB = 0.000018 * amplitude
    0x0001, // (-949/10)dB = 0.000018 * amplitude
    0x0001, // (-948/10)dB = 0.000018 * amplitude
    0x0001, // (-947/10)dB = 0.000018 * amplitude
    0x0001, // (-946/10)dB = 0.000019 * amplitude
    0x0001, // (-945/10)dB = 0.000019 * amplitude
    0x0001, // (-944/10)dB = 0.000019 * amplitude
    0x0001, // (-943/10)dB = 0.000019 * amplitude
    0x0001, // (-942/10)dB = 0.000019 * amplitude
    0x0001, // (-941/10)dB = 0.000020 * amplitude
    0x0001, // (-940/10)dB = 0.000020 * amplitude
    0x0001, // (-939/10)dB = 0.000020 * amplitude
    0x0001, // (-938/10)dB = 0.000020 * amplitude
    0x0001, // (-937/10)dB = 0.000021 * amplitude
    0x0001, // (-936/10)dB = 0.000021 * amplitude
    0x0001, // (-935/10)dB = 0.000021 * amplitude
    0x0001, // (-934/10)dB = 0.000021 * amplitude
    0x0001, // (-933/10)dB = 0.000022 * amplitude
    0x0001, // (-932/10)dB = 0.000022 * amplitude
    0x0001, // (-931/10)dB = 0.000022 * amplitude
    0x0001, // (-930/10)dB = 0.000022 * amplitude
    0x0001, // (-929/10)dB = 0.000023 * amplitude
    0x0001, // (-928/10)dB = 0.000023 * amplitude
    0x0001, // (-927/10)dB = 0.000023 * amplitude
    0x0001, // (-926/10)dB = 0.000023 * amplitude
    0x0001, // (-925/10)dB = 0.000024 * amplitude
    0x0001, // (-924/10)dB = 0.000024 * amplitude
    0x0001, // (-923/10)dB = 0.000024 * amplitude
    0x0001, // (-922/10)dB = 0.000025 * amplitude
    0x0001, // (-921/10)dB = 0.000025 * amplitude
    0x0001, // (-920/10)dB = 0.000025 * amplitude
    0x0001, // (-919/10)dB = 0.000025 * amplitude
    0x0001, // (-918/10)dB = 0.000026 * amplitude
    0x0001, // (-917/10)dB = 0.000026 * amplitude
    0x0001, // (-916/10)dB = 0.000026 * amplitude
    0x0001, // (-915/10)dB = 0.000027 * amplitude
    0x0001, // (-914/10)dB = 0.000027 * amplitude
    0x0001, // (-913/10)dB = 0.000027 * amplitude
    0x0001, // (-912/10)dB = 0.000028 * amplitude
    0x0001, // (-911/10)dB = 0.000028 * amplitude
    0x0001, // (-910/10)dB = 0.000028 * amplitude
    0x0001, // (-909/10)dB = 0.000029 * amplitude
    0x0001, // (-908/10)dB = 0.000029 * amplitude
    0x0001, // (-907/10)dB = 0.000029 * amplitude
    0x0001, // (-906/10)dB = 0.000030 * amplitude
    0x0001, // (-905/10)dB = 0.000030 * amplitude
    0x0001, // (-904/10)dB = 0.000030 * amplitude
    0x0002, // (-903/10)dB = 0.000031 * amplitude
    0x0002, // (-902/10)dB = 0.000031 * amplitude
    0x0002, // (-901/10)dB = 0.000031 * amplitude
    0x0002, // (-900/10)dB = 0.000032 * amplitude
    0x0002, // (-899/10)dB = 0.000032 * amplitude
    0x0002, // (-898/10)dB = 0.000032 * amplitude
    0x0002, // (-897/10)dB = 0.000033 * amplitude
    0x0002, // (-896/10)dB = 0.000033 * amplitude
    0x0002, // (-895/10)dB = 0.000033 * amplitude
    0x0002, // (-894/10)dB = 0.000034 * amplitude
    0x0002, // (-893/10)dB = 0.000034 * amplitude
    0x0002, // (-892/10)dB = 0.000035 * amplitude
    0x0002, // (-891/10)dB = 0.000035 * amplitude
    0x0002, // (-890/10)dB = 0.000035 * amplitude
    0x0002, // (-889/10)dB = 0.000036 * amplitude
    0x0002, // (-888/10)dB = 0.000036 * amplitude
    0x0002, // (-887/10)dB = 0.000037 * amplitude
    0x0002, // (-886/10)dB = 0.000037 * amplitude
    0x0002, // (-885/10)dB = 0.000038 * amplitude
    0x0002, // (-884/10)dB = 0.000038 * amplitude
    0x0002, // (-883/10)dB = 0.000038 * amplitude
    0x0002, // (-882/10)dB = 0.000039 * amplitude
    0x0002, // (-881/10)dB = 0.000039 * amplitude
    0x0002, // (-880/10)dB = 0.000040 * amplitude
    0x0002, // (-879/10)dB = 0.000040 * amplitude
    0x0002, // (-878/10)dB = 0.000041 * amplitude
    0x0002, // (-877/10)dB = 0.000041 * amplitude
    0x0002, // (-876/10)dB = 0.000042 * amplitude
    0x0002, // (-875/10)dB = 0.000042 * amplitude
    0x0002, // (-874/10)dB = 0.000043 * amplitude
    0x0002, // (-873/10)dB = 0.000043 * amplitude
    0x0002, // (-872/10)dB = 0.000044 * amplitude
    0x0002, // (-871/10)dB = 0.000044 * amplitude
    0x0002, // (-870/10)dB = 0.000045 * amplitude
    0x0002, // (-869/10)dB = 0.000045 * amplitude
    0x0002, // (-868/10)dB = 0.000046 * amplitude
    0x0003, // (-867/10)dB = 0.000046 * amplitude
    0x0003, // (-866/10)dB = 0.000047 * amplitude
    0x0003, // (-865/10)dB = 0.000047 * amplitude
    0x0003, // (-864/10)dB = 0.000048 * amplitude
    0x0003, // (-863/10)dB = 0.000048 * amplitude
    0x0003, // (-862/10)dB = 0.000049 * amplitude
    0x0003, // (-861/10)dB = 0.000050 * amplitude
    0x0003, // (-860/10)dB = 0.000050 * amplitude
    0x0003, // (-859/10)dB = 0.000051 * amplitude
    0x0003, // (-858/10)dB = 0.000051 * amplitude
    0x0003, // (-857/10)dB = 0.000052 * amplitude
    0x0003, // (-856/10)dB = 0.000052 * amplitude
    0x0003, // (-855/10)dB = 0.000053 * amplitude
    0x0003, // (-854/10)dB = 0.000054 * amplitude
    0x0003, // (-853/10)dB = 0.000054 * amplitude
    0x0003, // (-852/10)dB = 0.000055 * amplitude
    0x0003, // (-851/10)dB = 0.000056 * amplitude
    0x0003, // (-850/10)dB = 0.000056 * amplitude
    0x0003, // (-849/10)dB = 0.000057 * amplitude
    0x0003, // (-848/10)dB = 0.000058 * amplitude
    0x0003, // (-847/10)dB = 0.000058 * amplitude
    0x0003, // (-846/10)dB = 0.000059 * amplitude
    0x0003, // (-845/10)dB = 0.000060 * amplitude
    0x0003, // (-844/10)dB = 0.000060 * amplitude
    0x0003, // (-843/10)dB = 0.000061 * amplitude
    0x0004, // (-842/10)dB = 0.000062 * amplitude
    0x0004, // (-841/10)dB = 0.000062 * amplitude
    0x0004, // (-840/10)dB = 0.000063 * amplitude
    0x0004, // (-839/10)dB = 0.000064 * amplitude
    0x0004, // (-838/10)dB = 0.000065 * amplitude
    0x0004, // (-837/10)dB = 0.000065 * amplitude
    0x0004, // (-836/10)dB = 0.000066 * amplitude
    0x0004, // (-835/10)dB = 0.000067 * amplitude
    0x0004, // (-834/10)dB = 0.000068 * amplitude
    0x0004, // (-833/10)dB = 0.000068 * amplitude
    0x0004, // (-832/10)dB = 0.000069 * amplitude
    0x0004, // (-831/10)dB = 0.000070 * amplitude
    0x0004, // (-830/10)dB = 0.000071 * amplitude
    0x0004, // (-829/10)dB = 0.000072 * amplitude
    0x0004, // (-828/10)dB = 0.000072 * amplitude
    0x0004, // (-827/10)dB = 0.000073 * amplitude
    0x0004, // (-826/10)dB = 0.000074 * amplitude
    0x0004, // (-825/10)dB = 0.000075 * amplitude
    0x0004, // (-824/10)dB = 0.000076 * amplitude
    0x0005, // (-823/10)dB = 0.000077 * amplitude
    0x0005, // (-822/10)dB = 0.000078 * amplitude
    0x0005, // (-821/10)dB = 0.000079 * amplitude
    0x0005, // (-820/10)dB = 0.000079 * amplitude
    0x0005, // (-819/10)dB = 0.000080 * amplitude
    0x0005, // (-818/10)dB = 0.000081 * amplitude
    0x0005, // (-817/10)dB = 0.000082 * amplitude
    0x0005, // (-816/10)dB = 0.000083 * amplitude
    0x0005, // (-815/10)dB = 0.000084 * amplitude
    0x0005, // (-814/10)dB = 0.000085 * amplitude
    0x0005, // (-813/10)dB = 0.000086 * amplitude
    0x0005, // (-812/10)dB = 0.000087 * amplitude
    0x0005, // (-811/10)dB = 0.000088 * amplitude
    0x0005, // (-810/10)dB = 0.000089 * amplitude
    0x0005, // (-809/10)dB = 0.000090 * amplitude
    0x0005, // (-808/10)dB = 0.000091 * amplitude
    0x0006, // (-807/10)dB = 0.000092 * amplitude
    0x0006, // (-806/10)dB = 0.000093 * amplitude
    0x0006, // (-805/10)dB = 0.000094 * amplitude
    0x0006, // (-804/10)dB = 0.000095 * amplitude
    0x0006, // (-803/10)dB = 0.000097 * amplitude
    0x0006, // (-802/10)dB = 0.000098 * amplitude
    0x0006, // (-801/10)dB = 0.000099 * amplitude
    0x0006, // (-800/10)dB = 0.000100 * amplitude
    0x0006, // (-799/10)dB = 0.000101 * amplitude
    0x0006, // (-798/10)dB = 0.000102 * amplitude
    0x0006, // (-797/10)dB = 0.000104 * amplitude
    0x0006, // (-796/10)dB = 0.000105 * amplitude
    0x0006, // (-795/10)dB = 0.000106 * amplitude
    0x0007, // (-794/10)dB = 0.000107 * amplitude
    0x0007, // (-793/10)dB = 0.000108 * amplitude
    0x0007, // (-792/10)dB = 0.000110 * amplitude
    0x0007, // (-791/10)dB = 0.000111 * amplitude
    0x0007, // (-790/10)dB = 0.000112 * amplitude
    0x0007, // (-789/10)dB = 0.000114 * amplitude
    0x0007, // (-788/10)dB = 0.000115 * amplitude
    0x0007, // (-787/10)dB = 0.000116 * amplitude
    0x0007, // (-786/10)dB = 0.000117 * amplitude
    0x0007, // (-785/10)dB = 0.000119 * amplitude
    0x0007, // (-784/10)dB = 0.000120 * amplitude
    0x0007, // (-783/10)dB = 0.000122 * amplitude
    0x0008, // (-782/10)dB = 0.000123 * amplitude
    0x0008, // (-781/10)dB = 0.000124 * amplitude
    0x0008, // (-780/10)dB = 0.000126 * amplitude
    0x0008, // (-779/10)dB = 0.000127 * amplitude
    0x0008, // (-778/10)dB = 0.000129 * amplitude
    0x0008, // (-777/10)dB = 0.000130 * amplitude
    0x0008, // (-776/10)dB = 0.000132 * amplitude
    0x0008, // (-775/10)dB = 0.000133 * amplitude
    0x0008, // (-774/10)dB = 0.000135 * amplitude
    0x0008, // (-773/10)dB = 0.000136 * amplitude
    0x0009, // (-772/10)dB = 0.000138 * amplitude
    0x0009, // (-771/10)dB = 0.000140 * amplitude
    0x0009, // (-770/10)dB = 0.000141 * amplitude
    0x0009, // (-769/10)dB = 0.000143 * amplitude
    0x0009, // (-768/10)dB = 0.000145 * amplitude
    0x0009, // (-767/10)dB = 0.000146 * amplitude
    0x0009, // (-766/10)dB = 0.000148 * amplitude
    0x0009, // (-765/10)dB = 0.000150 * amplitude
    0x0009, // (-764/10)dB = 0.000151 * amplitude
    0x000A, // (-763/10)dB = 0.000153 * amplitude
    0x000A, // (-762/10)dB = 0.000155 * amplitude
    0x000A, // (-761/10)dB = 0.000157 * amplitude
    0x000A, // (-760/10)dB = 0.000158 * amplitude
    0x000A, // (-759/10)dB = 0.000160 * amplitude
    0x000A, // (-758/10)dB = 0.000162 * amplitude
    0x000A, // (-757/10)dB = 0.000164 * amplitude
    0x000A, // (-756/10)dB = 0.000166 * amplitude
    0x000B, // (-755/10)dB = 0.000168 * amplitude
    0x000B, // (-754/10)dB = 0.000170 * amplitude
    0x000B, // (-753/10)dB = 0.000172 * amplitude
    0x000B, // (-752/10)dB = 0.000174 * amplitude
    0x000B, // (-751/10)dB = 0.000176 * amplitude
    0x000B, // (-750/10)dB = 0.000178 * amplitude
    0x000B, // (-749/10)dB = 0.000180 * amplitude
    0x000B, // (-748/10)dB = 0.000182 * amplitude
    0x000C, // (-747/10)dB = 0.000184 * amplitude
    0x000C, // (-746/10)dB = 0.000186 * amplitude
    0x000C, // (-745/10)dB = 0.000188 * amplitude
    0x000C, // (-744/10)dB = 0.000191 * amplitude
    0x000C, // (-743/10)dB = 0.000193 * amplitude
    0x000C, // (-742/10)dB = 0.000195 * amplitude
    0x000C, // (-741/10)dB = 0.000197 * amplitude
    0x000D, // (-740/10)dB = 0.000200 * amplitude
    0x000D, // (-739/10)dB = 0.000202 * amplitude
    0x000D, // (-738/10)dB = 0.000204 * amplitude
    0x000D, // (-737/10)dB = 0.000207 * amplitude
    0x000D, // (-736/10)dB = 0.000209 * amplitude
    0x000D, // (-735/10)dB = 0.000211 * amplitude
    0x000E, // (-734/10)dB = 0.000214 * amplitude
    0x000E, // (-733/10)dB = 0.000216 * amplitude
    0x000E, // (-732/10)dB = 0.000219 * amplitude
    0x000E, // (-731/10)dB = 0.000221 * amplitude
    0x000E, // (-730/10)dB = 0.000224 * amplitude
    0x000E, // (-729/10)dB = 0.000226 * amplitude
    0x000F, // (-728/10)dB = 0.000229 * amplitude
    0x000F, // (-727/10)dB = 0.000232 * amplitude
    0x000F, // (-726/10)dB = 0.000234 * amplitude
    0x000F, // (-725/10)dB = 0.000237 * amplitude
    0x000F, // (-724/10)dB = 0.000240 * amplitude
    0x000F, // (-723/10)dB = 0.000243 * amplitude
    0x0010, // (-722/10)dB = 0.000245 * amplitude
    0x0010, // (-721/10)dB = 0.000248 * amplitude
    0x0010, // (-720/10)dB = 0.000251 * amplitude
    0x0010, // (-719/10)dB = 0.000254 * amplitude
    0x0010, // (-718/10)dB = 0.000257 * amplitude
    0x0011, // (-717/10)dB = 0.000260 * amplitude
    0x0011, // (-716/10)dB = 0.000263 * amplitude
    0x0011, // (-715/10)dB = 0.000266 * amplitude
    0x0011, // (-714/10)dB = 0.000269 * amplitude
    0x0011, // (-713/10)dB = 0.000272 * amplitude
    0x0012, // (-712/10)dB = 0.000275 * amplitude
    0x0012, // (-711/10)dB = 0.000279 * amplitude
    0x0012, // (-710/10)dB = 0.000282 * amplitude
    0x0012, // (-709/10)dB = 0.000285 * amplitude
    0x0012, // (-708/10)dB = 0.000288 * amplitude
    0x0013, // (-707/10)dB = 0.000292 * amplitude
    0x0013, // (-706/10)dB = 0.000295 * amplitude
    0x0013, // (-705/10)dB = 0.000299 * amplitude
    0x0013, // (-704/10)dB = 0.000302 * amplitude
    0x0014, // (-703/10)dB = 0.000305 * amplitude
    0x0014, // (-702/10)dB = 0.000309 * amplitude
    0x0014, // (-701/10)dB = 0.000313 * amplitude
    0x0014, // (-700/10)dB = 0.000316 * amplitude
    0x0014, // (-699/10)dB = 0.000320 * amplitude
    0x0015, // (-698/10)dB = 0.000324 * amplitude
    0x0015, // (-697/10)dB = 0.000327 * amplitude
    0x0015, // (-696/10)dB = 0.000331 * amplitude
    0x0015, // (-695/10)dB = 0.000335 * amplitude
    0x0016, // (-694/10)dB = 0.000339 * amplitude
    0x0016, // (-693/10)dB = 0.000343 * amplitude
    0x0016, // (-692/10)dB = 0.000347 * amplitude
    0x0016, // (-691/10)dB = 0.000351 * amplitude
    0x0017, // (-690/10)dB = 0.000355 * amplitude
    0x0017, // (-689/10)dB = 0.000359 * amplitude
    0x0017, // (-688/10)dB = 0.000363 * amplitude
    0x0018, // (-687/10)dB = 0.000367 * amplitude
    0x0018, // (-686/10)dB = 0.000372 * amplitude
    0x0018, // (-685/10)dB = 0.000376 * amplitude
    0x0018, // (-684/10)dB = 0.000380 * amplitude
    0x0019, // (-683/10)dB = 0.000385 * amplitude
    0x0019, // (-682/10)dB = 0.000389 * amplitude
    0x0019, // (-681/10)dB = 0.000394 * amplitude
    0x001A, // (-680/10)dB = 0.000398 * amplitude
    0x001A, // (-679/10)dB = 0.000403 * amplitude
    0x001A, // (-678/10)dB = 0.000407 * amplitude
    0x001B, // (-677/10)dB = 0.000412 * amplitude
    0x001B, // (-676/10)dB = 0.000417 * amplitude
    0x001B, // (-675/10)dB = 0.000422 * amplitude
    0x001B, // (-674/10)dB = 0.000427 * amplitude
    0x001C, // (-673/10)dB = 0.000432 * amplitude
    0x001C, // (-672/10)dB = 0.000437 * amplitude
    0x001C, // (-671/10)dB = 0.000442 * amplitude
    0x001D, // (-670/10)dB = 0.000447 * amplitude
    0x001D, // (-669/10)dB = 0.000452 * amplitude
    0x001D, // (-668/10)dB = 0.000457 * amplitude
    0x001E, // (-667/10)dB = 0.000462 * amplitude
    0x001E, // (-666/10)dB = 0.000468 * amplitude
    0x001F, // (-665/10)dB = 0.000473 * amplitude
    0x001F, // (-664/10)dB = 0.000479 * amplitude
    0x001F, // (-663/10)dB = 0.000484 * amplitude
    0x0020, // (-662/10)dB = 0.000490 * amplitude
    0x0020, // (-661/10)dB = 0.000495 * amplitude
    0x0020, // (-660/10)dB = 0.000501 * amplitude
    0x0021, // (-659/10)dB = 0.000507 * amplitude
    0x0021, // (-658/10)dB = 0.000513 * amplitude
    0x0022, // (-657/10)dB = 0.000519 * amplitude
    0x0022, // (-656/10)dB = 0.000525 * amplitude
    0x0022, // (-655/10)dB = 0.000531 * amplitude
    0x0023, // (-654/10)dB = 0.000537 * amplitude
    0x0023, // (-653/10)dB = 0.000543 * amplitude
    0x0024, // (-652/10)dB = 0.000550 * amplitude
    0x0024, // (-651/10)dB = 0.000556 * amplitude
    0x0024, // (-650/10)dB = 0.000562 * amplitude
    0x0025, // (-649/10)dB = 0.000569 * amplitude
    0x0025, // (-648/10)dB = 0.000575 * amplitude
    0x0026, // (-647/10)dB = 0.000582 * amplitude
    0x0026, // (-646/10)dB = 0.000589 * amplitude
    0x0027, // (-645/10)dB = 0.000596 * amplitude
    0x0027, // (-644/10)dB = 0.000603 * amplitude
    0x0027, // (-643/10)dB = 0.000610 * amplitude
    0x0028, // (-642/10)dB = 0.000617 * amplitude
    0x0028, // (-641/10)dB = 0.000624 * amplitude
    0x0029, // (-640/10)dB = 0.000631 * amplitude
    0x0029, // (-639/10)dB = 0.000638 * amplitude
    0x002A, // (-638/10)dB = 0.000646 * amplitude
    0x002A, // (-637/10)dB = 0.000653 * amplitude
    0x002B, // (-636/10)dB = 0.000661 * amplitude
    0x002B, // (-635/10)dB = 0.000668 * amplitude
    0x002C, // (-634/10)dB = 0.000676 * amplitude
    0x002C, // (-633/10)dB = 0.000684 * amplitude
    0x002D, // (-632/10)dB = 0.000692 * amplitude
    0x002D, // (-631/10)dB = 0.000700 * amplitude
    0x002E, // (-630/10)dB = 0.000708 * amplitude
    0x002E, // (-629/10)dB = 0.000716 * amplitude
    0x002F, // (-628/10)dB = 0.000724 * amplitude
    0x0030, // (-627/10)dB = 0.000733 * amplitude
    0x0030, // (-626/10)dB = 0.000741 * amplitude
    0x0031, // (-625/10)dB = 0.000750 * amplitude
    0x0031, // (-624/10)dB = 0.000759 * amplitude
    0x0032, // (-623/10)dB = 0.000767 * amplitude
    0x0032, // (-622/10)dB = 0.000776 * amplitude
    0x0033, // (-621/10)dB = 0.000785 * amplitude
    0x0034, // (-620/10)dB = 0.000794 * amplitude
    0x0034, // (-619/10)dB = 0.000804 * amplitude
    0x0035, // (-618/10)dB = 0.000813 * amplitude
    0x0035, // (-617/10)dB = 0.000822 * amplitude
    0x0036, // (-616/10)dB = 0.000832 * amplitude
    0x0037, // (-615/10)dB = 0.000841 * amplitude
    0x0037, // (-614/10)dB = 0.000851 * amplitude
    0x0038, // (-613/10)dB = 0.000861 * amplitude
    0x0039, // (-612/10)dB = 0.000871 * amplitude
    0x0039, // (-611/10)dB = 0.000881 * amplitude
    0x003A, // (-610/10)dB = 0.000891 * amplitude
    0x003B, // (-609/10)dB = 0.000902 * amplitude
    0x003B, // (-608/10)dB = 0.000912 * amplitude
    0x003C, // (-607/10)dB = 0.000923 * amplitude
    0x003D, // (-606/10)dB = 0.000933 * amplitude
    0x003D, // (-605/10)dB = 0.000944 * amplitude
    0x003E, // (-604/10)dB = 0.000955 * amplitude
    0x003F, // (-603/10)dB = 0.000966 * amplitude
    0x0040, // (-602/10)dB = 0.000977 * amplitude
    0x0040, // (-601/10)dB = 0.000989 * amplitude
    0x0041, // (-600/10)dB = 0.001000 * amplitude
    0x0042, // (-599/10)dB = 0.001012 * amplitude
    0x0043, // (-598/10)dB = 0.001023 * amplitude
    0x0043, // (-597/10)dB = 0.001035 * amplitude
    0x0044, // (-596/10)dB = 0.001047 * amplitude
    0x0045, // (-595/10)dB = 0.001059 * amplitude
    0x0046, // (-594/10)dB = 0.001072 * amplitude
    0x0047, // (-593/10)dB = 0.001084 * amplitude
    0x0047, // (-592/10)dB = 0.001096 * amplitude
    0x0048, // (-591/10)dB = 0.001109 * amplitude
    0x0049, // (-590/10)dB = 0.001122 * amplitude
    0x004A, // (-589/10)dB = 0.001135 * amplitude
    0x004B, // (-588/10)dB = 0.001148 * amplitude
    0x004C, // (-587/10)dB = 0.001161 * amplitude
    0x004C, // (-586/10)dB = 0.001175 * amplitude
    0x004D, // (-585/10)dB = 0.001189 * amplitude
    0x004E, // (-584/10)dB = 0.001202 * amplitude
    0x004F, // (-583/10)dB = 0.001216 * amplitude
    0x0050, // (-582/10)dB = 0.001230 * amplitude
    0x0051, // (-581/10)dB = 0.001245 * amplitude
    0x0052, // (-580/10)dB = 0.001259 * amplitude
    0x0053, // (-579/10)dB = 0.001274 * amplitude
    0x0054, // (-578/10)dB = 0.001288 * amplitude
    0x0055, // (-577/10)dB = 0.001303 * amplitude
    0x0056, // (-576/10)dB = 0.001318 * amplitude
    0x0057, // (-575/10)dB = 0.001334 * amplitude
    0x0058, // (-574/10)dB = 0.001349 * amplitude
    0x0059, // (-573/10)dB = 0.001365 * amplitude
    0x005A, // (-572/10)dB = 0.001380 * amplitude
    0x005B, // (-571/10)dB = 0.001396 * amplitude
    0x005C, // (-570/10)dB = 0.001413 * amplitude
    0x005D, // (-569/10)dB = 0.001429 * amplitude
    0x005E, // (-568/10)dB = 0.001445 * amplitude
    0x005F, // (-567/10)dB = 0.001462 * amplitude
    0x0060, // (-566/10)dB = 0.001479 * amplitude
    0x0062, // (-565/10)dB = 0.001496 * amplitude
    0x0063, // (-564/10)dB = 0.001514 * amplitude
    0x0064, // (-563/10)dB = 0.001531 * amplitude
    0x0065, // (-562/10)dB = 0.001549 * amplitude
    0x0066, // (-561/10)dB = 0.001567 * amplitude
    0x0067, // (-560/10)dB = 0.001585 * amplitude
    0x0069, // (-559/10)dB = 0.001603 * amplitude
    0x006A, // (-558/10)dB = 0.001622 * amplitude
    0x006B, // (-557/10)dB = 0.001641 * amplitude
    0x006C, // (-556/10)dB = 0.001660 * amplitude
    0x006E, // (-555/10)dB = 0.001679 * amplitude
    0x006F, // (-554/10)dB = 0.001698 * amplitude
    0x0070, // (-553/10)dB = 0.001718 * amplitude
    0x0071, // (-552/10)dB = 0.001738 * amplitude
    0x0073, // (-551/10)dB = 0.001758 * amplitude
    0x0074, // (-550/10)dB = 0.001778 * amplitude
    0x0075, // (-549/10)dB = 0.001799 * amplitude
    0x0077, // (-548/10)dB = 0.001820 * amplitude
    0x0078, // (-547/10)dB = 0.001841 * amplitude
    0x007A, // (-546/10)dB = 0.001862 * amplitude
    0x007B, // (-545/10)dB = 0.001884 * amplitude
    0x007C, // (-544/10)dB = 0.001905 * amplitude
    0x007E, // (-543/10)dB = 0.001928 * amplitude
    0x007F, // (-542/10)dB = 0.001950 * amplitude
    0x0081, // (-541/10)dB = 0.001972 * amplitude
    0x0082, // (-540/10)dB = 0.001995 * amplitude
    0x0084, // (-539/10)dB = 0.002018 * amplitude
    0x0085, // (-538/10)dB = 0.002042 * amplitude
    0x0087, // (-537/10)dB = 0.002065 * amplitude
    0x0088, // (-536/10)dB = 0.002089 * amplitude
    0x008A, // (-535/10)dB = 0.002113 * amplitude
    0x008C, // (-534/10)dB = 0.002138 * amplitude
    0x008D, // (-533/10)dB = 0.002163 * amplitude
    0x008F, // (-532/10)dB = 0.002188 * amplitude
    0x0091, // (-531/10)dB = 0.002213 * amplitude
    0x0092, // (-530/10)dB = 0.002239 * amplitude
    0x0094, // (-529/10)dB = 0.002265 * amplitude
    0x0096, // (-528/10)dB = 0.002291 * amplitude
    0x0097, // (-527/10)dB = 0.002317 * amplitude
    0x0099, // (-526/10)dB = 0.002344 * amplitude
    0x009B, // (-525/10)dB = 0.002371 * amplitude
    0x009D, // (-524/10)dB = 0.002399 * amplitude
    0x009F, // (-523/10)dB = 0.002427 * amplitude
    0x00A0, // (-522/10)dB = 0.002455 * amplitude
    0x00A2, // (-521/10)dB = 0.002483 * amplitude
    0x00A4, // (-520/10)dB = 0.002512 * amplitude
    0x00A6, // (-519/10)dB = 0.002541 * amplitude
    0x00A8, // (-518/10)dB = 0.002570 * amplitude
    0x00AA, // (-517/10)dB = 0.002600 * amplitude
    0x00AC, // (-516/10)dB = 0.002630 * amplitude
    0x00AE, // (-515/10)dB = 0.002661 * amplitude
    0x00B0, // (-514/10)dB = 0.002692 * amplitude
    0x00B2, // (-513/10)dB = 0.002723 * amplitude
    0x00B4, // (-512/10)dB = 0.002754 * amplitude
    0x00B6, // (-511/10)dB = 0.002786 * amplitude
    0x00B8, // (-510/10)dB = 0.002818 * amplitude
    0x00BA, // (-509/10)dB = 0.002851 * amplitude
    0x00BD, // (-508/10)dB = 0.002884 * amplitude
    0x00BF, // (-507/10)dB = 0.002917 * amplitude
    0x00C1, // (-506/10)dB = 0.002951 * amplitude
    0x00C3, // (-505/10)dB = 0.002985 * amplitude
    0x00C5, // (-504/10)dB = 0.003020 * amplitude
    0x00C8, // (-503/10)dB = 0.003055 * amplitude
    0x00CA, // (-502/10)dB = 0.003090 * amplitude
    0x00CC, // (-501/10)dB = 0.003126 * amplitude
    0x00CF, // (-500/10)dB = 0.003162 * amplitude
    0x00D1, // (-499/10)dB = 0.003199 * amplitude
    0x00D4, // (-498/10)dB = 0.003236 * amplitude
    0x00D6, // (-497/10)dB = 0.003273 * amplitude
    0x00D9, // (-496/10)dB = 0.003311 * amplitude
    0x00DB, // (-495/10)dB = 0.003350 * amplitude
    0x00DE, // (-494/10)dB = 0.003388 * amplitude
    0x00E0, // (-493/10)dB = 0.003428 * amplitude
    0x00E3, // (-492/10)dB = 0.003467 * amplitude
    0x00E5, // (-491/10)dB = 0.003508 * amplitude
    0x00E8, // (-490/10)dB = 0.003548 * amplitude
    0x00EB, // (-489/10)dB = 0.003589 * amplitude
    0x00ED, // (-488/10)dB = 0.003631 * amplitude
    0x00F0, // (-487/10)dB = 0.003673 * amplitude
    0x00F3, // (-486/10)dB = 0.003715 * amplitude
    0x00F6, // (-485/10)dB = 0.003758 * amplitude
    0x00F9, // (-484/10)dB = 0.003802 * amplitude
    0x00FC, // (-483/10)dB = 0.003846 * amplitude
    0x00FE, // (-482/10)dB = 0.003890 * amplitude
    0x0101, // (-481/10)dB = 0.003936 * amplitude
    0x0104, // (-480/10)dB = 0.003981 * amplitude
    0x0107, // (-479/10)dB = 0.004027 * amplitude
    0x010A, // (-478/10)dB = 0.004074 * amplitude
    0x010E, // (-477/10)dB = 0.004121 * amplitude
    0x0111, // (-476/10)dB = 0.004169 * amplitude
    0x0114, // (-475/10)dB = 0.004217 * amplitude
    0x0117, // (-474/10)dB = 0.004266 * amplitude
    0x011A, // (-473/10)dB = 0.004315 * amplitude
    0x011E, // (-472/10)dB = 0.004365 * amplitude
    0x0121, // (-471/10)dB = 0.004416 * amplitude
    0x0124, // (-470/10)dB = 0.004467 * amplitude
    0x0128, // (-469/10)dB = 0.004519 * amplitude
    0x012B, // (-468/10)dB = 0.004571 * amplitude
    0x012F, // (-467/10)dB = 0.004624 * amplitude
    0x0132, // (-466/10)dB = 0.004677 * amplitude
    0x0136, // (-465/10)dB = 0.004732 * amplitude
    0x0139, // (-464/10)dB = 0.004786 * amplitude
    0x013D, // (-463/10)dB = 0.004842 * amplitude
    0x0140, // (-462/10)dB = 0.004898 * amplitude
    0x0144, // (-461/10)dB = 0.004955 * amplitude
    0x0148, // (-460/10)dB = 0.005012 * amplitude
    0x014C, // (-459/10)dB = 0.005070 * amplitude
    0x0150, // (-458/10)dB = 0.005129 * amplitude
    0x0154, // (-457/10)dB = 0.005188 * amplitude
    0x0157, // (-456/10)dB = 0.005248 * amplitude
    0x015B, // (-455/10)dB = 0.005309 * amplitude
    0x015F, // (-454/10)dB = 0.005370 * amplitude
    0x0164, // (-453/10)dB = 0.005433 * amplitude
    0x0168, // (-452/10)dB = 0.005495 * amplitude
    0x016C, // (-451/10)dB = 0.005559 * amplitude
    0x0170, // (-450/10)dB = 0.005623 * amplitude
    0x0174, // (-449/10)dB = 0.005689 * amplitude
    0x0179, // (-448/10)dB = 0.005754 * amplitude
    0x017D, // (-447/10)dB = 0.005821 * amplitude
    0x0181, // (-446/10)dB = 0.005888 * amplitude
    0x0186, // (-445/10)dB = 0.005957 * amplitude
    0x018A, // (-444/10)dB = 0.006026 * amplitude
    0x018F, // (-443/10)dB = 0.006095 * amplitude
    0x0194, // (-442/10)dB = 0.006166 * amplitude
    0x0198, // (-441/10)dB = 0.006237 * amplitude
    0x019D, // (-440/10)dB = 0.006310 * amplitude
    0x01A2, // (-439/10)dB = 0.006383 * amplitude
    0x01A7, // (-438/10)dB = 0.006457 * amplitude
    0x01AC, // (-437/10)dB = 0.006531 * amplitude
    0x01B0, // (-436/10)dB = 0.006607 * amplitude
    0x01B6, // (-435/10)dB = 0.006683 * amplitude
    0x01BB, // (-434/10)dB = 0.006761 * amplitude
    0x01C0, // (-433/10)dB = 0.006839 * amplitude
    0x01C5, // (-432/10)dB = 0.006918 * amplitude
    0x01CA, // (-431/10)dB = 0.006998 * amplitude
    0x01CF, // (-430/10)dB = 0.007079 * amplitude
    0x01D5, // (-429/10)dB = 0.007161 * amplitude
    0x01DA, // (-428/10)dB = 0.007244 * amplitude
    0x01E0, // (-427/10)dB = 0.007328 * amplitude
    0x01E5, // (-426/10)dB = 0.007413 * amplitude
    0x01EB, // (-425/10)dB = 0.007499 * amplitude
    0x01F1, // (-424/10)dB = 0.007586 * amplitude
    0x01F6, // (-423/10)dB = 0.007674 * amplitude
    0x01FC, // (-422/10)dB = 0.007762 * amplitude
    0x0202, // (-421/10)dB = 0.007852 * amplitude
    0x0208, // (-420/10)dB = 0.007943 * amplitude
    0x020E, // (-419/10)dB = 0.008035 * amplitude
    0x0214, // (-418/10)dB = 0.008128 * amplitude
    0x021A, // (-417/10)dB = 0.008222 * amplitude
    0x0221, // (-416/10)dB = 0.008318 * amplitude
    0x0227, // (-415/10)dB = 0.008414 * amplitude
    0x022D, // (-414/10)dB = 0.008511 * amplitude
    0x0234, // (-413/10)dB = 0.008610 * amplitude
    0x023A, // (-412/10)dB = 0.008710 * amplitude
    0x0241, // (-411/10)dB = 0.008810 * amplitude
    0x0248, // (-410/10)dB = 0.008913 * amplitude
    0x024E, // (-409/10)dB = 0.009016 * amplitude
    0x0255, // (-408/10)dB = 0.009120 * amplitude
    0x025C, // (-407/10)dB = 0.009226 * amplitude
    0x0263, // (-406/10)dB = 0.009333 * amplitude
    0x026A, // (-405/10)dB = 0.009441 * amplitude
    0x0271, // (-404/10)dB = 0.009550 * amplitude
    0x0279, // (-403/10)dB = 0.009661 * amplitude
    0x0280, // (-402/10)dB = 0.009772 * amplitude
    0x0287, // (-401/10)dB = 0.009886 * amplitude
    0x028F, // (-400/10)dB = 0.010000 * amplitude
    0x0296, // (-399/10)dB = 0.010116 * amplitude
    0x029E, // (-398/10)dB = 0.010233 * amplitude
    0x02A6, // (-397/10)dB = 0.010351 * amplitude
    0x02AE, // (-396/10)dB = 0.010471 * amplitude
    0x02B6, // (-395/10)dB = 0.010593 * amplitude
    0x02BE, // (-394/10)dB = 0.010715 * amplitude
    0x02C6, // (-393/10)dB = 0.010839 * amplitude
    0x02CE, // (-392/10)dB = 0.010965 * amplitude
    0x02D6, // (-391/10)dB = 0.011092 * amplitude
    0x02DF, // (-390/10)dB = 0.011220 * amplitude
    0x02E7, // (-389/10)dB = 0.011350 * amplitude
    0x02F0, // (-388/10)dB = 0.011482 * amplitude
    0x02F9, // (-387/10)dB = 0.011614 * amplitude
    0x0301, // (-386/10)dB = 0.011749 * amplitude
    0x030A, // (-385/10)dB = 0.011885 * amplitude
    0x0313, // (-384/10)dB = 0.012023 * amplitude
    0x031D, // (-383/10)dB = 0.012162 * amplitude
    0x0326, // (-382/10)dB = 0.012303 * amplitude
    0x032F, // (-381/10)dB = 0.012445 * amplitude
    0x0339, // (-380/10)dB = 0.012589 * amplitude
    0x0342, // (-379/10)dB = 0.012735 * amplitude
    0x034C, // (-378/10)dB = 0.012882 * amplitude
    0x0356, // (-377/10)dB = 0.013032 * amplitude
    0x035F, // (-376/10)dB = 0.013183 * amplitude
    0x0369, // (-375/10)dB = 0.013335 * amplitude
    0x0374, // (-374/10)dB = 0.013490 * amplitude
    0x037E, // (-373/10)dB = 0.013646 * amplitude
    0x0388, // (-372/10)dB = 0.013804 * amplitude
    0x0393, // (-371/10)dB = 0.013964 * amplitude
    0x039D, // (-370/10)dB = 0.014125 * amplitude
    0x03A8, // (-369/10)dB = 0.014289 * amplitude
    0x03B3, // (-368/10)dB = 0.014454 * amplitude
    0x03BE, // (-367/10)dB = 0.014622 * amplitude
    0x03C9, // (-366/10)dB = 0.014791 * amplitude
    0x03D4, // (-365/10)dB = 0.014962 * amplitude
    0x03DF, // (-364/10)dB = 0.015136 * amplitude
    0x03EB, // (-363/10)dB = 0.015311 * amplitude
    0x03F7, // (-362/10)dB = 0.015488 * amplitude
    0x0402, // (-361/10)dB = 0.015668 * amplitude
    0x040E, // (-360/10)dB = 0.015849 * amplitude
    0x041A, // (-359/10)dB = 0.016032 * amplitude
    0x0426, // (-358/10)dB = 0.016218 * amplitude
    0x0433, // (-357/10)dB = 0.016406 * amplitude
    0x043F, // (-356/10)dB = 0.016596 * amplitude
    0x044C, // (-355/10)dB = 0.016788 * amplitude
    0x0458, // (-354/10)dB = 0.016982 * amplitude
    0x0465, // (-353/10)dB = 0.017179 * amplitude
    0x0472, // (-352/10)dB = 0.017378 * amplitude
    0x0480, // (-351/10)dB = 0.017579 * amplitude
    0x048D, // (-350/10)dB = 0.017783 * amplitude
    0x049A, // (-349/10)dB = 0.017989 * amplitude
    0x04A8, // (-348/10)dB = 0.018197 * amplitude
    0x04B6, // (-347/10)dB = 0.018408 * amplitude
    0x04C4, // (-346/10)dB = 0.018621 * amplitude
    0x04D2, // (-345/10)dB = 0.018836 * amplitude
    0x04E0, // (-344/10)dB = 0.019055 * amplitude
    0x04EF, // (-343/10)dB = 0.019275 * amplitude
    0x04FD, // (-342/10)dB = 0.019498 * amplitude
    0x050C, // (-341/10)dB = 0.019724 * amplitude
    0x051B, // (-340/10)dB = 0.019953 * amplitude
    0x052A, // (-339/10)dB = 0.020184 * amplitude
    0x053A, // (-338/10)dB = 0.020417 * amplitude
    0x0549, // (-337/10)dB = 0.020654 * amplitude
    0x0559, // (-336/10)dB = 0.020893 * amplitude
    0x0569, // (-335/10)dB = 0.021135 * amplitude
    0x0579, // (-334/10)dB = 0.021380 * amplitude
    0x0589, // (-333/10)dB = 0.021627 * amplitude
    0x0599, // (-332/10)dB = 0.021878 * amplitude
    0x05AA, // (-331/10)dB = 0.022131 * amplitude
    0x05BB, // (-330/10)dB = 0.022387 * amplitude
    0x05CC, // (-329/10)dB = 0.022646 * amplitude
    0x05DD, // (-328/10)dB = 0.022909 * amplitude
    0x05EE, // (-327/10)dB = 0.023174 * amplitude
    0x0600, // (-326/10)dB = 0.023442 * amplitude
    0x0612, // (-325/10)dB = 0.023714 * amplitude
    0x0624, // (-324/10)dB = 0.023988 * amplitude
    0x0636, // (-323/10)dB = 0.024266 * amplitude
    0x0648, // (-322/10)dB = 0.024547 * amplitude
    0x065B, // (-321/10)dB = 0.024831 * amplitude
    0x066E, // (-320/10)dB = 0.025119 * amplitude
    0x0681, // (-319/10)dB = 0.025410 * amplitude
    0x0694, // (-318/10)dB = 0.025704 * amplitude
    0x06A8, // (-317/10)dB = 0.026002 * amplitude
    0x06BB, // (-316/10)dB = 0.026303 * amplitude
    0x06CF, // (-315/10)dB = 0.026607 * amplitude
    0x06E3, // (-314/10)dB = 0.026915 * amplitude
    0x06F8, // (-313/10)dB = 0.027227 * amplitude
    0x070D, // (-312/10)dB = 0.027542 * amplitude
    0x0721, // (-311/10)dB = 0.027861 * amplitude
    0x0737, // (-310/10)dB = 0.028184 * amplitude
    0x074C, // (-309/10)dB = 0.028510 * amplitude
    0x0762, // (-308/10)dB = 0.028840 * amplitude
    0x0777, // (-307/10)dB = 0.029174 * amplitude
    0x078E, // (-306/10)dB = 0.029512 * amplitude
    0x07A4, // (-305/10)dB = 0.029854 * amplitude
    0x07BB, // (-304/10)dB = 0.030200 * amplitude
    0x07D2, // (-303/10)dB = 0.030549 * amplitude
    0x07E9, // (-302/10)dB = 0.030903 * amplitude
    0x0800, // (-301/10)dB = 0.031261 * amplitude
    0x0818, // (-300/10)dB = 0.031623 * amplitude
    0x0830, // (-299/10)dB = 0.031989 * amplitude
    0x0848, // (-298/10)dB = 0.032359 * amplitude
    0x0861, // (-297/10)dB = 0.032734 * amplitude
    0x087A, // (-296/10)dB = 0.033113 * amplitude
    0x0893, // (-295/10)dB = 0.033497 * amplitude
    0x08AC, // (-294/10)dB = 0.033884 * amplitude
    0x08C6, // (-293/10)dB = 0.034277 * amplitude
    0x08E0, // (-292/10)dB = 0.034674 * amplitude
    0x08FA, // (-291/10)dB = 0.035075 * amplitude
    0x0915, // (-290/10)dB = 0.035481 * amplitude
    0x0930, // (-289/10)dB = 0.035892 * amplitude
    0x094B, // (-288/10)dB = 0.036308 * amplitude
    0x0967, // (-287/10)dB = 0.036728 * amplitude
    0x0982, // (-286/10)dB = 0.037154 * amplitude
    0x099F, // (-285/10)dB = 0.037584 * amplitude
    0x09BB, // (-284/10)dB = 0.038019 * amplitude
    0x09D8, // (-283/10)dB = 0.038459 * amplitude
    0x09F5, // (-282/10)dB = 0.038905 * amplitude
    0x0A13, // (-281/10)dB = 0.039355 * amplitude
    0x0A31, // (-280/10)dB = 0.039811 * amplitude
    0x0A4F, // (-279/10)dB = 0.040272 * amplitude
    0x0A6D, // (-278/10)dB = 0.040738 * amplitude
    0x0A8C, // (-277/10)dB = 0.041210 * amplitude
    0x0AAB, // (-276/10)dB = 0.041687 * amplitude
    0x0ACB, // (-275/10)dB = 0.042170 * amplitude
    0x0AEB, // (-274/10)dB = 0.042658 * amplitude
    0x0B0C, // (-273/10)dB = 0.043152 * amplitude
    0x0B2C, // (-272/10)dB = 0.043652 * amplitude
    0x0B4D, // (-271/10)dB = 0.044157 * amplitude
    0x0B6F, // (-270/10)dB = 0.044668 * amplitude
    0x0B91, // (-269/10)dB = 0.045186 * amplitude
    0x0BB3, // (-268/10)dB = 0.045709 * amplitude
    0x0BD6, // (-267/10)dB = 0.046238 * amplitude
    0x0BF9, // (-266/10)dB = 0.046774 * amplitude
    0x0C1C, // (-265/10)dB = 0.047315 * amplitude
    0x0C40, // (-264/10)dB = 0.047863 * amplitude
    0x0C65, // (-263/10)dB = 0.048417 * amplitude
    0x0C89, // (-262/10)dB = 0.048978 * amplitude
    0x0CAE, // (-261/10)dB = 0.049545 * amplitude
    0x0CD4, // (-260/10)dB = 0.050119 * amplitude
    0x0CFA, // (-259/10)dB = 0.050699 * amplitude
    0x0D21, // (-258/10)dB = 0.051286 * amplitude
    0x0D48, // (-257/10)dB = 0.051880 * amplitude
    0x0D6F, // (-256/10)dB = 0.052481 * amplitude
    0x0D97, // (-255/10)dB = 0.053088 * amplitude
    0x0DBF, // (-254/10)dB = 0.053703 * amplitude
    0x0DE8, // (-253/10)dB = 0.054325 * amplitude
    0x0E11, // (-252/10)dB = 0.054954 * amplitude
    0x0E3B, // (-251/10)dB = 0.055590 * amplitude
    0x0E65, // (-250/10)dB = 0.056234 * amplitude
    0x0E90, // (-249/10)dB = 0.056885 * amplitude
    0x0EBB, // (-248/10)dB = 0.057544 * amplitude
    0x0EE6, // (-247/10)dB = 0.058210 * amplitude
    0x0F13, // (-246/10)dB = 0.058884 * amplitude
    0x0F3F, // (-245/10)dB = 0.059566 * amplitude
    0x0F6C, // (-244/10)dB = 0.060256 * amplitude
    0x0F9A, // (-243/10)dB = 0.060954 * amplitude
    0x0FC8, // (-242/10)dB = 0.061660 * amplitude
    0x0FF7, // (-241/10)dB = 0.062373 * amplitude
    0x1027, // (-240/10)dB = 0.063096 * amplitude
    0x1056, // (-239/10)dB = 0.063826 * amplitude
    0x1087, // (-238/10)dB = 0.064565 * amplitude
    0x10B8, // (-237/10)dB = 0.065313 * amplitude
    0x10E9, // (-236/10)dB = 0.066069 * amplitude
    0x111C, // (-235/10)dB = 0.066834 * amplitude
    0x114E, // (-234/10)dB = 0.067608 * amplitude
    0x1182, // (-233/10)dB = 0.068391 * amplitude
    0x11B5, // (-232/10)dB = 0.069183 * amplitude
    0x11EA, // (-231/10)dB = 0.069984 * amplitude
    0x121F, // (-230/10)dB = 0.070795 * amplitude
    0x1255, // (-229/10)dB = 0.071614 * amplitude
    0x128B, // (-228/10)dB = 0.072444 * amplitude
    0x12C2, // (-227/10)dB = 0.073282 * amplitude
    0x12FA, // (-226/10)dB = 0.074131 * amplitude
    0x1332, // (-225/10)dB = 0.074989 * amplitude
    0x136B, // (-224/10)dB = 0.075858 * amplitude
    0x13A4, // (-223/10)dB = 0.076736 * amplitude
    0x13DF, // (-222/10)dB = 0.077625 * amplitude
    0x141A, // (-221/10)dB = 0.078524 * amplitude
    0x1455, // (-220/10)dB = 0.079433 * amplitude
    0x1491, // (-219/10)dB = 0.080353 * amplitude
    0x14CE, // (-218/10)dB = 0.081283 * amplitude
    0x150C, // (-217/10)dB = 0.082224 * amplitude
    0x154B, // (-216/10)dB = 0.083176 * amplitude
    0x158A, // (-215/10)dB = 0.084140 * amplitude
    0x15CA, // (-214/10)dB = 0.085114 * amplitude
    0x160A, // (-213/10)dB = 0.086099 * amplitude
    0x164B, // (-212/10)dB = 0.087096 * amplitude
    0x168E, // (-211/10)dB = 0.088105 * amplitude
    0x16D0, // (-210/10)dB = 0.089125 * amplitude
    0x1714, // (-209/10)dB = 0.090157 * amplitude
    0x1758, // (-208/10)dB = 0.091201 * amplitude
    0x179E, // (-207/10)dB = 0.092257 * amplitude
    0x17E4, // (-206/10)dB = 0.093325 * amplitude
    0x182A, // (-205/10)dB = 0.094406 * amplitude
    0x1872, // (-204/10)dB = 0.095499 * amplitude
    0x18BB, // (-203/10)dB = 0.096605 * amplitude
    0x1904, // (-202/10)dB = 0.097724 * amplitude
    0x194E, // (-201/10)dB = 0.098855 * amplitude
    0x1999, // (-200/10)dB = 0.100000 * amplitude
    0x19E5, // (-199/10)dB = 0.101158 * amplitude
    0x1A32, // (-198/10)dB = 0.102329 * amplitude
    0x1A7F, // (-197/10)dB = 0.103514 * amplitude
    0x1ACE, // (-196/10)dB = 0.104713 * amplitude
    0x1B1D, // (-195/10)dB = 0.105925 * amplitude
    0x1B6E, // (-194/10)dB = 0.107152 * amplitude
    0x1BBF, // (-193/10)dB = 0.108393 * amplitude
    0x1C11, // (-192/10)dB = 0.109648 * amplitude
    0x1C65, // (-191/10)dB = 0.110917 * amplitude
    0x1CB9, // (-190/10)dB = 0.112202 * amplitude
    0x1D0E, // (-189/10)dB = 0.113501 * amplitude
    0x1D64, // (-188/10)dB = 0.114815 * amplitude
    0x1DBB, // (-187/10)dB = 0.116145 * amplitude
    0x1E13, // (-186/10)dB = 0.117490 * amplitude
    0x1E6C, // (-185/10)dB = 0.118850 * amplitude
    0x1EC7, // (-184/10)dB = 0.120226 * amplitude
    0x1F22, // (-183/10)dB = 0.121619 * amplitude
    0x1F7E, // (-182/10)dB = 0.123027 * amplitude
    0x1FDC, // (-181/10)dB = 0.124451 * amplitude
    0x203A, // (-180/10)dB = 0.125893 * amplitude
    0x209A, // (-179/10)dB = 0.127350 * amplitude
    0x20FA, // (-178/10)dB = 0.128825 * amplitude
    0x215C, // (-177/10)dB = 0.130317 * amplitude
    0x21BF, // (-176/10)dB = 0.131826 * amplitude
    0x2223, // (-175/10)dB = 0.133352 * amplitude
    0x2288, // (-174/10)dB = 0.134896 * amplitude
    0x22EE, // (-173/10)dB = 0.136458 * amplitude
    0x2356, // (-172/10)dB = 0.138038 * amplitude
    0x23BF, // (-171/10)dB = 0.139637 * amplitude
    0x2429, // (-170/10)dB = 0.141254 * amplitude
    0x2494, // (-169/10)dB = 0.142889 * amplitude
    0x2500, // (-168/10)dB = 0.144544 * amplitude
    0x256E, // (-167/10)dB = 0.146218 * amplitude
    0x25DD, // (-166/10)dB = 0.147911 * amplitude
    0x264D, // (-165/10)dB = 0.149624 * amplitude
    0x26BF, // (-164/10)dB = 0.151356 * amplitude
    0x2732, // (-163/10)dB = 0.153109 * amplitude
    0x27A6, // (-162/10)dB = 0.154882 * amplitude
    0x281B, // (-161/10)dB = 0.156675 * amplitude
    0x2892, // (-160/10)dB = 0.158489 * amplitude
    0x290B, // (-159/10)dB = 0.160325 * amplitude
    0x2984, // (-158/10)dB = 0.162181 * amplitude
    0x29FF, // (-157/10)dB = 0.164059 * amplitude
    0x2A7C, // (-156/10)dB = 0.165959 * amplitude
    0x2AFA, // (-155/10)dB = 0.167880 * amplitude
    0x2B79, // (-154/10)dB = 0.169824 * amplitude
    0x2BFA, // (-153/10)dB = 0.171791 * amplitude
    0x2C7C, // (-152/10)dB = 0.173780 * amplitude
    0x2D00, // (-151/10)dB = 0.175792 * amplitude
    0x2D86, // (-150/10)dB = 0.177828 * amplitude
    0x2E0D, // (-149/10)dB = 0.179887 * amplitude
    0x2E95, // (-148/10)dB = 0.181970 * amplitude
    0x2F1F, // (-147/10)dB = 0.184077 * amplitude
    0x2FAB, // (-146/10)dB = 0.186209 * amplitude
    0x3038, // (-145/10)dB = 0.188365 * amplitude
    0x30C7, // (-144/10)dB = 0.190546 * amplitude
    0x3158, // (-143/10)dB = 0.192752 * amplitude
    0x31EA, // (-142/10)dB = 0.194984 * amplitude
    0x327E, // (-141/10)dB = 0.197242 * amplitude
    0x3314, // (-140/10)dB = 0.199526 * amplitude
    0x33AB, // (-139/10)dB = 0.201837 * amplitude
    0x3444, // (-138/10)dB = 0.204174 * amplitude
    0x34DF, // (-137/10)dB = 0.206538 * amplitude
    0x357C, // (-136/10)dB = 0.208930 * amplitude
    0x361A, // (-135/10)dB = 0.211349 * amplitude
    0x36BB, // (-134/10)dB = 0.213796 * amplitude
    0x375D, // (-133/10)dB = 0.216272 * amplitude
    0x3801, // (-132/10)dB = 0.218776 * amplitude
    0x38A7, // (-131/10)dB = 0.221309 * amplitude
    0x394F, // (-130/10)dB = 0.223872 * amplitude
    0x39F9, // (-129/10)dB = 0.226464 * amplitude
    0x3AA5, // (-128/10)dB = 0.229087 * amplitude
    0x3B53, // (-127/10)dB = 0.231739 * amplitude
    0x3C03, // (-126/10)dB = 0.234423 * amplitude
    0x3CB5, // (-125/10)dB = 0.237137 * amplitude
    0x3D68, // (-124/10)dB = 0.239883 * amplitude
    0x3E1F, // (-123/10)dB = 0.242661 * amplitude
    0x3ED7, // (-122/10)dB = 0.245471 * amplitude
    0x3F91, // (-121/10)dB = 0.248313 * amplitude
    0x404D, // (-120/10)dB = 0.251189 * amplitude
    0x410C, // (-119/10)dB = 0.254097 * amplitude
    0x41CD, // (-118/10)dB = 0.257040 * amplitude
    0x4290, // (-117/10)dB = 0.260016 * amplitude
    0x4355, // (-116/10)dB = 0.263027 * amplitude
    0x441D, // (-115/10)dB = 0.266073 * amplitude
    0x44E7, // (-114/10)dB = 0.269153 * amplitude
    0x45B3, // (-113/10)dB = 0.272270 * amplitude
    0x4682, // (-112/10)dB = 0.275423 * amplitude
    0x4753, // (-111/10)dB = 0.278612 * amplitude
    0x4826, // (-110/10)dB = 0.281838 * amplitude
    0x48FC, // (-109/10)dB = 0.285102 * amplitude
    0x49D4, // (-108/10)dB = 0.288403 * amplitude
    0x4AAF, // (-107/10)dB = 0.291743 * amplitude
    0x4B8D, // (-106/10)dB = 0.295121 * amplitude
    0x4C6D, // (-105/10)dB = 0.298538 * amplitude
    0x4D4F, // (-104/10)dB = 0.301995 * amplitude
    0x4E34, // (-103/10)dB = 0.305492 * amplitude
    0x4F1C, // (-102/10)dB = 0.309030 * amplitude
    0x5007, // (-101/10)dB = 0.312608 * amplitude
    0x50F4, // (-100/10)dB = 0.316228 * amplitude
    0x51E4, // (-99/10)dB = 0.319890 * amplitude
    0x52D7, // (-98/10)dB = 0.323594 * amplitude
    0x53CC, // (-97/10)dB = 0.327341 * amplitude
    0x54C5, // (-96/10)dB = 0.331131 * amplitude
    0x55C0, // (-95/10)dB = 0.334965 * amplitude
    0x56BE, // (-94/10)dB = 0.338844 * amplitude
    0x57BF, // (-93/10)dB = 0.342768 * amplitude
    0x58C3, // (-92/10)dB = 0.346737 * amplitude
    0x59CA, // (-91/10)dB = 0.350752 * amplitude
    0x5AD5, // (-90/10)dB = 0.354813 * amplitude
    0x5BE2, // (-89/10)dB = 0.358922 * amplitude
    0x5CF2, // (-88/10)dB = 0.363078 * amplitude
    0x5E06, // (-87/10)dB = 0.367282 * amplitude
    0x5F1C, // (-86/10)dB = 0.371535 * amplitude
    0x6036, // (-85/10)dB = 0.375837 * amplitude
    0x6154, // (-84/10)dB = 0.380189 * amplitude
    0x6274, // (-83/10)dB = 0.384592 * amplitude
    0x6398, // (-82/10)dB = 0.389045 * amplitude
    0x64BF, // (-81/10)dB = 0.393550 * amplitude
    0x65EA, // (-80/10)dB = 0.398107 * amplitude
    0x6718, // (-79/10)dB = 0.402717 * amplitude
    0x684A, // (-78/10)dB = 0.407380 * amplitude
    0x697F, // (-77/10)dB = 0.412098 * amplitude
    0x6AB7, // (-76/10)dB = 0.416869 * amplitude
    0x6BF4, // (-75/10)dB = 0.421697 * amplitude
    0x6D34, // (-74/10)dB = 0.426580 * amplitude
    0x6E78, // (-73/10)dB = 0.431519 * amplitude
    0x6FBF, // (-72/10)dB = 0.436516 * amplitude
    0x710A, // (-71/10)dB = 0.441570 * amplitude
    0x7259, // (-70/10)dB = 0.446684 * amplitude
    0x73AC, // (-69/10)dB = 0.451856 * amplitude
    0x7503, // (-68/10)dB = 0.457088 * amplitude
    0x765E, // (-67/10)dB = 0.462381 * amplitude
    0x77BD, // (-66/10)dB = 0.467735 * amplitude
    0x7920, // (-65/10)dB = 0.473151 * amplitude
    0x7A87, // (-64/10)dB = 0.478630 * amplitude
    0x7BF2, // (-63/10)dB = 0.484172 * amplitude
    0x7D62, // (-62/10)dB = 0.489779 * amplitude
    0x7ED5, // (-61/10)dB = 0.495450 * amplitude
    0x804D, // (-60/10)dB = 0.501187 * amplitude
    0x81CA, // (-59/10)dB = 0.506991 * amplitude
    0x834A, // (-58/10)dB = 0.512861 * amplitude
    0x84D0, // (-57/10)dB = 0.518800 * amplitude
    0x8659, // (-56/10)dB = 0.524807 * amplitude
    0x87E8, // (-55/10)dB = 0.530884 * amplitude
    0x897A, // (-54/10)dB = 0.537032 * amplitude
    0x8B12, // (-53/10)dB = 0.543250 * amplitude
    0x8CAE, // (-52/10)dB = 0.549541 * amplitude
    0x8E4F, // (-51/10)dB = 0.555904 * amplitude
    0x8FF5, // (-50/10)dB = 0.562341 * amplitude
    0x91A0, // (-49/10)dB = 0.568853 * amplitude
    0x9350, // (-48/10)dB = 0.575440 * amplitude
    0x9504, // (-47/10)dB = 0.582103 * amplitude
    0x96BE, // (-46/10)dB = 0.588844 * amplitude
    0x987D, // (-45/10)dB = 0.595662 * amplitude
    0x9A41, // (-44/10)dB = 0.602560 * amplitude
    0x9C0A, // (-43/10)dB = 0.609537 * amplitude
    0x9DD9, // (-42/10)dB = 0.616595 * amplitude
    0x9FAD, // (-41/10)dB = 0.623735 * amplitude
    0xA186, // (-40/10)dB = 0.630957 * amplitude
    0xA365, // (-39/10)dB = 0.638263 * amplitude
    0xA549, // (-38/10)dB = 0.645654 * amplitude
    0xA733, // (-37/10)dB = 0.653131 * amplitude
    0xA923, // (-36/10)dB = 0.660693 * amplitude
    0xAB18, // (-35/10)dB = 0.668344 * amplitude
    0xAD13, // (-34/10)dB = 0.676083 * amplitude
    0xAF14, // (-33/10)dB = 0.683912 * amplitude
    0xB11B, // (-32/10)dB = 0.691831 * amplitude
    0xB328, // (-31/10)dB = 0.699842 * amplitude
    0xB53B, // (-30/10)dB = 0.707946 * amplitude
    0xB755, // (-29/10)dB = 0.716143 * amplitude
    0xB974, // (-28/10)dB = 0.724436 * amplitude
    0xBB9A, // (-27/10)dB = 0.732825 * amplitude
    0xBDC6, // (-26/10)dB = 0.741310 * amplitude
    0xBFF9, // (-25/10)dB = 0.749894 * amplitude
    0xC232, // (-24/10)dB = 0.758578 * amplitude
    0xC471, // (-23/10)dB = 0.767361 * amplitude
    0xC6B8, // (-22/10)dB = 0.776247 * amplitude
    0xC905, // (-21/10)dB = 0.785236 * amplitude
    0xCB59, // (-20/10)dB = 0.794328 * amplitude
    0xCDB3, // (-19/10)dB = 0.803526 * amplitude
    0xD015, // (-18/10)dB = 0.812831 * amplitude
    0xD27E, // (-17/10)dB = 0.822243 * amplitude
    0xD4EE, // (-16/10)dB = 0.831764 * amplitude
    0xD765, // (-15/10)dB = 0.841395 * amplitude
    0xD9E4, // (-14/10)dB = 0.851138 * amplitude
    0xDC6A, // (-13/10)dB = 0.860994 * amplitude
    0xDEF7, // (-12/10)dB = 0.870964 * amplitude
    0xE18C, // (-11/10)dB = 0.881049 * amplitude
    0xE429, // (-10/10)dB = 0.891251 * amplitude
    0xE6CD, // (-9/10)dB = 0.901571 * amplitude
    0xE979, // (-8/10)dB = 0.912011 * amplitude
    0xEC2D, // (-7/10)dB = 0.922571 * amplitude
    0xEEE9, // (-6/10)dB = 0.933254 * amplitude
    0xF1AD, // (-5/10)dB = 0.944061 * amplitude
    0xF47A, // (-4/10)dB = 0.954993 * amplitude
    0xF74F, // (-3/10)dB = 0.966051 * amplitude
    0xFA2C, // (-2/10)dB = 0.977237 * amplitude
    0xFD11, // (-1/10)dB = 0.988553 * amplitude
    0xFFFF // 0dB
    };

#endif // USE_DB_CONV_LOOKUP

// The two routines that follow convert from a linear AmpFactor in the
// range 65535/65536 to 1/65536 to a decibel range of -100.00 to -0.01
// and vice versa.
//
// The amp factors are expressed in parts per 65536 as a 16 bit numerator,
// i.e. actual amplification is (ampfactor)/65536.
//
// The decibels are expressed as parts per 100.  i.e. -100 means -1dB.
//
// Note that although a decibel is formally 10 * log10(x), the AmpFactor
// refers to voltages and the dB to powers, and power = v**2/r
// so the rule is dB = 20*log10(x)
//
// If you change one routine you will almost certainly have to change
// the other.  In an ideal world the routines would be inverses
// meaning that the following is true
//
// 	AmpFactor == DBToAmpFactor(AmpFactorToDB(AmpFactor))
//
// HOWEVER many iterations are made through the routines. The
// secondary objective is to minimise the drift such that
// successive iterations do not stray too far from the starting
// point.
//
// This is clearly impossible because the quantisations are
// quite different.  10**-.005 = 1.0115 or 65536->64786, so if
// we start from (say) 65000 then something nasty must happen.
// likewise the difference between an amp factor of 1/65536 and
// 2/65536 is about 6dB - so much for the nearest hundredth!
//
// From here on we'll call them DB (even though that should be a
// dekabell (10 bells) to avoid confusion with hungarian.
//

//
//  DBToAmpFactor
//
//  Converts lDB, specified in 100ths decibels, into a
//  linear amplification factor.  For all lDB >= 0 this
//  function returns 0xffff.  This is because our current
//  implementation of DirectSound doesn't allow amplification,
//  and users of this function often require 16-bit results
//
DWORD DBToAmpFactor( LONG lDB )
{
#ifndef USE_DB_CONV_LOOKUP
    double dAF;

    // make mixer code work- it only handles 16-bit factors and cannot amplify
    if (0 <= lDB) return 0x0000FFFF;

    // input lDB is 100ths of decibels

    dAF = pow(10.0, (0.5+((double)lDB))/2000.0);

    // This gives me a number in the range 0-1
    // normalise to 0-65535

    return (DWORD)(dAF*65535);

#else // USE_DB_CONV_LOOKUP

    DWORD dwFactor;

    //
    // bias and scale the input and check boundaries for indexing into table
    //
    lDB = (lDB-5)/10;		    // scaled to 1/10th dB units

    if (lDB > 0)    lDB = 0;        // upper boundary
    if (lDB < -964) return 0;  	    // lower boundary

    lDB = (lDB + 964);		    // bias

    // lDB better be a good index
//    ASSERT(lDB >= 0);
//    ASSERT(lDB <= sizeof(tblDBTenthsToAmpFactor) / sizeof(tblDBTenthsToAmpFactor[0]));

    dwFactor = (DWORD) tblDBTenthsToAmpFactor[lDB];

    return dwFactor;

#endif // USE_DB_CONV_LOOKUP
}

LONG AmpFactorToDB( DWORD dwFactor )
{
#ifndef USE_DB_CONV_LOOKUP
    if (1>=dwFactor) {
	return -10000;
    } else if (0xFFFF <= dwFactor) {
	return 0;	// This puts an upper bound - no amplification
    } else {
	return (LONG)(2000.0 * log10((-0.5+(double)dwFactor)/65536.0));
    }

#else // USE_DB_CONV_LOOKUP

    LONG    lDB;

    int	    iMin;
    int	    iMax;
    int     iMiddle;

    iMin = 0;

    iMax = sizeof(tblDBTenthsToAmpFactor) / sizeof(tblDBTenthsToAmpFactor[0]) - 1;

#ifdef BINARY_CHOP
    int     i;
    for (i = 0; i <= iMax - 1; i++) {
	if (dwFactor <= (DWORD) tblDBTenthsToAmpFactor[i]) break;
    }

    lDB = (i - 964) * 10;
#else
    //
    // Binary search through the table of AmpFactors.
    //
    // iMin and iMax specify the range in which the result can be found this
    // is initially the whole table.
    //
    // In each interation we reduce the range by half.
    //
    // Loop variant: iMax - iMin, which decreases in each iteration.
    // Invariant:  iMin <= iMax.
    // Exit condition: iMin = iMax.
    //
    while (iMin != iMax) {
//        ASSERT(iMin < iMax);

        iMiddle = (iMin + iMax) / 2;

        // Chose the half which includes dwFactor.
        if (dwFactor <= tblDBTenthsToAmpFactor[iMiddle])
            iMax = iMiddle;       // iMin does not change
        else
            iMin = iMiddle + 1;   // iMax does not change
    }

    lDB = (iMin - 964) * 10;
#endif

    return lDB;
#endif // USE_DB_CONV_LOOKUP
}


