#include "../src/win32ole.h"
#include <mruby/error.h>
#include <string.h>

struct early_invoke_fake {
    void *lpVtbl;
    ULONG refs;
};

struct early_invoke_call {
    mrb_value receiver;
};

static ULONG STDMETHODCALLTYPE
early_invoke_addref(IDispatch *self)
{
    return ++((struct early_invoke_fake *)self)->refs;
}

static ULONG STDMETHODCALLTYPE
early_invoke_release(IDispatch *self)
{
    return --((struct early_invoke_fake *)self)->refs;
}

static mrb_value
call_early_invoke(mrb_state *mrb, void *data)
{
    struct early_invoke_call *call = data;
    mrb_value args = mrb_ary_new_capa(mrb, 2);
    mrb_value types = mrb_ary_new_capa(mrb, 2);

    mrb_ary_push(mrb, args, mrb_str_new_lit(mrb, "not an integer"));
    mrb_ary_push(mrb, args, call->receiver);
    mrb_ary_push(mrb, types, INT2FIX(VT_I4));
    mrb_ary_push(mrb, types, INT2FIX(VT_VARIANT));
    return mrb_funcall(mrb, call->receiver, "_invoke", 3,
                       INT2FIX(1), args, types);
}

static mrb_value
early_invoke_cleanup_exercise(mrb_state *mrb, mrb_value self)
{
    struct early_invoke_fake fake;
    struct early_invoke_call call;
    IDispatchVtbl dispatch_vtbl;
    VARIANT var;
    mrb_value receiver;
    mrb_bool error = FALSE;
    mrb_value exception;
    mrb_value message;
    const char *message_ptr;

    fake.refs = 1;
    memset(&dispatch_vtbl, 0, sizeof(dispatch_vtbl));
    dispatch_vtbl.AddRef = early_invoke_addref;
    dispatch_vtbl.Release = early_invoke_release;
    fake.lpVtbl = &dispatch_vtbl;

    VariantInit(&var);
    V_VT(&var) = VT_DISPATCH;
    V_DISPATCH(&var) = (IDispatch *)&fake;
    receiver = ole_variant2val(mrb, &var);
    VariantClear(&var);
    call.receiver = receiver;

    exception = mrb_protect_error(mrb, call_early_invoke, &call, &error);
    if (!error || !mrb_obj_is_kind_of(mrb, exception, mrb_class_get(mrb, "TypeError"))) {
        mrb_funcall(mrb, receiver, "ole_free", 0);
        mrb_raise(mrb, E_RUNTIME_ERROR, "argument conversion did not raise TypeError");
    }
    message = mrb_obj_as_string(mrb, exception);
    message_ptr = mrb_string_value_cstr(mrb, &message);
    if (strstr(message_ptr, "cannot be converted") == NULL) {
        mrb_funcall(mrb, receiver, "ole_free", 0);
        mrb_raise(mrb, E_RUNTIME_ERROR, "argument conversion TypeError detail changed");
    }
    if (fake.refs != 1) {
        mrb_funcall(mrb, receiver, "ole_free", 0);
        mrb_raise(mrb, E_RUNTIME_ERROR, "failed argument conversion leaked a dispatch reference");
    }
    mrb_funcall(mrb, receiver, "ole_free", 0);
    if (fake.refs != 0) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "ole_free did not release the remaining dispatch reference");
    }
    return mrb_true_value();
}

void
mrb_mruby_win32ole_early_invoke_test(mrb_state *mrb)
{
    mrb_define_method(mrb, mrb->object_class, "__win32ole_early_invoke_cleanup_exercise",
                      early_invoke_cleanup_exercise, MRB_ARGS_NONE());
}
