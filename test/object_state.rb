assert('WIN32OLE remains safe after failed initialize and repeated ole_free') do
  ole = WIN32OLE.new('Scripting.Dictionary')
  assert_raise(ArgumentError) { ole.__send__(:initialize) }
  assert_true(ole.__win32ole_data_ptr_null?)
  assert_kind_of(String, ole.inspect)
  assert_nil(ole.ole_free)
  assert_nil(ole.ole_free)

  assert_raise(RuntimeError) { ole.invoke('Count') }
  assert_raise(RuntimeError) { ole.CompareMode = 0 }
  assert_raise(RuntimeError) { ole.each { |_key, _value| } }
  assert_raise(RuntimeError) { ole.ole_methods }
  assert_raise(RuntimeError) { ole.ole_get_methods }
  assert_raise(RuntimeError) { ole.ole_put_methods }
  assert_raise(RuntimeError) { ole.ole_func_methods }
  assert_raise(RuntimeError) { ole.ole_method('Count') }
  assert_raise(RuntimeError) { ole.ole_type }
  assert_raise(RuntimeError) { ole.ole_typelib }
  assert_raise(RuntimeError) { ole.ole_respond_to?('Count') }
  assert_raise(RuntimeError) { ole.ole_activex_initialize }
  assert_raise(RuntimeError) { ole.ole_query_interface('{00020400-0000-0000-C000-000000000046}') }
  assert_raise(RuntimeError) { WIN32OLE.const_load(ole) }
  assert_raise(RuntimeError) { WIN32OLE_EVENT.new(ole) }
  assert_raise(RuntimeError) { WIN32OLE_EVENT.new(ole, 'IDispatch') }
  true
end

assert('released WIN32OLE remains safe to inspect and release repeatedly') do
  ole = WIN32OLE.new('Scripting.Dictionary')
  ole.ole_free
  assert_equal(false, ole.__win32ole_data_ptr_null?)
  assert_kind_of(String, ole.inspect)
  assert_nil(ole.ole_free)
  assert_nil(ole.ole_free)
  assert_raise(RuntimeError) { ole.invoke('Count') }
  assert_raise(RuntimeError) { ole.CompareMode = 0 }
  assert_raise(RuntimeError) { ole.each { |_key, _value| } }
  assert_raise(RuntimeError) { ole.ole_methods }
  assert_raise(RuntimeError) { ole.ole_get_methods }
  assert_raise(RuntimeError) { ole.ole_put_methods }
  assert_raise(RuntimeError) { ole.ole_func_methods }
  assert_raise(RuntimeError) { ole.ole_method('Count') }
  assert_raise(RuntimeError) { ole.ole_type }
  assert_raise(RuntimeError) { ole.ole_typelib }
  assert_raise(RuntimeError) { ole.ole_respond_to?('Count') }
  assert_raise(RuntimeError) { ole.ole_activex_initialize }
  assert_raise(RuntimeError) { ole.ole_query_interface('{00020400-0000-0000-C000-000000000046}') }
  assert_raise(RuntimeError) { WIN32OLE.const_load(ole) }
  assert_raise(RuntimeError) { WIN32OLE_EVENT.new(ole) }
  assert_raise(RuntimeError) { WIN32OLE_EVENT.new(ole, 'IDispatch') }
  true
end

assert('WIN32OLE argument conversion rejects an object with no native data') do
  receiver = WIN32OLE.new('Scripting.Dictionary')
  argument = WIN32OLE.new('Scripting.Dictionary')
  assert_raise(ArgumentError) { argument.__send__(:initialize) }
  assert_raise(RuntimeError) do
    receiver._invoke(1, [argument], [WIN32OLE::VARIANT::VT_VARIANT])
  end
  receiver.ole_free
  true
end

assert('WIN32OLE metadata objects reject access after failed initialize') do
  ole = WIN32OLE.new('Scripting.Dictionary')
  typelib = ole.ole_typelib
  type = ole.ole_type
  method = ole.ole_method('Add')
  param = method.params[0]
  objects = [
    [type, :ole_type, [nil, nil], TypeError],
    [method, :return_type, [nil, nil], TypeError],
    [param, :ole_type, [nil, nil], TypeError],
    [typelib, :name, [], ArgumentError]
  ]

  objects.each do |object, accessor, args, error_class|
    assert_raise(error_class) { object.__send__(:initialize, *args) }
    assert_true(object.__win32ole_data_ptr_null?)
    assert_raise(RuntimeError) { object.__send__(accessor) }
  end
  true
end

assert('WIN32OLE_VARIANT rejects access after failed initialize') do
  variant = WIN32OLE_VARIANT.new(1)
  assert_raise(ArgumentError) { variant.__send__(:initialize) }
  assert_true(variant.__win32ole_data_ptr_null?)
  assert_raise(RuntimeError) { variant.value }
end

assert('WIN32OLE_RECORD without native data raises instead of crashing') do
  record = WIN32OLE_RECORD.allocate
  assert_raise(ArgumentError) { record.__send__(:initialize, nil, nil) }
  assert_true(record.__win32ole_data_ptr_null?)
  ole = WIN32OLE.new('Scripting.Dictionary')
  assert_raise(RuntimeError) do
    ole._invoke(1, [record], [WIN32OLE::VARIANT::VT_VARIANT])
  end
  ole.ole_free
end
