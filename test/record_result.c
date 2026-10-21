#include "../src/win32ole.h"
#include "../src/win32ole_record.h"
#include <string.h>

struct fake_recordinfo {
    IRecordInfo iface;
    ULONG refs;
};

struct fake_record_stats {
    ULONG copies;
    ULONG destroys;
    ULONG clears;
    ULONG addrefs;
    ULONG releases;
};

static IRecordInfoVtbl fake_recordinfo_vtbl;
static struct fake_record_stats fake_record_stats;
static BOOL fake_recordinfo_fail_copy;

static HRESULT STDMETHODCALLTYPE
fake_recordinfo_queryinterface(IRecordInfo *self, REFIID riid, void **object)
{
    if (!object) return E_POINTER;
    *object = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IRecordInfo)) {
        *object = self;
        ((struct fake_recordinfo *)self)->refs++;
        fake_record_stats.addrefs++;
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE
fake_recordinfo_addref(IRecordInfo *self)
{
    struct fake_recordinfo *recordinfo = (struct fake_recordinfo *)self;
    fake_record_stats.addrefs++;
    return ++recordinfo->refs;
}

static ULONG STDMETHODCALLTYPE
fake_recordinfo_release(IRecordInfo *self)
{
    struct fake_recordinfo *recordinfo = (struct fake_recordinfo *)self;
    ULONG refs = --recordinfo->refs;
    fake_record_stats.releases++;
    if (refs == 0) CoTaskMemFree(recordinfo);
    return refs;
}

static HRESULT STDMETHODCALLTYPE
fake_recordinfo_getname(IRecordInfo *self, BSTR *name)
{
    (void)self;
    *name = SysAllocString(L"CallbackRecord");
    return *name ? S_OK : E_OUTOFMEMORY;
}

static HRESULT STDMETHODCALLTYPE
fake_recordinfo_getfieldnames(IRecordInfo *self, ULONG *count, BSTR *names)
{
    (void)self;
    (void)names;
    *count = 0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
fake_recordinfo_getsize(IRecordInfo *self, ULONG *size)
{
    (void)self;
    *size = sizeof(LONG);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
fake_recordinfo_recordinit(IRecordInfo *self, PVOID data)
{
    (void)self;
    *(LONG *)data = 123;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
fake_recordinfo_recordclear(IRecordInfo *self, PVOID data)
{
    (void)self;
    *(LONG *)data = 0;
    fake_record_stats.clears++;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
fake_recordinfo_recordcreatecopy(IRecordInfo *self, PVOID source, PVOID *copy)
{
    LONG *record_copy;
    (void)self;
    record_copy = CoTaskMemAlloc(sizeof(LONG));
    if (!record_copy) return E_OUTOFMEMORY;
    *record_copy = *(LONG *)source;
    *copy = record_copy;
    fake_record_stats.copies++;
    if (fake_recordinfo_fail_copy) {
        fake_recordinfo_fail_copy = FALSE;
        return E_FAIL;
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
fake_recordinfo_recorddestroy(IRecordInfo *self, PVOID data)
{
    (void)self;
    CoTaskMemFree(data);
    fake_record_stats.destroys++;
    return S_OK;
}

static mrb_value
fake_recordinfo_new_record(mrb_state *mrb)
{
    struct fake_recordinfo *recordinfo;
    mrb_value record;
    recordinfo = CoTaskMemAlloc(sizeof(*recordinfo));
    if (!recordinfo) mrb_raise(mrb, E_RUNTIME_ERROR, "fake IRecordInfo allocation failed");
    memset(recordinfo, 0, sizeof(*recordinfo));
    recordinfo->iface.lpVtbl = &fake_recordinfo_vtbl;
    recordinfo->refs = 1;
    record = create_win32ole_record(mrb, &recordinfo->iface, NULL);
    mrb_iv_set(mrb, record, mrb_intern_lit(mrb, "fields"), mrb_hash_new(mrb));
    fake_recordinfo_release(&recordinfo->iface);
    return record;
}

static mrb_value
fake_recordinfo_raises(mrb_state *mrb, void *data)
{
    return mrb_funcall(mrb, *(mrb_value *)data, "initialize", 2,
                       mrb_nil_value(), mrb_nil_value());
}

struct fake_record_copy_call {
    mrb_value record;
    VARIANT *result;
};

static mrb_value
fake_recordinfo_copy_failure(mrb_state *mrb, void *data)
{
    struct fake_record_copy_call *call = data;
    ole_val2variant_result(mrb, call->record, call->result);
    return mrb_nil_value();
}

static mrb_value
fake_recordinfo_result_exercise(mrb_state *mrb, mrb_value self)
{
    mrb_value record;
    mrb_value wrapper;
    VARIANT first, second;
    VARIANT failed_copy;
    mrb_bool error = FALSE;
    mrb_value error_value;
    struct fake_record_copy_call failed_call;
    (void)self;

    memset(&fake_record_stats, 0, sizeof(fake_record_stats));
    memset(&fake_recordinfo_vtbl, 0, sizeof(fake_recordinfo_vtbl));
    fake_recordinfo_vtbl.AddRef = fake_recordinfo_addref;
    fake_recordinfo_vtbl.Release = fake_recordinfo_release;
    fake_recordinfo_vtbl.QueryInterface = fake_recordinfo_queryinterface;
    fake_recordinfo_vtbl.GetName = fake_recordinfo_getname;
    fake_recordinfo_vtbl.GetSize = fake_recordinfo_getsize;
    fake_recordinfo_vtbl.GetFieldNames = fake_recordinfo_getfieldnames;
    fake_recordinfo_vtbl.RecordInit = fake_recordinfo_recordinit;
    fake_recordinfo_vtbl.RecordClear = fake_recordinfo_recordclear;
    fake_recordinfo_vtbl.RecordCreateCopy = fake_recordinfo_recordcreatecopy;
    fake_recordinfo_vtbl.RecordDestroy = fake_recordinfo_recorddestroy;
    record = fake_recordinfo_new_record(mrb);
    VariantInit(&first);
    VariantInit(&second);
    ole_rec2variant(mrb, record, &first);
    ole_val2variant_result(mrb, record, &second);
    if (V_VT(&first) != VT_RECORD || V_VT(&second) != VT_RECORD ||
        V_RECORD(&first) == V_RECORD(&second) ||
        *(LONG *)V_RECORD(&first) != 123 || *(LONG *)V_RECORD(&second) != 123 ||
        fake_record_stats.copies != 2 || fake_record_stats.addrefs != 3) {
        VariantClear(&first);
        VariantClear(&second);
        mrb_raise(mrb, E_RUNTIME_ERROR, "VT_RECORD result copies or references were not independent");
    }

    wrapper = ole_variant2val(mrb, &first);
    mrb_full_gc(mrb);
    error_value = mrb_protect_error(mrb, fake_recordinfo_raises, &wrapper, &error);
    (void)error_value;
    if (!error || V_VT(&first) != VT_RECORD || *(LONG *)V_RECORD(&first) != 123 ||
        *(LONG *)V_RECORD(&second) != 123 || fake_record_stats.releases != 2) {
        VariantClear(&first);
        VariantClear(&second);
        mrb_raise(mrb, E_RUNTIME_ERROR, "failed record initialize invalidated a live VT_RECORD result");
    }

    VariantClear(&first);
    if (fake_record_stats.clears != 1 || fake_record_stats.releases != 3 ||
        *(LONG *)V_RECORD(&second) != 123) {
        VariantClear(&second);
        mrb_raise(mrb, E_RUNTIME_ERROR, "clearing one VT_RECORD result damaged the other copy");
    }
    VariantClear(&second);
    if (fake_record_stats.clears != 2 || fake_record_stats.releases != 4) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "VT_RECORD result cleanup did not release each copy");
    }

    VariantInit(&failed_copy);
    failed_call.record = record;
    failed_call.result = &failed_copy;
    fake_recordinfo_fail_copy = TRUE;
    error_value = mrb_protect_error(mrb, fake_recordinfo_copy_failure, &failed_call, &error);
    (void)error_value;
    if (!error || V_VT(&failed_copy) != VT_EMPTY || fake_record_stats.destroys != 1 ||
        fake_recordinfo_fail_copy) {
        VariantClear(&failed_copy);
        mrb_raise(mrb, E_RUNTIME_ERROR, "failed VT_RECORD copy was not destroyed exactly once");
    }
    VariantClear(&failed_copy);
    return mrb_true_value();
}

void
mrb_mruby_win32ole_record_result_test(mrb_state *mrb)
{
    memset(&fake_record_stats, 0, sizeof(fake_record_stats));
    memset(&fake_recordinfo_vtbl, 0, sizeof(fake_recordinfo_vtbl));
    fake_recordinfo_vtbl.AddRef = fake_recordinfo_addref;
    fake_recordinfo_vtbl.Release = fake_recordinfo_release;
    fake_recordinfo_vtbl.QueryInterface = fake_recordinfo_queryinterface;
    fake_recordinfo_vtbl.GetName = fake_recordinfo_getname;
    fake_recordinfo_vtbl.GetSize = fake_recordinfo_getsize;
    fake_recordinfo_vtbl.GetFieldNames = fake_recordinfo_getfieldnames;
    fake_recordinfo_vtbl.RecordInit = fake_recordinfo_recordinit;
    fake_recordinfo_vtbl.RecordClear = fake_recordinfo_recordclear;
    fake_recordinfo_vtbl.RecordCreateCopy = fake_recordinfo_recordcreatecopy;
    fake_recordinfo_vtbl.RecordDestroy = fake_recordinfo_recorddestroy;
    fake_recordinfo_fail_copy = FALSE;
    mrb_define_method(mrb, mrb->object_class, "__win32ole_record_result_exercise",
                      fake_recordinfo_result_exercise, MRB_ARGS_NONE());
}
