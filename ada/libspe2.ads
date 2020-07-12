-----------------------------------------------------------
-- 
-- libspe2 API functions
-- 
-----------------------------------------------------------
with System;
with Interfaces.C;
with libspe2_types;

package libspe2 is
  use System;
  use Interfaces.C;
  use libspe2_types;

--
-- link with libspe2
--
  pragma Linker_Options("-lspe2");

--
-- spe_context_create
--
  function spe_context_create
    (flags	: unsigned;
     gang	: spe_gang_context_ptr_t) 
  return spe_context_ptr_t;
  pragma Import (C, spe_context_create, "spe_context_create");

--
-- spe_context_create_affinity
--
  function spe_context_create_affinity
    (flags		: unsigned;
     affinity_neighbor	: spe_context_ptr_t;
     gang		: spe_gang_context_ptr_t) 
  return spe_context_ptr_t;
  pragma Import (C, spe_context_create_affinity, "spe_context_create_affinity");

--
-- spe_context_destroy
--
  function spe_context_destroy
    (spe  : spe_context_ptr_t)
  return Integer;
  pragma Import (C, spe_context_destroy, "spe_context_destroy");

--
-- spe_gang_context_create
--
  function spe_gang_context_create
    (flags	: unsigned)
  return spe_gang_context_ptr_t;
  pragma Import (C, spe_gang_context_create, "spe_gang_context_create");

--
-- spe_gang_context_destroy
--
  function spe_gang_context_destroy
    (gang  : spe_gang_context_ptr_t)
  return Integer;
  pragma Import (C, spe_gang_context_destroy, "spe_gang_context_destroy");

--
-- spe_image_open
--
  function spe_image_open
    (filename  : char_array)
    return spe_program_handle_t;
  pragma Import (C, spe_image_open, "spe_image_open");

--
-- spe_image_close
--
  function spe_image_close
    (program  : spe_program_handle_t)
    return Integer;
  pragma Import (C, spe_image_close, "spe_image_close");

--
-- spe_program_load
--
  function spe_program_load
    (spe        : spe_context_ptr_t;
     program    : spe_program_handle_t)
  return Integer;
  pragma Import (C, spe_program_load, "spe_program_load");

--
-- spe_context_run
--
  function spe_context_run
    (spe                : spe_context_ptr_t;
     spe_entry_ptr     	: access unsigned;
     run_flags          : unsigned;
     argp               : addr_ptr;
     envp               : addr_ptr;
     stop_info		: spe_stop_info_ptr)
  return Integer;
  pragma Import (C, spe_context_run, "spe_context_run");

--
-- spe_stop_info_read
--
  function spe_stop_info_read
    (spe      	  : spe_context_ptr_t;
     stop_info    : spe_stop_info_ptr)
  return Integer;
  pragma Import (C, spe_stop_info_read, "spe_stop_info_read");

--
-- spe_event_handler_create
--
  function spe_event_handler_create 
  return spe_event_handler_ptr_t;
  pragma Import (C, spe_event_handler_create, "spe_event_handler_create");

--
-- spe_event_handler_destroy
--
  function spe_event_handler_destroy 
    (evhandler	:spe_event_handler_ptr_t)
  return Integer;
  pragma Import (C, spe_event_handler_destroy, "spe_event_handler_destroy");

--
-- spe_event_handler_register
--
  function spe_event_handler_register 
    (evhandler	: spe_event_handler_ptr_t;
     event	: spe_event_unit_t_ptr)
  return Integer;
  pragma Import (C, spe_event_handler_register, "spe_event_handler_register");

--
-- spe_event_handler_deregister
--
  function spe_event_handler_deregister 
    (evhandler	: spe_event_handler_ptr_t;
     event	: spe_event_unit_t_ptr)
  return Integer;
  pragma Import (C, spe_event_handler_deregister, "spe_event_handler_deregister");

--
-- spe_event_wait
--
  function spe_event_wait 
    (evhandler	: spe_event_handler_ptr_t;
     events	: addr_ptr;
     max_events	: Integer;
     timeout	: Integer)
  return Integer;
  pragma Import (C, spe_event_wait, "spe_event_wait");

--
-- MFCIO Proxy Commands
--
  function spe_mfcio_put
    (spe  	: spe_context_ptr_t;
     ls		: unsigned;
     ea 	: addr_ptr;
     size	: unsigned;
     tag	: unsigned;
     tid	: unsigned;
     rid	: unsigned)
  return Integer;
  pragma Import (C, spe_mfcio_put, "spe_mfcio_put");

  function spe_mfcio_putb
    (spe  	: spe_context_ptr_t;
     ls		: unsigned;
     ea 	: addr_ptr;
     size	: unsigned;
     tag	: unsigned;
     tid	: unsigned;
     rid	: unsigned)
  return Integer;
  pragma Import (C, spe_mfcio_putb, "spe_mfcio_putb");

  function spe_mfcio_putf
    (spe  	: spe_context_ptr_t;
     ls		: unsigned;
     ea 	: addr_ptr;
     size	: unsigned;
     tag	: unsigned;
     tid	: unsigned;
     rid	: unsigned)
  return Integer;
  pragma Import (C, spe_mfcio_putf, "spe_mfcio_putf");

  function spe_mfcio_get
    (spe  	: spe_context_ptr_t;
     ls		: unsigned;
     ea 	: addr_ptr;
     size	: unsigned;
     tag	: unsigned;
     tid	: unsigned;
     rid	: unsigned)
  return Integer;
  pragma Import (C, spe_mfcio_get, "spe_mfcio_get");

  function spe_mfcio_getb
    (spe  	: spe_context_ptr_t;
     ls		: unsigned;
     ea 	: addr_ptr;
     size	: unsigned;
     tag	: unsigned;
     tid	: unsigned;
     rid	: unsigned)
  return Integer;
  pragma Import (C, spe_mfcio_getb, "spe_mfcio_getb");

  function spe_mfcio_getf
    (spe  	: spe_context_ptr_t;
     ls		: unsigned;
     ea 	: addr_ptr;
     size	: unsigned;
     tag	: unsigned;
     tid	: unsigned;
     rid	: unsigned)
  return Integer;
  pragma Import (C, spe_mfcio_getf, "spe_mfcio_getf");

--
-- MFCIO Tag Group Completion
--
  function spe_mfcio_tag_status_read
    (spe  		: spe_context_ptr_t;
     mask		: unsigned;
     behavior		: unsigned;
     tag_status_ptr	: unsigned_ptr)
  return Integer;
  pragma Import (C, spe_mfcio_tag_status_read, "spe_mfcio_tag_status_read");

--
-- SPE Mailbox Facility
--
  function spe_out_mbox_read
    (spe  		: spe_context_ptr_t;
     mbox_data_ptr	: unsigned_ptr;
     count		: Integer)
  return Integer;
  pragma Import (C, spe_out_mbox_read, "spe_out_mbox_read");

  function spe_out_mbox_status
    (spe  		: spe_context_ptr_t)
  return Integer;
  pragma Import (C, spe_out_mbox_status, "spe_out_mbox_status");

  function spe_in_mbox_write
    (spe  		: spe_context_ptr_t;
     mbox_data_ptr	: unsigned_ptr;
     count		: Integer;
     behavior		: unsigned)
  return Integer;
  pragma Import (C, spe_in_mbox_write, "spe_in_mbox_write");

  function spe_in_mbox_status
    (spe  		: spe_context_ptr_t)
  return Integer;
  pragma Import (C, spe_in_mbox_status, "spe_in_mbox_status");

  function spe_out_intr_mbox_read
    (spe  		: spe_context_ptr_t;
     mbox_data_ptr	: unsigned_ptr;
     count		: Integer;
     behavior		: unsigned)
  return Integer;
  pragma Import (C, spe_out_intr_mbox_read, "spe_out_intr_mbox_read");

  function spe_out_intr_mbox_status
    (spe  		: spe_context_ptr_t)
  return Integer;
  pragma Import (C, spe_out_intr_mbox_status, "spe_out_intr_mbox_status");

--
-- SPU SIgnal Notification Facility
--
  function spe_signal_write
    (spe  		: spe_context_ptr_t;
     signal_reg		: unsigned;
     data		: unsigned)
  return Integer;
  pragma Import (C, spe_signal_write, "spe_signal_write");

--
-- spe_ls_area_get
--
  function spe_ls_area_get
    (spe  		: spe_context_ptr_t)
  return addr_ptr;
  pragma Import (C, spe_ls_area_get, "spe_ls_area_get");

--
-- spe_ls_size_get
--
  function spe_ls_size_get
    (spe  		: spe_context_ptr_t)
  return Integer;
  pragma Import (C, spe_ls_size_get, "spe_ls_size_get");

--
-- spe_ps_area_get
--
  function spe_ps_area_get
    (spe  		: spe_context_ptr_t;
     area		: ps_area)
  return addr_ptr;
  pragma Import (C, spe_ps_area_get, "spe_ps_area_get");

--
-- spe_callback_handler_register
--
  function spe_callback_handler_register
    (handler  		: addr_ptr;
     callnum		: unsigned;
     mode		: unsigned)
  return Integer;
  pragma Import (C, spe_callback_handler_register, "spe_callback_handler_register");

--
-- spe_callback_handler_deregister
--
  function spe_callback_handler_deregister
    (callnum		: unsigned)
  return Integer;
  pragma Import (C, spe_callback_handler_deregister, "spe_callback_handler_deregister");

--
-- spe_callback_handler_query
--
  function spe_callback_handler_query
    (callnum		: unsigned)
  return spe_event_handler_ptr_t;
  pragma Import (C, spe_callback_handler_query, "spe_callback_handler_query");

--
-- spe_cpu_info_get
--
  function spe_cpu_info_get
    (info_requested	: Integer;
     cpu_node		: Integer)
  return Integer;
  pragma Import (C, spe_cpu_info_get, "spe_cpu_info_get");

end libspe2;
