class Win32OLECallbackArenaFixture
  def string_result
    "callback result"
  end

  def immediate_result
    42
  end

  def byref_result
    WIN32OLE_VARIANT.new("callback result", WIN32OLE::VARIANT::VT_BSTR | WIN32OLE::VARIANT::VT_BYREF)
  end

  def raise_error
    raise "callback failure"
  end
end

fixture = Win32OLECallbackArenaFixture.new
assert('WIN32OLE COM callback preserves the GC arena and VARIANT results') do
  assert_true fixture.__win32ole_callback_arena_exercise("string_result", 100)
  assert_true fixture.__win32ole_callback_arena_exercise("immediate_result", 100)
  assert_true fixture.__win32ole_callback_arena_exercise("byref_result", 100)
  assert_true fixture.__win32ole_callback_arena_exercise("raise_error", 100)
end
