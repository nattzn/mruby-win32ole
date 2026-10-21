assert('WIN32OLE argument conversion failure releases earlier dispatch arguments') do
  assert_true Object.new.__win32ole_early_invoke_cleanup_exercise
end

assert('released WIN32OLE rejects _invoke, _getproperty, and _setproperty') do
  ole = WIN32OLE.new('Scripting.Dictionary')
  ole.ole_free

  assert_raise(RuntimeError) { ole._invoke(1, [], []) }
  assert_raise(RuntimeError) { ole._getproperty(1, [], []) }
  assert_raise(RuntimeError) { ole._setproperty(1, [], []) }
end

assert('WIN32OLE methods reject an object with no initialized data') do
  ole = WIN32OLE.new('Scripting.Dictionary')
  assert_raise(ArgumentError) { ole.__send__(:initialize) }

  assert_raise(RuntimeError) { ole._invoke(1, [], []) }
  assert_raise(RuntimeError) { ole._getproperty(1, [], []) }
  assert_raise(RuntimeError) { ole._setproperty(1, [], []) }
end

assert('WIN32OLE invoke methods still work for a live receiver') do
  ole = WIN32OLE.new('Scripting.Dictionary')
  begin
    ole._invoke(1, ['key', 7], [WIN32OLE::VARIANT::VT_VARIANT, WIN32OLE::VARIANT::VT_VARIANT])
    assert_equal(7, ole._getproperty(0, ['key'], [WIN32OLE::VARIANT::VT_VARIANT]))
    assert_nil(ole._setproperty(0, ['key', 8], [WIN32OLE::VARIANT::VT_VARIANT, WIN32OLE::VARIANT::VT_VARIANT]))
    assert_equal(8, ole._getproperty(0, ['key'], [WIN32OLE::VARIANT::VT_VARIANT]))
  ensure
    ole.ole_free
  end
end
