class Win32OLECallbackArenaFixture
  def string_result
    "callback result"
  end

  def immediate_result
    42
  end

  def byref_result
    @result_source = WIN32OLE_VARIANT.new("callback result", WIN32OLE::VARIANT::VT_BSTR | WIN32OLE::VARIANT::VT_BYREF)
  end

  def mutate_result_source
    @result_source = WIN32OLE_VARIANT.new("changed after callback")
  end

  def byref_array_result
    [byref_result]
  end

  def nested_byref_array_result
    inner = WIN32OLE_VARIANT.new([byref_result], WIN32OLE::VARIANT::VT_ARRAY | WIN32OLE::VARIANT::VT_VARIANT)
    [inner]
  end

  def raise_error
    raise "callback failure"
  end
end

fixture = Win32OLECallbackArenaFixture.new
assert('WIN32OLE COM callback returns independent VARIANT results') do
  assert_true fixture.__win32ole_callback_arena_exercise("string_result", 100)
  assert_true fixture.__win32ole_callback_arena_exercise("immediate_result", 100)
  assert_true fixture.__win32ole_callback_arena_exercise("byref_result", 100)
  assert_true fixture.__win32ole_callback_arena_exercise("byref_array_result", 100)
  assert_true fixture.__win32ole_callback_arena_exercise("nested_byref_array_result", 100)
  assert_true fixture.__win32ole_callback_arena_exercise("raise_error", 100)
  assert_true Object.new.__win32ole_record_result_exercise
end

event_result_source = nil
begin
  event_result_source = WIN32OLE.new('ADODB.Connection')
rescue RuntimeError
  # ADO is optional on Windows installations.
end

assert('WIN32OLE_EVENT returns independent VARIANT results') do
  skip('ADODB.Connection is unavailable') unless event_result_source
  event = nil
  begin
    event = WIN32OLE_EVENT.new(event_result_source, 'ConnectionEvents')
    in_array = false
    event.on_event('WillConnect') do |*args|
      value = WIN32OLE_VARIANT.new("callback result", WIN32OLE::VARIANT::VT_BSTR | WIN32OLE::VARIANT::VT_BYREF)
      in_array ? [value] : value
    end
    assert_true Object.new.__win32ole_event_result_arena_exercise(event_result_source, false, 100)
    in_array = true
    assert_true Object.new.__win32ole_event_result_arena_exercise(event_result_source, true, 100)
  ensure
    event.unadvise if event
    event_result_source.ole_free
  end
end
