/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "edify/expr.h"
#include "dec.h"
#include "gpt-utils.h"

Value* DecryptFn(const char* name, State* state, int argc, Expr* argv[]) {
    int rc = -1;
    char *src_file, *dst_file;

    if (argc != 2)
        return ErrorAbort(state, "%s expects 2 args, got %d", name, argc);

    if (ReadArgs(state, argv, 2, &src_file, &dst_file))
        return NULL;

    rc = decrypt_image(src_file, dst_file);

    free(src_file);
    free(dst_file);

    return StringValue(strdup(rc >= 0 ? "t" : ""));
}

Value* BootUpdateFn(const char* name, State* state, int argc, Expr* argv[])
{
    int rc = 0;
    char *stageStr;
    enum boot_update_stage stage;

    if (argc != 1)
        return ErrorAbort(state, "%s() expects 1 args, got %d", name, argc);

    if (ReadArgs(state, argv, 1, &stageStr))
        return NULL;

    if (!strcmp(stageStr, "main"))
        stage = UPDATE_MAIN;
    else if (!strcmp(stageStr, "backup"))
        stage = UPDATE_BACKUP;
    else if (!strcmp(stageStr, "finalize"))
        stage = UPDATE_FINALIZE;
    else {
        fprintf(stderr, "Unrecognized boot update stage, exitting\n");
        rc = -1;
    }

    free(stageStr);

    if (!rc)
        rc = prepare_boot_update(stage);

    return StringValue(strdup(rc ? "" : "t"));
}

void Register_librecovery_updater_msm8998() {
    RegisterFunction("msm.decrypt", DecryptFn);
    RegisterFunction("msm.boot_update", BootUpdateFn);
}
