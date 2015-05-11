//
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2002-2014 Intel Corporation All Rights Reserved. 
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
//
#include "gpioBaseAddr.h"
//////////////////////////////////////////////////////////////////////////
enum
{
	PCI_TYPE1_ADDRESS      = 0x0CF8,
	PCI_TYPE1_DATA         = 0x0CFC,
	PCI_TYPE2_FORWARD      = 0x0CFA,
	PCI_TYPE2_CSE          = 0x0CF8
};

typedef struct  _TYPE1_PCI_ADDRESS {
	union {
		struct {
			ULONG   Reserved:2;
			ULONG   Register:6;
			ULONG   Function:3;
			ULONG   Device:5;
			ULONG   Bus:8;
			ULONG   Reserved2:7;
			ULONG   ConfigurationAccess:1;
		} bits;
		ULONG   AsULONG;
	} u;
} TYPE1_PCI_ADDRESS, *PTYPE1_PCI_ADDRESS;

enum { HW_ACCESS_SIZE  = sizeof(DWORD) }; // 32-bit access only
static const DWORD HW_ODD_BITS     = (HW_ACCESS_SIZE - 1);
static const DWORD BITS_PER_BYTE   = 8;
static const BYTE  BYTE_MASK       = 0xff;
//////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
//
// Function:  PCI_Type1_Configuration
//
// Given the bus number, device number, and function number, read or write
// based on the "fWriting" flag "Length" bytes at "ByteOffset" in PCI space.
// Note that on this platform the PCI space exists in X86 IO space and can
// only be accessed as long-words. This routine is responsible for managing
// accesses that are not on long-word boundaries.
//
static ULONG PCI_Type1_Configuration(
									 ULONG     BusNumber,
									 ULONG     DeviceNumber,
									 ULONG     FunctionNumber,
									 __inout_bcount(Length) void* Buffer,
									 ULONG     ByteOffset,
									 ULONG     Length,        // Length in bytes to read or write
									 BOOL      fWriting       // False for read, true for write
									 )
{
	TYPE1_PCI_ADDRESS type1Address;
	ULONG             longOffset;   // Hardware can only access longs
	ULONG             value;
	int               bitPosition;
	union {
		ULONG *Long;    // Aligned accesses
		UCHAR *Char;    // Unaligned
	} ptr;

	ptr.Long = Buffer;
	longOffset = ByteOffset / HW_ACCESS_SIZE;

	// Construct a PCI address from the various fields.
	type1Address.u.AsULONG = 0UL;
	type1Address.u.bits.ConfigurationAccess = 1UL;
	type1Address.u.bits.Bus = BusNumber;
	type1Address.u.bits.Device = DeviceNumber;
	type1Address.u.bits.Function = FunctionNumber;
	type1Address.u.bits.Register = longOffset;

	// If the offset is not on a long-word boundary,
	// operate on bytes until it is.
	if (ByteOffset & HW_ODD_BITS) 
	{
		// Set the PCI address latch in x86 IO space.
		WRITE_PORT_ULONG((DWORD*) PCI_TYPE1_ADDRESS, type1Address.u.AsULONG);

		// Read the long-word from this aligned address.
		value = READ_PORT_ULONG ((DWORD*) PCI_TYPE1_DATA);

		if (!fWriting)
		{
			// Discard odd bytes read.
			value >>= (ByteOffset & HW_ODD_BITS) * BITS_PER_BYTE;
		}

		// Operate on the unaligned bytes.
		while ((ByteOffset & HW_ODD_BITS) && (Length > 0UL)) 
		{
			if (fWriting) 
			{
				bitPosition = (int)(ByteOffset & HW_ODD_BITS) * BITS_PER_BYTE;
				value &= ~(BYTE_MASK << bitPosition);
				value |= *ptr.Char++ << bitPosition;
			} 
			else 
			{
				*ptr.Char++ = (UCHAR)value;
				value >>= BITS_PER_BYTE;
			}
			++ByteOffset;
			--Length;
		}
		if (fWriting) 
		{
			// Now write back the full long-word that has the odd bytes.
			WRITE_PORT_ULONG ((DWORD*) PCI_TYPE1_DATA, value);
		}
		++longOffset;
	}

	// Long alignment has been established. Operate on longs now.
	while (Length >= sizeof *ptr.Long) 
	{
		// Set the PCI address latch in x86 IO space.
		type1Address.u.bits.Register = longOffset++;
		WRITE_PORT_ULONG((DWORD*) PCI_TYPE1_ADDRESS, type1Address.u.AsULONG);
		if (fWriting) 
		{
			WRITE_PORT_ULONG((ULONG*) PCI_TYPE1_DATA, *ptr.Long++);
		} 
		else 
		{
			*ptr.Long++ = READ_PORT_ULONG ((ULONG*) PCI_TYPE1_DATA);
		}
		Length -= sizeof *ptr.Long;
	}

	// Operate on remaining bytes if the length was not long aligned.
	if (Length > 0UL)
	{
		// Set the PCI address latch in x86 IO space.
		type1Address.u.bits.Register = longOffset;
		WRITE_PORT_ULONG((DWORD*) PCI_TYPE1_ADDRESS, type1Address.u.AsULONG);

		// Read the aligned long-word to operate on the odd bytes.
		value = READ_PORT_ULONG((DWORD*) PCI_TYPE1_DATA);

		for (ByteOffset = 0UL; Length > 0UL; ++ByteOffset) 
		{
			if (fWriting) 
			{
				bitPosition = (int)(ByteOffset & HW_ODD_BITS) * BITS_PER_BYTE;
				value &= ~(BYTE_MASK << bitPosition);
				value |= (ULONG)(*ptr.Char++ << bitPosition);
			} 
			else 
			{
				*ptr.Char++ = (UCHAR)(value & BYTE_MASK);
				value >>= BITS_PER_BYTE;
			}
			Length -= sizeof *ptr.Char;
		}
		if (fWriting) 
		{
			// Now write back the full long-word that has the remaining bytes.
			WRITE_PORT_ULONG((DWORD*) PCI_TYPE1_DATA, value);
		}
	}

	return (ULONG)(ptr.Char - (UCHAR *)Buffer);
}



ULONG PCIReadBusData(
					 ULONG BusNumber,
					 ULONG DeviceNumber,
					 ULONG FunctionNumber,
					 __out_bcount(Length) void* Buffer,
					 ULONG Offset,
					 ULONG Length
					 )
{
	//if (IsAfterPostInit() && g_pNKGlobal)
	//{
	//	g_pNKGlobal->pfnAcquireOalSpinLock ();
	//}


	Length = PCI_Type1_Configuration (BusNumber, DeviceNumber, FunctionNumber, Buffer, Offset, Length, FALSE);


	//if (IsAfterPostInit() && g_pNKGlobal)
	//	g_pNKGlobal->pfnReleaseOalSpinLock ();

	return Length;
}

