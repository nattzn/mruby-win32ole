#include "../src/win32ole.h"
#include <mruby/gc.h>
#include <string.h>

void mrb_mruby_win32ole_reinitialize_test(mrb_state *mrb);
void mrb_mruby_win32ole_early_invoke_test(mrb_state *mrb);
void mrb_mruby_win32ole_bstr_cleanup_test(mrb_state *mrb);
void mrb_mruby_win32ole_object_state_test(mrb_state *mrb);
void mrb_mruby_win32ole_record_result_test(mrb_state *mrb);

static const WCHAR result_text[] = L"callback result";

/* Checks that callback results own their BSTR data after the Ruby side ran a
   full GC. */
static BOOL
owned_result_alive(VARIANT *result, BOOL in_array)
{
    VARIANT *elem = result;
    BOOL ok;

    if (in_array) {
        SAFEARRAY *psa;
        if (V_VT(result) != (VT_ARRAY | VT_VARIANT)) return FALSE;
        psa = V_ARRAY(result);
        if (!psa || SafeArrayGetDim(psa) != 1 || psa->rgsabound[0].cElements != 1) return FALSE;
        if (FAILED(SafeArrayAccessData(psa, (void **)&elem))) return FALSE;
    }
    ok = V_VT(elem) == VT_BSTR && V_BSTR(elem) &&
         wcscmp(V_BSTR(elem), result_text) == 0;
    if (in_array) SafeArrayUnaccessData(V_ARRAY(result));
    return ok;
}

static BOOL
owned_nested_result_alive(VARIANT *result)
{
    SAFEARRAY *outer, *inner;
    VARIANT *outer_elem, *inner_elem;
    BOOL ok = FALSE;

    if (V_VT(result) != (VT_ARRAY | VT_VARIANT)) return FALSE;
    outer = V_ARRAY(result);
    if (!outer || SafeArrayGetDim(outer) != 1 || outer->rgsabound[0].cElements != 1 ||
        FAILED(SafeArrayAccessData(outer, (void **)&outer_elem))) return FALSE;
    if (V_VT(outer_elem) == (VT_ARRAY | VT_VARIANT)) {
        inner = V_ARRAY(outer_elem);
        if (inner && SafeArrayGetDim(inner) == 1 && inner->rgsabound[0].cElements == 1 &&
            SUCCEEDED(SafeArrayAccessData(inner, (void **)&inner_elem))) {
            ok = V_VT(inner_elem) == VT_BSTR && V_BSTR(inner_elem) &&
                 wcscmp(V_BSTR(inner_elem), result_text) == 0;
            SafeArrayUnaccessData(inner);
        }
    }
    SafeArrayUnaccessData(outer);
    return ok;
}

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
    BOOL expect_byref_array;
    BOOL expect_nested_array;
    BOOL mutate_source;

    mrb_get_args(mrb, "zi", &method, &iterations);
    expect_exception = strcmp(method, "raise_error") == 0;
    expect_string = strcmp(method, "string_result") == 0;
    expect_byref = strcmp(method, "byref_result") == 0;
    expect_byref_array = strcmp(method, "byref_array_result") == 0;
    expect_nested_array = strcmp(method, "nested_byref_array_result") == 0;
    mutate_source = expect_byref || expect_byref_array || expect_nested_array;
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
        else if ((expect_byref_array || expect_nested_array) ? V_VT(&result) != (VT_ARRAY | VT_VARIANT) :
                 expect_byref ? V_VT(&result) != VT_BSTR :
                 (expect_string ? V_VT(&result) != VT_BSTR : V_VT(&result) != VT_I4)) {
            VariantClear(&result);
            SysFreeString(name);
            VariantClear(&target);
            mrb_raise(mrb, E_RUNTIME_ERROR, "Invoke returned an unexpected VARIANT type");
        }
        if (mrb_gc_arena_save(mrb) != arena) {
            VariantClear(&result);
            SysFreeString(name);
            VariantClear(&target);
            mrb_raise(mrb, E_RUNTIME_ERROR, "Invoke changed the GC arena index");
        }

        if (mutate_source) {
            mrb_funcall(mrb, self, "mutate_result_source", 0);
        }
        if (expect_byref_array || expect_nested_array) {
            mrb_full_gc(mrb);
            if (expect_nested_array ? !owned_nested_result_alive(&result) :
                !owned_result_alive(&result, TRUE)) {
                VariantClear(&result);
                SysFreeString(name);
                VariantClear(&target);
                mrb_raise(mrb, E_RUNTIME_ERROR, "owned array element changed after GC");
            }
        }
        else if (expect_byref) {
            mrb_full_gc(mrb);
            if (!owned_result_alive(&result, FALSE)) {
                VariantClear(&result);
                SysFreeString(name);
                VariantClear(&target);
                mrb_raise(mrb, E_RUNTIME_ERROR, "owned BYREF result changed after GC");
            }
        }
        if (!expect_exception && !expect_byref_array && !expect_nested_array) {
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

/* Returns an AddRef'ed IDispatch of the first sink advised on the source
   that knows the given event name, or NULL. */
static IDispatch *
find_event_sink(IDispatch *source, LPOLESTR event_name, DISPID *pdispid)
{
    IConnectionPointContainer *pcpc = NULL;
    IEnumConnectionPoints *pecp = NULL;
    IConnectionPoint *pcp = NULL;
    IDispatch *found = NULL;

    if (FAILED(source->lpVtbl->QueryInterface(source, &IID_IConnectionPointContainer, (void **)&pcpc)))
        return NULL;
    if (FAILED(pcpc->lpVtbl->EnumConnectionPoints(pcpc, &pecp))) {
        pcpc->lpVtbl->Release(pcpc);
        return NULL;
    }
    while (!found && pecp->lpVtbl->Next(pecp, 1, &pcp, NULL) == S_OK) {
        IEnumConnections *pec = NULL;
        CONNECTDATA cd;
        if (SUCCEEDED(pcp->lpVtbl->EnumConnections(pcp, &pec))) {
            while (!found && pec->lpVtbl->Next(pec, 1, &cd, NULL) == S_OK) {
                IDispatch *pdisp = NULL;
                if (SUCCEEDED(cd.pUnk->lpVtbl->QueryInterface(cd.pUnk, &IID_IDispatch, (void **)&pdisp))) {
                    if (SUCCEEDED(pdisp->lpVtbl->GetIDsOfNames(pdisp, &IID_NULL, &event_name, 1,
                                                               LOCALE_USER_DEFAULT, pdispid))) {
                        found = pdisp;
                    }
                    else {
                        pdisp->lpVtbl->Release(pdisp);
                    }
                }
                cd.pUnk->lpVtbl->Release(cd.pUnk);
            }
            pec->lpVtbl->Release(pec);
        }
        pcp->lpVtbl->Release(pcp);
    }
    pecp->lpVtbl->Release(pecp);
    pcpc->lpVtbl->Release(pcpc);
    return found;
}

/* Invokes the advised WIN32OLE_EVENT sink directly with a result VARIANT and
   checks that an owned result survives a full GC after the callback. */
static mrb_value
event_result_arena_exercise(mrb_state *mrb, mrb_value self)
{
    mrb_value source;
    mrb_bool in_array;
    mrb_int iterations;
    mrb_int i;
    VARIANT target;
    IDispatch *sink;
    DISPID dispid;
    DISPPARAMS params;
    WCHAR event_name[] = L"WillConnect";
    int arena;
    const char *failure = NULL;

    mrb_get_args(mrb, "obi", &source, &in_array, &iterations);
    if (iterations < 1) mrb_raise(mrb, E_ARGUMENT_ERROR, "iterations must be positive");

    VariantInit(&target);
    ole_val2variant(mrb, source, &target);
    if (V_VT(&target) != VT_DISPATCH || V_DISPATCH(&target) == NULL) {
        VariantClear(&target);
        mrb_raise(mrb, E_RUNTIME_ERROR, "event source did not convert to VT_DISPATCH");
    }
    sink = find_event_sink(V_DISPATCH(&target), event_name, &dispid);
    VariantClear(&target);
    if (!sink) mrb_raise(mrb, E_RUNTIME_ERROR, "advised event sink was not found");

    memset(&params, 0, sizeof(params));
    arena = mrb_gc_arena_save(mrb);
    for (i = 0; i < iterations && !failure; i++) {
        VARIANT result;
        HRESULT hr;

        VariantInit(&result);
        hr = sink->lpVtbl->Invoke(sink, dispid, &IID_NULL, LOCALE_USER_DEFAULT,
                                  DISPATCH_METHOD, &params, &result, NULL, NULL);
        if (FAILED(hr)) {
            failure = "event Invoke failed";
        }
        else if (mrb_gc_arena_save(mrb) != arena) {
            failure = "event Invoke changed the GC arena index";
        }
        else {
            mrb_full_gc(mrb);
            if (!owned_result_alive(&result, in_array)) {
                failure = "owned event result changed after GC";
            }
        }
        VariantClear(&result);
        mrb_gc_arena_restore(mrb, arena);
    }
    sink->lpVtbl->Release(sink);
    if (failure) mrb_raise(mrb, E_RUNTIME_ERROR, failure);
    return mrb_true_value();
}

void
mrb_mruby_win32ole_gem_test(mrb_state *mrb)
{
    mrb_define_method(mrb, mrb->object_class, "__win32ole_callback_arena_exercise",
                      callback_arena_exercise, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, mrb->object_class, "__win32ole_event_result_arena_exercise",
                      event_result_arena_exercise, MRB_ARGS_REQ(3));
    mrb_mruby_win32ole_reinitialize_test(mrb);
    mrb_mruby_win32ole_early_invoke_test(mrb);
    mrb_mruby_win32ole_bstr_cleanup_test(mrb);
    mrb_mruby_win32ole_object_state_test(mrb);
    mrb_mruby_win32ole_record_result_test(mrb);
}
