#include <mruby.h>
#include <mruby/data.h>

static mrb_value
object_state_data_ptr_null(mrb_state *mrb, mrb_value self)
{
    return DATA_PTR(self) == NULL ? mrb_true_value() : mrb_false_value();
}

void
mrb_mruby_win32ole_object_state_test(mrb_state *mrb)
{
    mrb_define_method(mrb, mrb->object_class, "__win32ole_data_ptr_null?",
                      object_state_data_ptr_null, MRB_ARGS_NONE());
}
