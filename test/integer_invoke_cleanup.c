#include "../src/win32ole.h"
#include <stdint.h>
#include <string.h>

struct invoke_cleanup_fake {
    IDispatchVtbl *lpVtbl;
    ULONG refs;
    ULONG invoke_calls;
    IDispatch *return_dispatch;
    BOOL write_large_output;
};

struct invoke_cleanup_call {
    mrb_value receiver;
    mrb_value dispatch_argument;
    mrb_value integer_argument;
    BOOL early;
    BOOL output_case;
};

static ULONG STDMETHODCALLTYPE
invoke_cleanup_addref(IDispatch *self)
{
    return ++((struct invoke_cleanup_fake *)self)->refs;
}

static ULONG STDMETHODCALLTYPE
invoke_cleanup_release(IDispatch *self)
{
    return --((struct invoke_cleanup_fake *)self)->refs;
}

static HRESULT STDMETHODCALLTYPE
invoke_cleanup_queryinterface(IDispatch *self, REFIID riid, void **object)
{
    if (!object) return E_POINTER;
    *object = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDispatch)) {
        *object = self;
        invoke_cleanup_addref(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE
invoke_cleanup_get_typeinfo_count(IDispatch *self, UINT *count)
{
    (void)self;
    if (!count) return E_POINTER;
    *count = 0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
invoke_cleanup_get_typeinfo(IDispatch *self, UINT index, LCID lcid, ITypeInfo **typeinfo)
{
    (void)self;
    (void)index;
    (void)lcid;
    if (typeinfo) *typeinfo = NULL;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
invoke_cleanup_get_ids(IDispatch *self, REFIID riid, LPOLESTR *names, UINT count,
                       LCID lcid, DISPID *dispids)
{
    UINT i;
    (void)self;
    (void)riid;
    (void)names;
    (void)lcid;
    if (!dispids && count > 0) return E_POINTER;
    for (i = 0; i < count; ++i) dispids[i] = 1;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
invoke_cleanup_invoke(IDispatch *self, DISPID dispid, REFIID riid, LCID lcid, WORD flags,
                      DISPPARAMS *params, VARIANT *result, EXCEPINFO *excepinfo,
                      UINT *arg_error)
{
    struct invoke_cleanup_fake *fake = (struct invoke_cleanup_fake *)self;
    UINT i;
    BOOL wrote_output = FALSE;
    (void)dispid;
    (void)riid;
    (void)lcid;
    (void)flags;
    (void)excepinfo;
    (void)arg_error;
    fake->invoke_calls++;
    if (!fake->write_large_output) return S_OK;
    if (!params || !result) return E_POINTER;

    VariantInit(result);
    V_VT(result) = VT_DISPATCH;
    V_DISPATCH(result) = fake->return_dispatch;
    fake->return_dispatch->lpVtbl->AddRef(fake->return_dispatch);

    for (i = 0; i < params->cArgs; ++i) {
        VARIANTARG *argument = &params->rgvarg[i];
        if (V_VT(argument) == (VT_VARIANT | VT_BYREF)) {
            VARIANT *value = V_VARIANTREF(argument);
            if (value && V_VT(value) != VT_DISPATCH) {
                VariantClear(value);
                V_VT(value) = VT_UI8;
                V_UI8(value) = UINT64_MAX;
                wrote_output = TRUE;
                break;
            }
        }
    }
    return wrote_output ? S_OK : E_INVALIDARG;
}

static void
invoke_cleanup_init_vtable(IDispatchVtbl *vtbl)
{
    memset(vtbl, 0, sizeof(*vtbl));
    vtbl->QueryInterface = invoke_cleanup_queryinterface;
    vtbl->AddRef = invoke_cleanup_addref;
    vtbl->Release = invoke_cleanup_release;
    vtbl->GetTypeInfoCount = invoke_cleanup_get_typeinfo_count;
    vtbl->GetTypeInfo = invoke_cleanup_get_typeinfo;
    vtbl->GetIDsOfNames = invoke_cleanup_get_ids;
    vtbl->Invoke = invoke_cleanup_invoke;
}

static mrb_value
invoke_cleanup_wrap_dispatch(mrb_state *mrb, struct invoke_cleanup_fake *fake)
{
    VARIANT variant;
    mrb_value object;
    VariantInit(&variant);
    V_VT(&variant) = VT_DISPATCH;
    V_DISPATCH(&variant) = (IDispatch *)fake;
    object = ole_variant2val(mrb, &variant);
    VariantClear(&variant);
    return object;
}

static mrb_value
invoke_cleanup_call_method(mrb_state *mrb, void *data)
{
    struct invoke_cleanup_call *call = data;
    if (call->early) {
        mrb_value args = mrb_ary_new_capa(mrb, 2);
        mrb_value types = mrb_ary_new_capa(mrb, 2);
        mrb_value first = call->output_case ? INT2FIX(0) : call->integer_argument;
        mrb_ary_push(mrb, args, first);
        mrb_ary_push(mrb, args, call->dispatch_argument);
        mrb_ary_push(mrb, types, INT2FIX(VT_VARIANT));
        mrb_ary_push(mrb, types, INT2FIX(VT_VARIANT));
        return mrb_funcall(mrb, call->receiver, "_invoke", 3,
                           INT2FIX(1), args, types);
    }

    if (call->output_case) {
        return mrb_funcall(mrb, call->receiver, "invoke", 3,
                           mrb_str_new_lit(mrb, "CleanupOutput"),
                           INT2FIX(0), call->dispatch_argument);
    }
    return mrb_funcall(mrb, call->receiver, "invoke", 3,
                       mrb_str_new_lit(mrb, "Overflow"),
                       call->dispatch_argument, call->integer_argument);
}

static mrb_bool
invoke_cleanup_is_range_error(mrb_state *mrb, mrb_value exception, mrb_bool error)
{
    return error && mrb_obj_is_kind_of(mrb, exception, mrb_class_get(mrb, "RangeError"));
}

#ifdef MRB_USE_BIGINT
static mrb_bool
invoke_cleanup_overflow_case(mrb_state *mrb, IDispatchVtbl *vtbl, BOOL early,
                             mrb_value oversized)
{
    struct invoke_cleanup_fake target;
    struct invoke_cleanup_fake argument;
    struct invoke_cleanup_call call;
    mrb_value receiver;
    mrb_value dispatch_argument;
    mrb_value exception;
    mrb_bool error = FALSE;
    mrb_bool range_error;
    mrb_bool references_ok;

    memset(&target, 0, sizeof(target));
    memset(&argument, 0, sizeof(argument));
    target.lpVtbl = argument.lpVtbl = vtbl;
    target.refs = argument.refs = 1;
    receiver = invoke_cleanup_wrap_dispatch(mrb, &target);
    dispatch_argument = invoke_cleanup_wrap_dispatch(mrb, &argument);

    call.receiver = receiver;
    call.dispatch_argument = dispatch_argument;
    call.integer_argument = oversized;
    call.early = early;
    call.output_case = FALSE;
    exception = mrb_protect_error(mrb, invoke_cleanup_call_method, &call, &error);
    range_error = invoke_cleanup_is_range_error(mrb, exception, error);
    references_ok = target.refs == 1 && argument.refs == 1 && target.invoke_calls == 0;

    mrb_funcall(mrb, receiver, "ole_free", 0);
    mrb_funcall(mrb, dispatch_argument, "ole_free", 0);
    return range_error && references_ok && target.refs == 0 && argument.refs == 0;
}
#endif

#ifndef MRB_USE_BIGINT
static mrb_bool
invoke_cleanup_output_case(mrb_state *mrb, IDispatchVtbl *vtbl, BOOL early)
{
    struct invoke_cleanup_fake target;
    struct invoke_cleanup_fake argument;
    struct invoke_cleanup_fake returned;
    struct invoke_cleanup_call call;
    mrb_value receiver;
    mrb_value dispatch_argument;
    mrb_value exception;
    mrb_bool error = FALSE;
    mrb_bool range_error;
    mrb_bool references_ok;

    memset(&target, 0, sizeof(target));
    memset(&argument, 0, sizeof(argument));
    memset(&returned, 0, sizeof(returned));
    target.lpVtbl = argument.lpVtbl = returned.lpVtbl = vtbl;
    target.refs = argument.refs = returned.refs = 1;
    target.return_dispatch = (IDispatch *)&returned;
    target.write_large_output = TRUE;
    receiver = invoke_cleanup_wrap_dispatch(mrb, &target);
    dispatch_argument = invoke_cleanup_wrap_dispatch(mrb, &argument);

    call.receiver = receiver;
    call.dispatch_argument = dispatch_argument;
    call.integer_argument = mrb_nil_value();
    call.early = early;
    call.output_case = TRUE;
    exception = mrb_protect_error(mrb, invoke_cleanup_call_method, &call, &error);
    range_error = invoke_cleanup_is_range_error(mrb, exception, error);
    references_ok = target.refs == 1 && argument.refs == 1 && returned.refs == 1 &&
                    target.invoke_calls == 1;

    mrb_funcall(mrb, receiver, "ole_free", 0);
    mrb_funcall(mrb, dispatch_argument, "ole_free", 0);
    invoke_cleanup_release((IDispatch *)&returned);
    return range_error && references_ok && target.refs == 0 && argument.refs == 0 && returned.refs == 0;
}
#endif

static mrb_value
integer_invoke_cleanup_bigint_exercise(mrb_state *mrb, mrb_value self)
{
    IDispatchVtbl vtbl = { 0 };
    static const uint8_t too_large_bytes[] = { 1, 0, 0, 0, 0, 0, 0, 0, 0 };
    mrb_value oversized = mrb_nil_value();
    mrb_bool late_ok = FALSE;
    mrb_bool early_ok = FALSE;
    (void)self;

#ifdef MRB_USE_BIGINT
    invoke_cleanup_init_vtable(&vtbl);
    oversized = mrb_integer_from_bytes(mrb, too_large_bytes, sizeof(too_large_bytes), 1);
    late_ok = invoke_cleanup_overflow_case(mrb, &vtbl, FALSE, oversized);
    early_ok = invoke_cleanup_overflow_case(mrb, &vtbl, TRUE, oversized);
    if (!late_ok || !early_ok) {
        mrb_raise(mrb, E_RUNTIME_ERROR,
                  "late or early invoke leaked a dispatch reference after integer RangeError");
    }
    return mrb_true_value();
#else
    (void)vtbl;
    (void)too_large_bytes;
    (void)oversized;
    (void)late_ok;
    (void)early_ok;
    return mrb_false_value();
#endif
}

static mrb_value
integer_invoke_cleanup_output_exercise(mrb_state *mrb, mrb_value self)
{
    IDispatchVtbl vtbl = { 0 };
    mrb_bool late_ok = FALSE;
    mrb_bool early_ok = FALSE;
    (void)self;

#ifdef MRB_USE_BIGINT
    (void)mrb;
    (void)vtbl;
    (void)late_ok;
    (void)early_ok;
    return mrb_false_value();
#else
    invoke_cleanup_init_vtable(&vtbl);
    late_ok = invoke_cleanup_output_case(mrb, &vtbl, FALSE);
    early_ok = invoke_cleanup_output_case(mrb, &vtbl, TRUE);
    if (!late_ok || !early_ok) {
        mrb_raise(mrb, E_RUNTIME_ERROR,
                  "late or early invoke leaked a dispatch result after output RangeError");
    }
    return mrb_true_value();
#endif
}

static mrb_value
integer_invoke_cleanup_bigint_enabled(mrb_state *mrb, mrb_value self)
{
    (void)mrb;
    (void)self;
#ifdef MRB_USE_BIGINT
    return mrb_true_value();
#else
    return mrb_false_value();
#endif
}

void
mrb_mruby_win32ole_integer_invoke_cleanup_test(mrb_state *mrb)
{
    mrb_define_method(mrb, mrb->object_class,
                      "__win32ole_integer_invoke_cleanup_bigint_exercise",
                      integer_invoke_cleanup_bigint_exercise, MRB_ARGS_NONE());
    mrb_define_method(mrb, mrb->object_class,
                      "__win32ole_integer_invoke_cleanup_output_exercise",
                      integer_invoke_cleanup_output_exercise, MRB_ARGS_NONE());
    mrb_define_method(mrb, mrb->object_class,
                      "__win32ole_integer_invoke_cleanup_bigint_enabled?",
                      integer_invoke_cleanup_bigint_enabled, MRB_ARGS_NONE());
}
