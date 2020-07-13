/* --------------------------------------------------------------- */
/* (C) Copyright 2001,2006,                                        */
/* International Business Machines Corporation,                    */
/*                                                                 */
/* All Rights Reserved.                                            */
/* --------------------------------------------------------------- */
/* PROLOG END TAG zYx                                              */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <libspe2.h>

int main(void)
{
    spe_context_ptr_t context;
    int flags = 0;
    unsigned int entry = SPE_DEFAULT_ENTRY;
    void *argp = "argp";
    void *envp = "envp";
    spe_program_handle_t *program;
    spe_stop_info_t stop_info;
    int rc;

    printf("Extering main\n");

    printf("Invocation of spe_context_create\n");
    if ((context = spe_context_create(flags, NULL)) == NULL) {
        fprintf(stderr,
                "Invocation of spe_create_single FAILED, context=%p, errno=%d\n",
                context, errno);
        fflush(stderr);
        return -2;
    }

    printf("Invocation of spe_image_open, context=%p\n", context);
    if ((program = spe_image_open("testlibspe2hello.spu.elf")) == NULL) {
        fprintf(stderr,
                "Invocation of spe_image_open FAILED, context=%p, errno=%d\n",
                context, errno);
        fflush(stderr);
        return -1;
    }

    printf("Invocation of spe_program_load, context=%p\n", context);
    if ((rc = spe_program_load(context, program)) != 0) {
        fprintf(stderr,
                "Invocation of spe_program_load FAILED, context=%p, rc=%d, errno=%d\n",
                context, rc, errno);
        fflush(stderr);
        return -3;
    }

    printf
        ("Invocation of spe_context_run, content=%p, entry=0x%x, argp=%p, envp=%p\n",
         context, entry, argp, envp);
    if ((rc =
         spe_context_run(context, &entry, 0, argp, envp,
                         &stop_info)) < 0) {
        fprintf(stderr,
                "Invocation of spe_context_run FAILED, context=%p, rc=%d, errno=%d\n",
                context, rc, errno);
        fflush(stderr);
        return -4;
    }
    printf
        ("After spe_context_run, context=%p, rc=%d, entry=0x%x, stop_info.stop_reason=0x%x\n",
         context, rc, entry, stop_info.stop_reason);
    switch (stop_info.stop_reason) {
    case SPE_EXIT:
        printf
            ("After spe_context_run, context=%p, SPE_EXIT stop_info.result.stop_exit_code=%d\n",
             context, stop_info.result.spe_exit_code);
        break;
    case SPE_STOP_AND_SIGNAL:
        printf
            ("After spe_context_run, context=%p, SPE_STOP_AND_SIGNAL stop_info.result.stop_signal_code=%d\n",
             context, stop_info.result.spe_signal_code);
        break;
    case SPE_RUNTIME_ERROR:
        printf
            ("After spe_context_run, context=%p, SPE_RUNTIME_ERROR stop_info.result.spe_runtime_error=%d\n",
             context, stop_info.result.spe_runtime_error);
        break;
    case SPE_RUNTIME_EXCEPTION:
        printf
            ("After spe_context_run, context=%p, SPE_RUNTIME_EXCEPTION stop_info.result.spe_runtime_exception=%d\n",
             context, stop_info.result.spe_runtime_exception);
        break;
    case SPE_RUNTIME_FATAL:
        printf
            ("After spe_context_run, context=%p, SPE_RUNTIME_FATAL stop_info.result.spe_runtime_fatal=%d\n",
             context, stop_info.result.spe_runtime_fatal);
        break;
    case SPE_CALLBACK_ERROR:
        printf
            ("After spe_context_run, context=%p, SPE_CALLBACK_ERROR stop_info.result.spe_callback_error=%d\n",
             context, stop_info.result.spe_callback_error);
        break;
    default:
        printf("After spe_context_run, context=%p, UNKNOWN\n", context);
        break;
    }
    printf
        ("After spe_context_run, context=%p, stop_info.spu_status=0x%x\n",
         context, stop_info.spu_status);

    printf("Invocation of spe_context_destroy, context=%p\n", context);
    if ((rc = spe_context_destroy(context)) < 0) {
        fprintf(stderr,
                "Invocation of spe_context_destroy FAILED, context=%p, rc=%d, errno=%d\n",
                context, rc, errno);
        fflush(stderr);
        return -5;
    }

    printf("Exiting main\n");

    return 0;
}
