/*
 * libspe - A wrapper library to adapt the JSRE SPU usage model to SPUFS 
 * Copyright (C) 2005 IBM Corp. 
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by 
 * the Free Software Foundation; either version 2.1 of the License, 
 * or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but 
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public 
 *  License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License 
 *   along with this library; if not, write to the Free Software Foundation, 
 *   Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _cbea_map_h_
#define _cbea_map_h_

#ifdef __cplusplus
extern "C"
{
#endif

/* spe problem state areas
 */

typedef struct spe_mssync_area {
	unsigned int MFC_MSSync;    
} spe_mssync_area_t;  

typedef struct spe_mfc_command_area {
	unsigned char reserved_0_3[4];
	unsigned int MFC_LSA;
	unsigned int MFC_EAH;
	unsigned int MFC_EAL;
	unsigned int MFC_Size_Tag;
	union {     
		unsigned int MFC_ClassID_CMD;
		unsigned int MFC_CMDStatus;      
	};      
	unsigned char reserved_18_103[236];      
	unsigned int MFC_QStatus;      
	unsigned char reserved_108_203[252];     
	unsigned int Prxy_QueryType;      
	unsigned char reserved_208_21B[20];      
	unsigned int Prxy_QueryMask;     
	unsigned char reserved_220_22B[12];   
	unsigned int Prxy_TagStatus;  
} spe_mfc_command_area_t;  

typedef struct spe_spu_control_area {
	unsigned char reserved_0_3[4]; 
	unsigned int SPU_Out_Mbox; 
	unsigned char reserved_8_B[4];
	unsigned int SPU_In_Mbox; 
	unsigned char reserved_10_13[4]; 
	unsigned int SPU_Mbox_Stat; 
	unsigned char reserved_18_1B[4];
	unsigned int SPU_RunCntl;
	unsigned char reserved_20_23[4];  
	unsigned int SPU_Status; 
	unsigned char reserved_28_33[12];
	unsigned int SPU_NPC;    
} spe_spu_control_area_t;

typedef struct spe_sig_notify_1_area {     
	 unsigned char reserved_0_B[12]; 
	 unsigned int SPU_Sig_Notify_1;    
 } spe_sig_notify_1_area_t;   

typedef struct spe_sig_notify_2_area {  
	 unsigned char reserved_0_B[12]; 
	 unsigned int SPU_Sig_Notify_2;    
 } spe_sig_notify_2_area_t;  



#ifdef __cplusplus
}
#endif

#endif

