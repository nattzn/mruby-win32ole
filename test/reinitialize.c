#include "../src/win32ole.h"
#include <mruby/error.h>
#include <string.h>

struct fake_com {
    void *lpVtbl;
    ULONG refs;
};

struct reinitialize_call {
    mrb_value object;
    const char *kind;
};

static ULONG STDMETHODCALLTYPE fake_dispatch_addref(IDispatch *self)
{
    return ++((struct fake_com *)self)->refs;
}

static ULONG STDMETHODCALLTYPE fake_dispatch_release(IDispatch *self)
{
    return --((struct fake_com *)self)->refs;
}

static ULONG STDMETHODCALLTYPE fake_typeinfo_addref(ITypeInfo *self)
{
    return ++((struct fake_com *)self)->refs;
}

static ULONG STDMETHODCALLTYPE fake_typeinfo_release(ITypeInfo *self)
{
    return --((struct fake_com *)self)->refs;
}

static ULONG STDMETHODCALLTYPE fake_typelib_addref(ITypeLib *self)
{
    return ++((struct fake_com *)self)->refs;
}

static ULONG STDMETHODCALLTYPE fake_typelib_release(ITypeLib *self)
{
    return --((struct fake_com *)self)->refs;
}

static mrb_value
reinitialize_without_arguments(mrb_state *mrb, void *data)
{
    struct reinitialize_call *call = data;
    if (strcmp(call->kind, "WIN32OLE_TYPE") == 0) {
        return mrb_funcall(mrb, call->object, "initialize", 2,
                           mrb_nil_value(), mrb_nil_value());
    }
    if (strcmp(call->kind, "WIN32OLE_PARAM") == 0) {
        return mrb_funcall(mrb, call->object, "initialize", 2,
                           mrb_nil_value(), INT2FIX(0));
    }
    return mrb_funcall(mrb, call->object, "initialize", 0);
}

static mrb_bool
reinitialize_raises(mrb_state *mrb, mrb_value object, const char *kind)
{
    struct reinitialize_call call;
    mrb_bool error = FALSE;
    call.object = object;
    call.kind = kind;
    mrb_protect_error(mrb, reinitialize_without_arguments, &call, &error);
    return error;
}

static mrb_value
reinitialize_event_registry(mrb_state *mrb, mrb_value self)
{
    return mrb_obj_iv_get(mrb, (struct RObject *)mrb->object_class,
                          mrb_intern_lit(mrb, "_win32ole_ary_ole_event"));
}

static mrb_value
reinitialize_release_exercise(mrb_state *mrb, mrb_value self)
{
    const char *kind;
    struct fake_com fake;
    mrb_value object;
    VARIANT var;
    ULONG expected_refs;
    IDispatchVtbl dispatch_vtbl;
    ITypeInfoVtbl typeinfo_vtbl;
    ITypeLibVtbl typelib_vtbl;

    mrb_get_args(mrb, "z", &kind);
    fake.refs = 1;
    if (strcmp(kind, "WIN32OLE") == 0) {
        memset(&dispatch_vtbl, 0, sizeof(dispatch_vtbl));
        dispatch_vtbl.AddRef = fake_dispatch_addref;
        dispatch_vtbl.Release = fake_dispatch_release;
        fake.lpVtbl = &dispatch_vtbl;
        VariantInit(&var);
        V_VT(&var) = VT_DISPATCH;
        V_DISPATCH(&var) = (IDispatch *)&fake;
        object = ole_variant2val(mrb, &var);
        VariantClear(&var);
    }
    else if (strcmp(kind, "WIN32OLE_TYPE") == 0) {
        memset(&typeinfo_vtbl, 0, sizeof(typeinfo_vtbl));
        typeinfo_vtbl.AddRef = fake_typeinfo_addref;
        typeinfo_vtbl.Release = fake_typeinfo_release;
        fake.lpVtbl = &typeinfo_vtbl;
        object = create_win32ole_type(mrb, (ITypeInfo *)&fake, mrb_str_new_lit(mrb, "fake"));
    }
    else if (strcmp(kind, "WIN32OLE_PARAM") == 0) {
        memset(&typeinfo_vtbl, 0, sizeof(typeinfo_vtbl));
        typeinfo_vtbl.AddRef = fake_typeinfo_addref;
        typeinfo_vtbl.Release = fake_typeinfo_release;
        fake.lpVtbl = &typeinfo_vtbl;
        object = create_win32ole_param(mrb, (ITypeInfo *)&fake, 0, 0, mrb_str_new_lit(mrb, "fake"));
    }
    else if (strcmp(kind, "WIN32OLE_TYPELIB") == 0) {
        memset(&typelib_vtbl, 0, sizeof(typelib_vtbl));
        typelib_vtbl.AddRef = fake_typelib_addref;
        typelib_vtbl.Release = fake_typelib_release;
        fake.lpVtbl = &typelib_vtbl;
        object = create_win32ole_typelib(mrb, (ITypeLib *)&fake);
    }
    else {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "unknown WIN32OLE class");
    }
    expected_refs = (strcmp(kind, "WIN32OLE_TYPE") == 0 ||
                     strcmp(kind, "WIN32OLE_PARAM") == 0) ? 1 : 0;

    if (!reinitialize_raises(mrb, object, kind)) {
        DATA_PTR(object) = NULL;
        mrb_raise(mrb, E_RUNTIME_ERROR, "initialize without arguments did not raise");
    }
    if (fake.refs != expected_refs || DATA_PTR(object) != NULL) {
        DATA_PTR(object) = NULL;
        mrb_raise(mrb, E_RUNTIME_ERROR, "reinitialize did not clear the old COM reference exactly once");
    }
    if (!reinitialize_raises(mrb, object, kind) || fake.refs != expected_refs || DATA_PTR(object) != NULL) {
        DATA_PTR(object) = NULL;
        mrb_raise(mrb, E_RUNTIME_ERROR, "repeated failed initialize released a COM reference again");
    }
    DATA_PTR(object) = NULL;
    return mrb_true_value();
}

void
mrb_mruby_win32ole_reinitialize_test(mrb_state *mrb)
{
    mrb_define_method(mrb, mrb->object_class, "__win32ole_reinitialize_release_exercise",
                      reinitialize_release_exercise, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, mrb->object_class, "__win32ole_event_registry",
                      reinitialize_event_registry, MRB_ARGS_NONE());
}
