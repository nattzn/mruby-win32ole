#include "../src/win32ole.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

struct integer_variant_call {
    mrb_value value;
    VARIANT *variant;
    VARTYPE requested_type;
};

static mrb_value
integer_variant_convert(mrb_state *mrb, void *data)
{
    struct integer_variant_call *call = data;
    int64_t signed_value;
    uint64_t unsigned_value;
    double float_value;

    if (call->requested_type == VT_EMPTY) {
        ole_val2variant(mrb, call->value, call->variant);
    }
    else {
        ole_val2variant_ex(mrb, call->value, call->variant, call->requested_type);
    }

    if (V_VT(call->variant) == VT_I8) {
        if (mrb_nil_p(call->value)) {
            if (V_I8(call->variant) != 0) {
                mrb_raise(mrb, E_RUNTIME_ERROR, "nil did not convert to zero for VT_I8");
            }
        }
        else if (mrb_float_p(call->value)) {
            float_value = (double)mrb_float(call->value);
            if (float_value == float_value &&
                float_value >= (double)INT64_MIN && float_value < 9223372036854775808.0) {
                signed_value = (int64_t)trunc(float_value);
                if (V_I8(call->variant) != signed_value) {
                    mrb_raise(mrb, E_RUNTIME_ERROR, "VT_I8 did not preserve the truncated value");
                }
            }
        }
        else {
            signed_value = mrb_as_int64(mrb, call->value);
            if (V_I8(call->variant) != signed_value) {
                mrb_raise(mrb, E_RUNTIME_ERROR, "VT_I8 did not preserve the integer value");
            }
        }
    }
    else if (V_VT(call->variant) == VT_UI8) {
        if (mrb_nil_p(call->value)) {
            if (V_UI8(call->variant) != 0) {
                mrb_raise(mrb, E_RUNTIME_ERROR, "nil did not convert to zero for VT_UI8");
            }
        }
        else {
            unsigned_value = mrb_as_uint64(mrb, call->value);
            if (V_UI8(call->variant) != unsigned_value) {
                mrb_raise(mrb, E_RUNTIME_ERROR, "VT_UI8 did not preserve the integer value");
            }
        }
    }
    else if (V_VT(call->variant) == VT_I4) {
        if (mrb_nil_p(call->value)) {
            if (V_I4(call->variant) != 0) {
                mrb_raise(mrb, E_RUNTIME_ERROR, "nil did not convert to zero for VT_I4");
            }
        }
        else {
            signed_value = mrb_as_int64(mrb, call->value);
            if (V_I4(call->variant) != (LONG)signed_value) {
                mrb_raise(mrb, E_RUNTIME_ERROR, "VT_I4 did not preserve the integer value");
            }
        }
    }
    return ole_variant2val(mrb, call->variant);
}

static mrb_value
integer_to_variant(mrb_state *mrb, mrb_value self)
{
    mrb_value value;
    const char *kind;
    struct integer_variant_call call;
    VARIANT variant;
    mrb_bool error = FALSE;
    mrb_value result;
    mrb_value pair;

    (void)self;
    mrb_get_args(mrb, "oz", &value, &kind);
    VariantInit(&variant);
    call.value = value;
    call.variant = &variant;
    if (strcmp(kind, "auto") == 0) {
        call.requested_type = VT_EMPTY;
    }
    else if (strcmp(kind, "i8") == 0) {
        call.requested_type = VT_I8;
    }
    else if (strcmp(kind, "ui8") == 0) {
        call.requested_type = VT_UI8;
    }
    else {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "unknown integer VARIANT type");
    }

    result = mrb_protect_error(mrb, integer_variant_convert, &call, &error);
    if (error) {
        VariantClear(&variant);
        mrb_exc_raise(mrb, result);
    }
    pair = mrb_ary_new_capa(mrb, 2);
    mrb_ary_push(mrb, pair, INT2FIX(V_VT(&variant)));
    mrb_ary_push(mrb, pair, result);
    VariantClear(&variant);
    return pair;
}

static mrb_value
integer_fixture(mrb_state *mrb, mrb_value self)
{
    const char *name;
    static const uint8_t uint64_overflow[] = { 1, 0, 0, 0, 0, 0, 0, 0, 0 };
    static const uint8_t int64_underflow_magnitude[] = { 0x80, 0, 0, 0, 0, 0, 0, 1 };

    (void)self;
    mrb_get_args(mrb, "z", &name);
    if (strcmp(name, "positive_2p53_minus1") == 0) return mrb_int64_value(mrb, INT64_C(9007199254740991));
    if (strcmp(name, "positive_2p53_plus1") == 0) return mrb_int64_value(mrb, INT64_C(9007199254740993));
    if (strcmp(name, "negative_2p53_minus1") == 0) return mrb_int64_value(mrb, -INT64_C(9007199254740991));
    if (strcmp(name, "negative_2p53_plus1") == 0) return mrb_int64_value(mrb, -INT64_C(9007199254740993));
    if (strcmp(name, "i64_min") == 0) return mrb_int64_value(mrb, INT64_MIN);
    if (strcmp(name, "i64_max") == 0) return mrb_int64_value(mrb, INT64_MAX);
#ifdef MRB_USE_BIGINT
    if (strcmp(name, "i64_min_minus1") == 0) {
        return mrb_integer_from_bytes(mrb, int64_underflow_magnitude,
                                      sizeof(int64_underflow_magnitude), -1);
    }
#else
    (void)int64_underflow_magnitude;
#endif
    if (strcmp(name, "ui64_zero") == 0) return mrb_uint64_value(mrb, 0);
    if (strcmp(name, "ui64_42") == 0) return mrb_uint64_value(mrb, 42);
    if (strcmp(name, "ui64_i64_max") == 0) return mrb_uint64_value(mrb, (uint64_t)INT64_MAX);
    if (strcmp(name, "ui64_i64_max_plus1") == 0) return mrb_uint64_value(mrb, (uint64_t)INT64_MAX + 1);
    if (strcmp(name, "ui64_max") == 0) return mrb_uint64_value(mrb, UINT64_MAX);
#ifdef MRB_USE_BIGINT
    if (strcmp(name, "ui64_max_plus1") == 0) {
        return mrb_integer_from_bytes(mrb, uint64_overflow, sizeof(uint64_overflow), 1);
    }
#else
    (void)uint64_overflow;
#endif
    if (strcmp(name, "i32_min") == 0) return mrb_int_value(mrb, (mrb_int)INT32_MIN);
    if (strcmp(name, "i32_max") == 0) return mrb_int_value(mrb, (mrb_int)INT32_MAX);
    if (strcmp(name, "i32_min_minus1") == 0) return mrb_int64_value(mrb, (int64_t)INT32_MIN - 1);
    if (strcmp(name, "i32_max_plus1") == 0) return mrb_int64_value(mrb, (int64_t)INT32_MAX + 1);
    mrb_raise(mrb, E_ARGUMENT_ERROR, "unknown integer fixture");
}

static mrb_value
integer_wide_supported(mrb_state *mrb, mrb_value self)
{
    (void)mrb;
    (void)self;
#if defined(MRB_INT64) || defined(MRB_USE_BIGINT)
    return mrb_true_value();
#else
    return mrb_false_value();
#endif
}

static mrb_value
integer_bigint_enabled(mrb_state *mrb, mrb_value self)
{
    (void)mrb;
    (void)self;
#ifdef MRB_USE_BIGINT
    return mrb_true_value();
#else
    return mrb_false_value();
#endif
}

static mrb_value
integer_heap_boundary_values(mrb_state *mrb, mrb_value self)
{
    mrb_value values = mrb_ary_new_capa(mrb, 4);
    (void)self;
    mrb_ary_push(mrb, values, mrb_int_value(mrb, (mrb_int)MRB_FIXNUM_MIN));
    mrb_ary_push(mrb, values, mrb_int_value(mrb, (mrb_int)MRB_FIXNUM_MAX));
    if ((int64_t)MRB_FIXNUM_MIN > (int64_t)MRB_INT_MIN) {
        mrb_ary_push(mrb, values, mrb_int_value(mrb, (mrb_int)MRB_FIXNUM_MIN - 1));
    }
    if ((int64_t)MRB_FIXNUM_MAX < (int64_t)MRB_INT_MAX) {
        mrb_ary_push(mrb, values, mrb_int_value(mrb, (mrb_int)MRB_FIXNUM_MAX + 1));
    }
    return values;
}

struct integer_com_variant_call {
    VARIANT *variant;
};

static mrb_value
integer_from_com_variant(mrb_state *mrb, void *data)
{
    struct integer_com_variant_call *call = data;
    return ole_variant2val(mrb, call->variant);
}

static void
integer_set_com_fixture(mrb_state *mrb, VARIANT *variant, const char *name,
                        int64_t *byref_i8_value)
{
    SAFEARRAY *array;
    VARIANT element;
    LONG index = 0;
    HRESULT hr;

    if (strcmp(name, "i8_min") == 0) {
        V_VT(variant) = VT_I8;
        V_I8(variant) = INT64_MIN;
    }
    else if (strcmp(name, "i8_max") == 0) {
        V_VT(variant) = VT_I8;
        V_I8(variant) = INT64_MAX;
    }
    else if (strcmp(name, "i8_p2p53") == 0) {
        V_VT(variant) = VT_I8;
        V_I8(variant) = INT64_C(9007199254740993);
    }
    else if (strcmp(name, "i8_p2p53_minus1") == 0) {
        V_VT(variant) = VT_I8;
        V_I8(variant) = INT64_C(9007199254740991);
    }
    else if (strcmp(name, "i8_n2p53") == 0) {
        V_VT(variant) = VT_I8;
        V_I8(variant) = -INT64_C(9007199254740993);
    }
    else if (strcmp(name, "i8_n2p53_minus1") == 0) {
        V_VT(variant) = VT_I8;
        V_I8(variant) = -INT64_C(9007199254740991);
    }
    else if (strcmp(name, "ui8_zero") == 0) {
        V_VT(variant) = VT_UI8;
        V_UI8(variant) = 0;
    }
    else if (strcmp(name, "ui8_42") == 0) {
        V_VT(variant) = VT_UI8;
        V_UI8(variant) = 42;
    }
    else if (strcmp(name, "ui8_i64_max") == 0) {
        V_VT(variant) = VT_UI8;
        V_UI8(variant) = (uint64_t)INT64_MAX;
    }
    else if (strcmp(name, "ui8_i64_max_plus1") == 0) {
        V_VT(variant) = VT_UI8;
        V_UI8(variant) = (uint64_t)INT64_MAX + 1;
    }
    else if (strcmp(name, "ui8_max") == 0) {
        V_VT(variant) = VT_UI8;
        V_UI8(variant) = UINT64_MAX;
    }
    else if (strcmp(name, "byref_i8_p2p53") == 0) {
        *byref_i8_value = INT64_C(9007199254740993);
        V_VT(variant) = VT_I8 | VT_BYREF;
        V_BYREF(variant) = byref_i8_value;
    }
    else if (strcmp(name, "array_ui8_42") == 0) {
        array = SafeArrayCreateVector(VT_VARIANT, 0, 1);
        if (!array) mrb_raise(mrb, E_RUNTIME_ERROR, "SAFEARRAY allocation failed");
        VariantInit(&element);
        V_VT(&element) = VT_UI8;
        V_UI8(&element) = 42;
        hr = SafeArrayPutElement(array, &index, &element);
        VariantClear(&element);
        if (FAILED(hr)) {
            SafeArrayDestroy(array);
            mrb_raise(mrb, E_RUNTIME_ERROR, "SAFEARRAY element setup failed");
        }
        V_VT(variant) = VT_ARRAY | VT_VARIANT;
        V_ARRAY(variant) = array;
    }
    else {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "unknown COM integer fixture");
    }
}

static mrb_value
integer_from_com(mrb_state *mrb, mrb_value self)
{
    const char *name;
    VARIANT variant;
    int64_t byref_i8_value;
    struct integer_com_variant_call call;
    mrb_bool error = FALSE;
    mrb_value result;

    (void)self;
    mrb_get_args(mrb, "z", &name);
    VariantInit(&variant);
    integer_set_com_fixture(mrb, &variant, name, &byref_i8_value);
    call.variant = &variant;
    result = mrb_protect_error(mrb, integer_from_com_variant, &call, &error);
    VariantClear(&variant);
    if (error) mrb_exc_raise(mrb, result);
    return result;
}

static mrb_value
integer_array_overflow_cleanup_exercise(mrb_state *mrb, mrb_value self)
{
    SAFEARRAY *array;
    VARIANT variant;
    uint64_t value = UINT64_MAX;
    LONG index = 0;
    HRESULT hr;
    struct integer_com_variant_call call;
    mrb_bool error = FALSE;
    mrb_value exception;
    mrb_bool range_error;
    ULONG locks;

    (void)self;
    array = SafeArrayCreateVector(VT_UI8, 0, 1);
    if (!array) mrb_raise(mrb, E_RUNTIME_ERROR, "SAFEARRAY allocation failed");
    hr = SafeArrayPutElement(array, &index, &value);
    if (FAILED(hr)) {
        SafeArrayDestroy(array);
        mrb_raise(mrb, E_RUNTIME_ERROR, "SAFEARRAY element setup failed");
    }

    VariantInit(&variant);
    V_VT(&variant) = VT_ARRAY | VT_UI8;
    V_ARRAY(&variant) = array;
    call.variant = &variant;
    exception = mrb_protect_error(mrb, integer_from_com_variant, &call, &error);
    range_error = error && mrb_obj_is_kind_of(mrb, exception, mrb_class_get(mrb, "RangeError"));
    locks = array->cLocks;

    while (array->cLocks > 0) {
        hr = SafeArrayUnlock(array);
        if (FAILED(hr)) break;
    }
    V_VT(&variant) = VT_EMPTY;
    V_ARRAY(&variant) = NULL;
    hr = SafeArrayDestroy(array);
    if (!range_error || locks != 0 || FAILED(hr)) {
        mrb_raise(mrb, E_RUNTIME_ERROR,
                  "SAFEARRAY conversion did not raise RangeError and release its lock");
    }
    return mrb_true_value();
}

struct integer_float_write_call {
    mrb_value value;
    VARIANT *variant;
    VARTYPE type;
};

static mrb_value
integer_float_write(mrb_state *mrb, void *data)
{
    struct integer_float_write_call *call = data;
    ole_val2variant_ex(mrb, call->value, call->variant, call->type);
    return mrb_nil_value();
}

static mrb_value
integer_float_to_com(mrb_state *mrb, mrb_value self)
{
    mrb_value value;
    const char *kind;
    struct integer_float_write_call call;
    VARIANT variant;
    mrb_bool error = FALSE;
    mrb_value exception;
    double number;
    double truncated;
    uint64_t expected_unsigned;
    int64_t expected_signed;
    mrb_bool matches;

    (void)self;
    mrb_get_args(mrb, "oz", &value, &kind);
    if (!mrb_float_p(value)) mrb_raise(mrb, E_TYPE_ERROR, "expected a Float");
    if (strcmp(kind, "i8") == 0) {
        call.type = VT_I8;
    }
    else if (strcmp(kind, "ui8") == 0) {
        call.type = VT_UI8;
    }
    else {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "unknown integer VARIANT type");
    }

    VariantInit(&variant);
    call.value = value;
    call.variant = &variant;
    exception = mrb_protect_error(mrb, integer_float_write, &call, &error);
    if (error) {
        VariantClear(&variant);
        mrb_exc_raise(mrb, exception);
    }

    number = (double)mrb_float(value);
    truncated = trunc(number);
    if (number != number || number >= HUGE_VAL || number <= -HUGE_VAL) {
        VariantClear(&variant);
        mrb_raise(mrb, E_RUNTIME_ERROR, "non-finite Float unexpectedly converted to an integer VARIANT");
    }
    if (call.type == VT_UI8) {
        if (truncated < 0.0 || truncated >= 18446744073709551616.0) {
            VariantClear(&variant);
            mrb_raise(mrb, E_RUNTIME_ERROR, "out-of-range Float unexpectedly converted to VT_UI8");
        }
        expected_unsigned = (uint64_t)truncated;
        matches = V_VT(&variant) == VT_UI8 && V_UI8(&variant) == expected_unsigned;
    }
    else {
        if (truncated < -9223372036854775808.0 || truncated >= 9223372036854775808.0) {
            VariantClear(&variant);
            mrb_raise(mrb, E_RUNTIME_ERROR, "out-of-range Float unexpectedly converted to VT_I8");
        }
        expected_signed = (int64_t)truncated;
        matches = V_VT(&variant) == VT_I8 && V_I8(&variant) == expected_signed;
    }
    VariantClear(&variant);
    if (!matches) mrb_raise(mrb, E_RUNTIME_ERROR, "Float integer conversion stored an unexpected VARIANT value");
    return mrb_true_value();
}

void
mrb_mruby_win32ole_integer_conversion_test(mrb_state *mrb)
{
    mrb_define_method(mrb, mrb->object_class, "__win32ole_integer_to_variant",
                      integer_to_variant, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, mrb->object_class, "__win32ole_integer_fixture",
                      integer_fixture, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, mrb->object_class, "__win32ole_integer_wide_supported?",
                      integer_wide_supported, MRB_ARGS_NONE());
    mrb_define_method(mrb, mrb->object_class, "__win32ole_integer_bigint_enabled?",
                      integer_bigint_enabled, MRB_ARGS_NONE());
    mrb_define_method(mrb, mrb->object_class, "__win32ole_integer_heap_boundary_values",
                      integer_heap_boundary_values, MRB_ARGS_NONE());
    mrb_define_method(mrb, mrb->object_class, "__win32ole_integer_from_com",
                      integer_from_com, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, mrb->object_class, "__win32ole_integer_array_overflow_cleanup_exercise",
                      integer_array_overflow_cleanup_exercise, MRB_ARGS_NONE());
    mrb_define_method(mrb, mrb->object_class, "__win32ole_integer_float_to_com",
                      integer_float_to_com, MRB_ARGS_REQ(2));
}
