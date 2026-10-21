#include "../src/win32ole.h"
#include <mruby/error.h>
#include <stdlib.h>
#include <string.h>

struct bstr_cleanup_fake {
    void *lpVtbl;
    ULONG refs;
    UINT funcdesc_releases;
};

static TYPEATTR bstr_cleanup_typeattr;
static FUNCDESC bstr_cleanup_funcdesc;

static ULONG STDMETHODCALLTYPE bstr_cleanup_addref(ITypeInfo *self);
static ULONG STDMETHODCALLTYPE bstr_cleanup_release(ITypeInfo *self);
static HRESULT STDMETHODCALLTYPE bstr_cleanup_get_typeattr(ITypeInfo *self, TYPEATTR **typeattr);
static void STDMETHODCALLTYPE bstr_cleanup_release_typeattr(ITypeInfo *self, TYPEATTR *typeattr);
static HRESULT STDMETHODCALLTYPE bstr_cleanup_get_funcdesc(ITypeInfo *self, UINT index, FUNCDESC **funcdesc);
static void STDMETHODCALLTYPE bstr_cleanup_release_funcdesc(ITypeInfo *self, FUNCDESC *funcdesc);
static HRESULT STDMETHODCALLTYPE bstr_cleanup_get_documentation(ITypeInfo *self, MEMBERID memid, BSTR *name,
                                                                 BSTR *docstring, DWORD *helpcontext, BSTR *helpfile);
static HRESULT STDMETHODCALLTYPE bstr_cleanup_get_names(ITypeInfo *self, MEMBERID memid, BSTR *names,
                                                         UINT max_names, UINT *count);

static ITypeInfoVtbl bstr_cleanup_vtbl;

static ULONG STDMETHODCALLTYPE
bstr_cleanup_addref(ITypeInfo *self)
{
    return ++((struct bstr_cleanup_fake *)self)->refs;
}

static ULONG STDMETHODCALLTYPE
bstr_cleanup_release(ITypeInfo *self)
{
    struct bstr_cleanup_fake *fake = (struct bstr_cleanup_fake *)self;
    ULONG refs = --fake->refs;
    if (refs == 0) free(fake);
    return refs;
}

static HRESULT STDMETHODCALLTYPE
bstr_cleanup_get_typeattr(ITypeInfo *self, TYPEATTR **typeattr)
{
    *typeattr = &bstr_cleanup_typeattr;
    return S_OK;
}

static void STDMETHODCALLTYPE
bstr_cleanup_release_typeattr(ITypeInfo *self, TYPEATTR *typeattr)
{
}

static HRESULT STDMETHODCALLTYPE
bstr_cleanup_get_funcdesc(ITypeInfo *self, UINT index, FUNCDESC **funcdesc)
{
    if (index != 0) return TYPE_E_ELEMENTNOTFOUND;
    *funcdesc = &bstr_cleanup_funcdesc;
    return S_OK;
}

static void STDMETHODCALLTYPE
bstr_cleanup_release_funcdesc(ITypeInfo *self, FUNCDESC *funcdesc)
{
    ++((struct bstr_cleanup_fake *)self)->funcdesc_releases;
}

static HRESULT STDMETHODCALLTYPE
bstr_cleanup_get_documentation(ITypeInfo *self, MEMBERID memid, BSTR *name,
                               BSTR *docstring, DWORD *helpcontext,
                               BSTR *helpfile)
{
    if (memid != 17) return TYPE_E_ELEMENTNOTFOUND;
    if (name) *name = SysAllocString(L"SampleMethod");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
bstr_cleanup_get_names(ITypeInfo *self, MEMBERID memid, BSTR *names,
                       UINT max_names, UINT *count)
{
    if (memid != 17 || max_names < 2) return TYPE_E_ELEMENTNOTFOUND;
    names[0] = SysAllocString(L"SampleMethod");
    names[1] = SysAllocString(L"sampleArgument");
    *count = 2;
    return S_OK;
}

struct bstr_cleanup_initialize_call {
    mrb_value param;
    mrb_value method;
    mrb_int index;
};

static mrb_value
bstr_cleanup_initialize(mrb_state *mrb, void *userdata)
{
    struct bstr_cleanup_initialize_call *call = userdata;
    return mrb_funcall(mrb, call->param, "initialize", 2,
                       call->method, mrb_fixnum_value(call->index));
}

static mrb_value
bstr_cleanup_exercise(mrb_state *mrb, mrb_value self)
{
    struct bstr_cleanup_fake *fake;
    mrb_value method;
    mrb_value param;
    mrb_value name;
    struct bstr_cleanup_initialize_call call;
    mrb_bool error = FALSE;
    UINT releases;

    fake = calloc(1, sizeof(*fake));
    if (!fake) mrb_raise(mrb, E_RUNTIME_ERROR, "could not allocate fake ITypeInfo");
    memset(&bstr_cleanup_vtbl, 0, sizeof(bstr_cleanup_vtbl));
    bstr_cleanup_vtbl.AddRef = bstr_cleanup_addref;
    bstr_cleanup_vtbl.Release = bstr_cleanup_release;
    bstr_cleanup_vtbl.GetTypeAttr = bstr_cleanup_get_typeattr;
    bstr_cleanup_vtbl.ReleaseTypeAttr = bstr_cleanup_release_typeattr;
    bstr_cleanup_vtbl.GetFuncDesc = bstr_cleanup_get_funcdesc;
    bstr_cleanup_vtbl.ReleaseFuncDesc = bstr_cleanup_release_funcdesc;
    bstr_cleanup_vtbl.GetDocumentation = bstr_cleanup_get_documentation;
    bstr_cleanup_vtbl.GetNames = bstr_cleanup_get_names;
    memset(&bstr_cleanup_typeattr, 0, sizeof(bstr_cleanup_typeattr));
    memset(&bstr_cleanup_funcdesc, 0, sizeof(bstr_cleanup_funcdesc));
    fake->lpVtbl = &bstr_cleanup_vtbl;
    fake->refs = 1; /* helper ownership; wrappers retain their own references */
    bstr_cleanup_typeattr.cFuncs = 1;
    bstr_cleanup_funcdesc.memid = 17;
    bstr_cleanup_funcdesc.cParams = 1;

    method = create_win32ole_method(mrb, (ITypeInfo *)fake,
                                    mrb_str_new_lit(mrb, "SampleMethod"));
    param = create_win32ole_param(mrb, (ITypeInfo *)fake, 0, 0,
                                  mrb_str_new_lit(mrb, "initial"));
    call.param = param;
    call.method = method;
    call.index = 1;
    releases = fake->funcdesc_releases;
    mrb_protect_error(mrb, bstr_cleanup_initialize, &call, &error);
    if (error) {
        bstr_cleanup_release((ITypeInfo *)fake);
        mrb_raise(mrb, E_RUNTIME_ERROR, "valid parameter initialization raised");
    }
    name = mrb_funcall(mrb, param, "name", 0);
    if (!mrb_string_p(name) || RSTRING_LEN(name) != 14 ||
        memcmp(RSTRING_PTR(name), "sampleArgument", 14) != 0) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "parameter name changed");
    }
    if (fake->funcdesc_releases != releases + 1) {
        bstr_cleanup_release((ITypeInfo *)fake);
        mrb_raise(mrb, E_RUNTIME_ERROR, "successful parameter initialization did not release FUNCDESC");
    }

    call.index = 2;
    releases = fake->funcdesc_releases;
    error = FALSE;
    mrb_protect_error(mrb, bstr_cleanup_initialize, &call, &error);
    if (!error || fake->funcdesc_releases != releases + 1) {
        bstr_cleanup_release((ITypeInfo *)fake);
        mrb_raise(mrb, E_RUNTIME_ERROR, "invalid parameter index did not release FUNCDESC");
    }

    param = create_win32ole_param(mrb, (ITypeInfo *)fake, 0, 0,
                                  mrb_str_new_lit(mrb, "initial"));
    mrb_funcall(mrb, param, "freeze", 0);
    call.param = param;
    call.index = 1;
    releases = fake->funcdesc_releases;
    error = FALSE;
    mrb_protect_error(mrb, bstr_cleanup_initialize, &call, &error);
    if (!error || fake->funcdesc_releases != releases + 1) {
        bstr_cleanup_release((ITypeInfo *)fake);
        mrb_raise(mrb, E_RUNTIME_ERROR, "exception during parameter initialization did not release FUNCDESC");
    }
    bstr_cleanup_release((ITypeInfo *)fake);
    return mrb_true_value();
}

void
mrb_mruby_win32ole_bstr_cleanup_test(mrb_state *mrb)
{
    mrb_define_method(mrb, mrb->object_class, "__win32ole_bstr_cleanup_exercise",
                      bstr_cleanup_exercise, MRB_ARGS_NONE());
}
