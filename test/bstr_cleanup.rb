assert('WIN32OLE_PARAM releases FUNCDESC when parameter initialization raises') do
  assert_true Object.new.__win32ole_bstr_cleanup_exercise
end

event_source = nil
begin
  event_source = WIN32OLE.new('ADODB.Connection')
rescue RuntimeError
  # ADO is optional on Windows installations.
end

assert('WIN32OLE_EVENT remains usable after an exception during argument name lookup') do
  skip('ADODB.Connection is unavailable') unless event_source
  event = nil
  begin
    event = WIN32OLE_EVENT.new(event_source, 'ConnectionEvents')
    lookup_seen = false
    event.on_event('WillConnect') do |*args|
      Hash.new do |hash, key|
        lookup_seen = true
        raise 'event name lookup failure'
      end
    end
    # A missing provider avoids opening an external connection.
    assert_raise(RuntimeError) { event_source.Open('Provider=__CodexMissingProvider__') }
    assert_true(lookup_seen)

    next_event_seen = false
    event.on_event('WillConnect') do |*args|
      next_event_seen = true
      {}
    end
    assert_raise(RuntimeError) { event_source.Open('Provider=__CodexMissingProvider__') }
    assert_true(next_event_seen)
  ensure
    event.unadvise if event
    event_source.ole_free
  end
end
