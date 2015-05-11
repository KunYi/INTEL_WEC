//
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2014 Intel Corporation All Rights Reserved. 
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

#ifndef _SD3_VOLTAGE_H_
#define _SD3_VOLTAGE_H_

#define SD_VDD_WINDOW_LOW_VOLTAGE       0x01000000      //Switching to 1.8V Accepted


#undef SD_VDD_WINDOW_1_6_TO_1_7
#define SD_VDD_WINDOW_1_6_TO_1_7        SD_VDD_WINDOW_LOW_VOLTAGE      // 1.6 V to 1.7 Volts

#undef SD_VDD_WINDOW_1_7_TO_1_8
#define SD_VDD_WINDOW_1_7_TO_1_8        SD_VDD_WINDOW_LOW_VOLTAGE      // 1.7 V to 1.8 Volts

#undef SD_VDD_WINDOW_1_8_TO_1_9
#define SD_VDD_WINDOW_1_8_TO_1_9        SD_VDD_WINDOW_LOW_VOLTAGE      // 1.8 V to 1.9 Volts

#undef SD_VDD_WINDOW_1_9_TO_2_0
#define SD_VDD_WINDOW_1_9_TO_2_0        SD_VDD_WINDOW_LOW_VOLTAGE      // 1.9 V to 2.0 Volts

#undef SD_VDD_WINDOW_2_0_TO_2_1
#define SD_VDD_WINDOW_2_0_TO_2_1        SD_VDD_WINDOW_LOW_VOLTAGE      // 2.0 V to 2.1 Volts

#undef SD_VDD_WINDOW_2_1_TO_2_2
#define SD_VDD_WINDOW_2_1_TO_2_2        SD_VDD_WINDOW_LOW_VOLTAGE      // 2.1 V to 2.2 Volts

#undef SD_VDD_WINDOW_2_2_TO_2_3
#define SD_VDD_WINDOW_2_2_TO_2_3        SD_VDD_WINDOW_LOW_VOLTAGE      // 2.2 V to 2.3 Volts

#undef SD_VDD_WINDOW_2_3_TO_2_4
#define SD_VDD_WINDOW_2_3_TO_2_4        SD_VDD_WINDOW_LOW_VOLTAGE      // 2.3 V to 2.4 Volts

#undef SD_VDD_WINDOW_2_4_TO_2_5
#define SD_VDD_WINDOW_2_4_TO_2_5        SD_VDD_WINDOW_LOW_VOLTAGE      // 2.4 V to 2.5 Volts

#undef SD_VDD_WINDOW_2_5_TO_2_6
#define SD_VDD_WINDOW_2_5_TO_2_6        SD_VDD_WINDOW_LOW_VOLTAGE      // 2.5 V to 2.6 Volts

#undef SD_VDD_WINDOW_2_6_TO_2_7
#define SD_VDD_WINDOW_2_6_TO_2_7        SD_VDD_WINDOW_LOW_VOLTAGE      // 2.6 V to 2.7 Volts

#endif _SD3_VOLTAGE_H_

