--
-- Definition of data types and constants
--
with System;
with Interfaces.C;

package libspe2_types is
  use System;
  use Interfaces.C;

type addr_ptr is access Address;
type int_ptr is access Integer;
type unsigned_ptr is access unsigned;

-- SPE program handle
-- Structure spe_program_handle per CESOF specification
-- libspe2 applications usually only keep a pointer
-- to the program handle and do not use the structure
-- directly.
type spe_program_handle is
  record
    handle_size		: unsigned;
    elf_image_ptr	: addr_ptr;
    toe_shadow_ptr	: addr_ptr;
  end record;
type spe_program_handle_t is access spe_program_handle;

-- SPE context
-- The SPE context is one of the base data structures for
-- the libspe2 implementation. It holds all persistent information
-- about a "logical SPE" used by the application. This data
-- structure should not be accessed directly, but the application
-- uses a pointer to an SPE context as an identifier for the
-- "logical SPE" it is dealing with through libspe2 API calls.
type spe_context is
  record
    handle		: spe_program_handle;
    base_private	: addr_ptr;
    event_private	: addr_ptr;
  end record;
type spe_context_ptr_t is access spe_context;

-- SPE gang context
-- The SPE gang context is one of the base data structures for
-- the libspe2 implementation. It holds all persistent information
-- about a group of SPE contexts that should be treated as a gang,
-- i.e., be execute together with certain properties. This data
-- structure should not be accessed directly, but the application
-- uses a pointer to an SPE gang context as an identifier for the
-- SPE gang it is dealing with through libspe2 API calls.
type spe_gang_context is
  record
    base_private	: addr_ptr;
    event_private	: addr_ptr;
  end record; 
type spe_gang_context_ptr_t is access spe_gang_context;

type index is range 1 .. 8;

type spe_stop_info (reason: index := 1) is
  record
   spu_status	: Integer;
   reserved	: Integer;
   case reason is
    when 1 	=>	spe_exit_code		: Integer;
    when 2	=>	spe_signal_code		: Integer;
    when 3	=>	spe_runtime_error	: Integer;
    when 4	=>	spe_runtime_exception	: Integer;
    when 5	=>	spe_runtime_fatal	: Integer;
    when 6	=>	spe_callback_error	: Integer;
    when 7	=>	reserved_ptr		: addr_ptr;
    when 8	=>	reserved_u64		: unsigned_long;
   end case;
  end record;

for spe_stop_info use 
  record
   reason			at 0 range 0 .. 31;
   reserved			at 4 range 0 .. 31;-- only for 8 bytes alignment
   spe_exit_code		at 8 range 0 .. 31;
   spe_signal_code		at 8 range 0 .. 31;  
   spe_runtime_error		at 8 range 0 .. 31;
   spe_runtime_exception	at 8 range 0 .. 31;
   spe_runtime_fatal		at 8 range 0 .. 31;
   spe_callback_error		at 8 range 0 .. 31;
   reserved_ptr			at 8 range 0 .. 63;  
   reserved_u64			at 8 range 0 .. 63;  
   spu_status			at 16 range 0 .. 31;
  end record;

for spe_stop_info'size use 5*32;

type spe_stop_info_ptr is access spe_stop_info;

type spe_event_handler_t is new Integer;
type spe_event_handler_ptr_t is new addr_ptr;


type spe_event_data_t is
  record
    	ptr		: addr_ptr;
-- FIXME: union without discriminant
--	u32		: unsigned;
--	u64		: unsigned_long;
  end record;
for spe_event_data_t'Size use 64;

type spe_event_unit_t is
  record
    events	: unsigned;
    spe		: spe_context_ptr_t;
    data	: spe_event_data_t;	
  end record;
type spe_event_unit_t_ptr is access spe_event_unit_t;

--
--  API symbolic constants
--
type ps_area is (SPE_MSSYNC_AREA, SPE_MFC_COMMAND_AREA, SPE_CONTROL_AREA,
		 SPE_SIG_NOTIFY_1_AREA, SPE_SIG_NOTIFY_2_AREA);

--
-- Flags for spe_context_create
--
 SPE_CFG_SIGNOTIFY1_OR	: constant unsigned := 16#00000010#;
 SPE_CFG_SIGNOTIFY2_OR	: constant unsigned := 16#00000020#;
 SPE_MAP_PS 		: constant unsigned := 16#00000040#;
 SPE_ISOLATE		: constant unsigned := 16#00000080#;
 SPE_EVENTS_ENABLE	: constant unsigned := 16#00001000#;
 SPE_AFFINITY_MEMORY	: constant unsigned := 16#00002000#;

--
-- Symbolic constants for stop reasons
-- as returned in spe_stop_info_t
 SPE_EXIT		: constant Integer := 1;
 SPE_STOP_AND_SIGNAL	: constant Integer := 2;
 SPE_RUNTIME_ERROR	: constant Integer := 3;
 SPE_RUNTIME_EXCEPTION	: constant Integer := 4;
 SPE_RUNTIME_FATAL	: constant Integer := 5;
 SPE_CALLBACK_ERROR	: constant Integer := 6;

--
-- Runtime errors
--
 SPE_SPU_STOPPED_BY_STOP	: constant unsigned := 16#02#;-- INTERNAL USE ONLY
 SPE_SPU_HALT			: constant unsigned := 16#04#;
 SPE_SPU_WAITING_ON_CHANNEL	: constant unsigned := 16#08#;-- INTERNAL USE ONLY
 SPE_SPU_SINGLE_STEP		: constant unsigned := 16#10#;
 SPE_SPU_INVALID_INSTR		: constant unsigned := 16#20#;
 SPE_SPU_INVALID_CHANNEL	: constant unsigned := 16#40#;

--
-- Runtime exceptions
--
 SPE_DMA_ALIGNMENT	: constant unsigned := 16#0008#;
 SPE_DMA_SEGMENTATION	: constant unsigned := 16#0020#;
 SPE_DMA_STORAGE	: constant unsigned := 16#0040#;

-- 
-- Supported SPE events
--
 SPE_EVENT_OUT_INTR_MBOX: constant unsigned := 16#00000001#;
 SPE_EVENT_IN_MBOX	: constant unsigned := 16#00000002#;
 SPE_EVENT_TAG_GROUP	: constant unsigned := 16#00000004#;
 SPE_EVENT_SPE_STOPPED	: constant unsigned := 16#00000008#;
 SPE_EVENT_ALL_EVENTS   : constant unsigned := 16#0000000F#; 

--
-- Behavior flags for mailbox read/write functions
--
 SPE_MBOX_ALL_BLOCKING		: constant unsigned := 1;
 SPE_MBOX_ANY_BLOCKING		: constant unsigned := 2;                    
 SPE_MBOX_ANY_NONBLOCKING	: constant unsigned := 3;             

--
-- Behavior flags tag status functions
--
 SPE_TAG_ALL		: constant unsigned := 1;
 SPE_TAG_ANY		: constant unsigned := 2;                    
 SPE_TAG_IMMEDIATE	: constant unsigned := 3;             

--
-- Flags for _base_spe_context_run
--
 SPE_DEFAULT_ENTRY	: constant unsigned := 2**int'Size-1;-- UINT_MAX on spe
-- 128b user data for r3-5. --
 SPE_RUN_USER_REGS      : constant Integer := 16#00000001#;
 SPE_NO_CALLBACKS	: constant Integer := 16#00000002#;

--
--
--
 SPE_CALLBACK_NEW	: constant unsigned := 1;
 SPE_CALLBACK_UPDATE	: constant unsigned := 2;

--
--  
--
 SPE_COUNT_PHYSICAL_CPU_NODES	: constant Integer := 1;
 SPE_COUNT_PHYSICAL_SPES	: constant Integer := 2;                   
 SPE_COUNT_USABLE_SPES		: constant Integer := 3;             

--
-- Signal Targets
--
 SPE_SIG_NOTIFY_REG_1	: constant unsigned := 1;
 SPE_SIG_NOTIFY_REG_2   : constant unsigned := 2;

end libspe2_types;

