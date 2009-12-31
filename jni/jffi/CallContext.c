#include <sys/param.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#if defined(__sun) || defined(_AIX)
#  include <sys/sysmacros.h>
#  include <alloca.h>
#endif
#ifdef _WIN32
#  include <malloc.h>
#endif
#include <ffi.h>
#include <jni.h>
#include "jffi.h"
#include "Exception.h"
#include "CallContext.h"
#include "com_kenai_jffi_Foreign.h"

#ifndef MAX
#  define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

static inline int FFI_ALIGN(int v, int a) {
    return ((((size_t) v) - 1) | (a - 1)) +1;
}

/*
 * Class:     com_kenai_jffi_Foreign
 * Method:    newCallContext
 * Signature: (I[II)J
 */
JNIEXPORT jlong JNICALL
Java_com_kenai_jffi_Foreign_newCallContext(JNIEnv* env, jobject self,
        jlong returnType, jlongArray paramArray, jint flags)
{
    CallContext* ctx = NULL;
    jlong* paramTypes;
    int paramCount, i, rawOffset = 0;
    ffi_type* ffiParamTypes;
    int ffiStatus;
    int abi;

    paramCount = (*env)->GetArrayLength(env, paramArray);
    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        throwException(env, OutOfMemory, "Failed to allocate CallContext");
        goto cleanup;
    }
    ctx->ffiParamTypes = calloc(MAX(1, paramCount), sizeof(ffi_type *));
    if (ctx->ffiParamTypes == NULL) {
        throwException(env, OutOfMemory, "Failed to allocate CallContext#ffiParamTypes");
        goto cleanup;
    }
    ctx->rawParamOffsets = calloc(MAX(1, paramCount), sizeof(*ctx->rawParamOffsets));
    if (ctx->rawParamOffsets == NULL) {
        throwException(env, OutOfMemory, "Failed to allocate CallContext#rawParamOffsets");
        goto cleanup;
    }

    paramTypes = alloca(paramCount * sizeof(jlong));
    (*env)->GetLongArrayRegion(env, paramArray, 0, paramCount, paramTypes);

    for (i = 0; i < paramCount; ++i) {
        ffi_type* type = (ffi_type *) j2p(paramTypes[i]);
        if (type == NULL) {
            throwException(env, IllegalArgument, "Invalid parameter type: %#x", paramTypes[i]);
            goto cleanup;
        }
        ctx->ffiParamTypes[i] = type;
        ctx->rawParamOffsets[i] = rawOffset;
        rawOffset += FFI_ALIGN(type->size, FFI_SIZEOF_ARG);
    }

    // On win32, we might need to set the abi to stdcall - but win64 only supports cdecl/default
#if defined(_WIN32) && !defined(_WIN64)
    abi = (flags & com_kenai_jffi_Foreign_F_STDCALL) != 0 ? FFI_STDCALL : FFI_DEFAULT_ABI;
#else
    abi = FFI_DEFAULT_ABI;
#endif

    ffiStatus = ffi_prep_cif(&ctx->cif, abi, paramCount, (ffi_type *) j2p(returnType),
            ctx->ffiParamTypes);
    switch (ffiStatus) {
        case FFI_OK:
            break;
        case FFI_BAD_TYPEDEF:
            throwException(env, IllegalArgument, "Bad typedef");
            goto cleanup;
        case FFI_BAD_ABI:
            throwException(env, Runtime, "Invalid ABI");
            goto cleanup;
        default:
            throwException(env, Runtime, "Unknown FFI error");
    }
    ctx->rawParameterSize = rawOffset;
    /* Save errno unless explicitly told not to do so */
    ctx->saveErrno = (flags & com_kenai_jffi_Foreign_F_NOERRNO) == 0;

    return p2j(ctx);
cleanup:
    if (ctx != NULL) {
        if (ctx->rawParamOffsets != NULL) {
            free(ctx->rawParamOffsets);
        }
        if (ctx->ffiParamTypes != NULL) {
            free(ctx->ffiParamTypes);
        }
       free(ctx);
    }
    return 0LL;
}

/*
 * Class:     com_kenai_jffi_Foreign
 * Method:    freeCallContext
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_com_kenai_jffi_Foreign_freeCallContext(JNIEnv* env, jobject self, jlong handle)
{
    CallContext* ctx = (CallContext *) j2p(handle);
    if (ctx != NULL) {
        if (ctx->rawParamOffsets != NULL) {
            free(ctx->rawParamOffsets);
        }
        if (ctx->ffiParamTypes != NULL) {
            free(ctx->ffiParamTypes);
        }
        free(ctx);
    }
}

/*
 * Class:     com_kenai_jffi_Foreign
 * Method:    getCallContextRawParameterSize
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL
Java_com_kenai_jffi_Foreign_getCallContextRawParameterSize(JNIEnv* env, jobject self, jlong handle)
{
    CallContext* ctx = (CallContext *) j2p(handle);
    return ctx->rawParameterSize;
}

