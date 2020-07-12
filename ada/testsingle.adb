 -- 
 -- test functions: 
 --    spe_context_create 
 --    spe_image_open
 --    spe_program_load
 --    spe_context_run
 --    spe_context_destroy
 --    spe_image_close
 -- 
with System;
with Ada.Text_IO;
with Interfaces.C;
with libspe2;
with libspe2_types;

procedure testsingle is
  use System;
  use Ada.Text_IO;
  use Interfaces.C;
  use libspe2;
  use libspe2_types;

  ctx : spe_context_ptr_t;
  flags : unsigned := 0;
  gang : spe_gang_context_ptr_t;
  program : spe_program_handle_t;
  argp : addr_ptr := Null;
  envp : addr_ptr := Null;
  rc : Integer;

  spe_entry : unsigned := SPE_DEFAULT_ENTRY;
  spe_entry_ptr : unsigned_ptr := new unsigned'(spe_entry);
  stop_info_ptr : spe_stop_info_ptr := new spe_stop_info;

  filename : String (1 .. 5) := "hello";
  filename_c : char_array (1 .. 6);

begin
  filename_c := To_C (filename, True); 
  program := spe_image_open(filename_c);
  if program = NULL then
    Put_Line("spe_image_open failed!");
  end if;
 
  gang := Null;
  ctx := spe_context_create(flags, gang);
  if ctx = NULL then
    Put_Line("spe_context_create failed!");
  end if;

  rc := spe_program_load(ctx, program);
  if rc /= 0 then
    Put_Line("spe_program_load failed!");
  end if;

  rc := spe_context_run(ctx, spe_entry_ptr, 0, argp, envp, stop_info_ptr);
  Put_Line("exit_code: " & Integer'Image(stop_info_ptr.spe_exit_code));
  if rc /= 0 then
    Put_Line("spe_context_run failed!");
  end if;

  rc := spe_context_destroy (ctx); 
  if rc /= 0 then
    Put_Line("spe_context_destroy failed!");
  end if;
 
  rc := spe_image_close(program);
  if rc /= 0 then
    Put_Line("spe_image_close failed!");
  end if;

end testsingle;


