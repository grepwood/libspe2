--
-- Data structure declarations for SPE problem state areas
-- (part of public API)
--
with System;
with Interfaces.C;

package cbea_map is
  use System;
  use Interfaces.C;

type spe_mssync_area_t is
  record
    MFC_MSSync		: unsigned;
  end record;
type spe_mssync_area_ptr is access spe_mssync_area_t;

type spe_mfc_command_area_t is
  record
    reserved_0_3	: char_array (0 .. 3);
    MFC_LSA		: unsigned; 
    MFC_EAH		: unsigned; 
    MFC_EAL		: unsigned; 
    MFC_Size_Tag	: unsigned; 
    MFC_ClassID_CMD	: unsigned;
--  MFC_CMDStatus	: unsigned;
    reserved_18_103	: char_array (0 .. 235);
    MFC_QStatus		: unsigned;
    reserved_108_203	: char_array (0 .. 251);
    Prxy_QueryType	: unsigned;
    reserved_208_21B	: char_array (0 .. 19);
    Prxy_QueryMask	: unsigned;
    reserved_220_22B	: char_array (0 .. 11);
    Prxy_TagStatus	: unsigned;
  end record;
type spe_mfc_command_area_ptr is access spe_mfc_command_area_t;

type spe_spu_control_area_t is
  record
    reserved_0_3	: char_array (0 .. 3);
    SPU_Out_Mbox	: unsigned; 
    reserved_8_B	: char_array (0 .. 3);
    SPU_In_Mbox		: unsigned; 
    reserved_10_13	: char_array (0 .. 3);
    SPU_Mbox_Stat	: unsigned; 
    reserved_18_1B	: char_array (0 .. 3);
    SPU_RunCntl		: unsigned; 
    reserved_20_23	: char_array (0 .. 3);
    SPU_Status		: unsigned; 
    reserved_28_33	: char_array (0 .. 11);
    SPU_NPC		: unsigned; 
  end record;
type spe_spu_control_area_ptr is access spe_spu_control_area_t;

type spe_sig_notify_1_area_t is
  record
    reserved_0_B	: char_array (0 .. 11);
    SPU_Sig_Notify_1	: unsigned; 
  end record;
type spe_sig_notify_1_area_ptr is access spe_sig_notify_1_area_t;

type spe_sig_notify_2_area_t is
  record
    reserved_0_B	: char_array (0 .. 11);
    SPU_Sig_Notify_2	: unsigned; 
  end record;
type spe_sig_notify_2_area_ptr is access spe_sig_notify_2_area_t;

end cbea_map;

