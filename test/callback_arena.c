#include "../src/win32ole.h"
#include <mruby/gc.h>
#include <string.h>

void mrb_mruby_win32ole_reinitialize_test(mrb_state *mrb);
void mrb_mruby_win32ole_early_invoke_test(mrb_state *mrb);

static mrb_value
callback_arena_exercise(mrb_state *mrb, mrb_value self)
{
    const char *method;
    mrb_int iterations;
    VARIANT target;
    DISPPARAMS params;
    DISPID dispid;
    BSTR name;
    HRESULT hr;
    int arena;
    mrb_int i;
    BOOL expect_exception;
    BOOL expect_string;
    BOOL expect_byref;

    mrb_get_args(mrb, "zi", &method, &iterations);
    expect_exception = strcmp(method, "raise_error") == 0;
    expect_string = strcmp(method, "string_result") == 0;
    expect_byref = strcmp(method, "byref_result") == 0;
    if (iterations < 1) mrb_raise(mrb, E_ARGUMENT_ERROR, "iterations must be positive");

    VariantInit(&target);
    ole_val2variant(mrb, self, &target);
    if (V_VT(&target) != VT_DISPATCH || V_DISPATCH(&target) == NULL) {
        VariantClear(&target);
        mrb_raise(mrb, E_RUNTIME_ERROR, "object did not convert to VT_DISPATCH");
    }
    memset(&params, 0, sizeof(params));
    name = SysAllocStringLen(NULL, (UINT)strlen(method));
    if (!name) {
        VariantClear(&target);
        mrb_raise(mrb, E_RUNTIME_ERROR, "could not allocate method name");
    }
    MultiByteToWideChar(CP_ACP, 0, method, -1, name, (int)strlen(method) + 1);

    /* Conversion may populate the proxy cache. Measure callback-local arena use
       from the stable point after conversion. */
    arena = mrb_gc_arena_save(mrb);
    for (i = 0; i < iterations; i++) {
        VARIANT result;

        hr = V_DISPATCH(&target)->lpVtbl->GetIDsOfNames(
            V_DISPATCH(&target), &IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &dispid);
        if (FAILED(hr)) {
            SysFreeString(name);
            VariantClear(&target);
            mrb_raise(mrb, E_RUNTIME_ERROR, "GetIDsOfNames failed");
        }
        if (mrb_gc_arena_save(mrb) != arena) {
            SysFreeString(name);
            VariantClear(&target);
            mrb_raise(mrb, E_RUNTIME_ERROR, "GetIDsOfNames changed the GC arena index");
        }

        VariantInit(&result);
        hr = V_DISPATCH(&target)->lpVtbl->Invoke(
            V_DISPATCH(&target), dispid, &IID_NULL, LOCALE_USER_DEFAULT,
            DISPATCH_METHOD, &params, &result, NULL, NULL);

        if (expect_exception) {
            if (hr != DISP_E_EXCEPTION) {
                VariantClear(&result);
                SysFreeString(name);
                VariantClear(&target);
                mrb_raise(mrb, E_RUNTIME_ERROR, "Ruby exception did not become DISP_E_EXCEPTION");
            }
        }
        else if (FAILED(hr)) {
            VariantClear(&result);
            SysFreeString(name);
            VariantClear(&target);
            mrb_raise(mrb, E_RUNTIME_ERROR, "Invoke failed");
        }
        else if (expect_byref ? V_VT(&result) != (VT_BSTR | VT_BYREF) :
                 (expect_string ? V_VT(&result) != VT_BSTR : V_VT(&result) != VT_I4)) {
            VariantClear(&result);
            SysFreeString(name);
            VariantClear(&target);
            mrb_raise(mrb, E_RUNTIME_ERROR, "Invoke returned an unexpected VARIANT type");
        }
        if (mrb_gc_arena_save(mrb) != arena + (expect_byref ? 1 : 0)) {
            VariantClear(&result);
            SysFreeString(name);
            VariantClear(&target);
            mrb_raise(mrb, E_RUNTIME_ERROR, "Invoke changed the GC arena index");
        }

        if (expect_byref) mrb_full_gc(mrb);
        if (!expect_exception) {
            mrb_value ruby_result = ole_variant2val(mrb, &result);
            if (expect_string || expect_byref) {
                mrb_full_gc(mrb);
                if (!mrb_string_p(ruby_result) ||
                    RSTRING_LEN(ruby_result) != (mrb_int)strlen("callback result") ||
                    memcmp(RSTRING_PTR(ruby_result), "callback result", strlen("callback result")) != 0) {
                    VariantClear(&result);
                    SysFreeString(name);
                    VariantClear(&target);
                    mrb_raise(mrb, E_RUNTIME_ERROR, "String result changed after GC");
                }
            }
            else if (!mrb_fixnum_p(ruby_result) || mrb_fixnum(ruby_result) != 42) {
                VariantClear(&result);
                SysFreeString(name);
                VariantClear(&target);
                mrb_raise(mrb, E_RUNTIME_ERROR, "immediate result was not 42");
            }
        }

        VariantClear(&result);
        mrb_gc_arena_restore(mrb, arena);
    }
    SysFreeString(name);
    VariantClear(&target);
    return mrb_true_value();
}

void
mrb_mruby_win32ole_gem_test(mrb_state *mrb)
{
    mrb_define_method(mrb, mrb->object_class, "__win32ole_callback_arena_exercise",
                      callback_arena_exercise, MRB_ARGS_REQ(2));
    mrb_mruby_win32ole_reinitialize_test(mrb);
    mrb_mruby_win32ole_early_invoke_test(mrb);
}
