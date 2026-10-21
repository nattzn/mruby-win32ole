assert('WIN32OLE, WIN32OLE_TYPE, WIN32OLE_PARAM, and WIN32OLE_TYPELIB release old COM references on failed reinitialize') do
  ["WIN32OLE", "WIN32OLE_TYPE", "WIN32OLE_PARAM", "WIN32OLE_TYPELIB"].each do |klass|
    assert_true Object.new.__win32ole_reinitialize_release_exercise(klass)
  end
end

assert('WIN32OLE remains usable after successful reinitialize') do
  dictionary = WIN32OLE.new('Scripting.Dictionary')
  begin
    dictionary.Add('old', 1)
    dictionary.__send__(:initialize, 'Scripting.Dictionary')
    assert_equal(0, dictionary.Count)
    dictionary.Add('new', 2)
    assert_equal(1, dictionary.Count)
  ensure
    dictionary.ole_free
  end
end

event_host = nil
begin
  event_host = WIN32OLE.new('ADODB.Connection')
rescue
  # ADO is optional on Windows installations.
end

if event_host
  assert('WIN32OLE_EVENT releases its old connection on successful and failed reinitialize') do
    registry = Object.new.__win32ole_event_registry
    original_refs = WIN32OLE.ole_reference_count(event_host)
    event = WIN32OLE_EVENT.new(event_host, 'ConnectionEvents')
    begin
      event_index = registry.length - 1
      advised_refs = WIN32OLE.ole_reference_count(event_host)
      assert_equal(event, registry[event_index])
      event.__send__(:initialize, event_host, 'ConnectionEvents')
      assert_nil(registry[event_index])
      assert_equal(event, registry[registry.length - 1])
      assert_equal(advised_refs, WIN32OLE.ole_reference_count(event_host))

      event_index = registry.length - 1
      assert_raise(TypeError) { event.__send__(:initialize) }
      assert_nil(registry[event_index])
      assert_equal(original_refs, WIN32OLE.ole_reference_count(event_host))
    ensure
      event.unadvise
    end
  end
else
  assert('WIN32OLE_EVENT reinitialize registry cleanup (skipped: ADODB.Connection unavailable)') do
    skip('ADODB.Connection is unavailable')
  end
end
