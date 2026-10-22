invoke_cleanup_test = Object.new

assert('WIN32OLE late and early invoke release converted Dispatch arguments after integer RangeError') do
  if invoke_cleanup_test.__win32ole_integer_invoke_cleanup_bigint_enabled?
    assert_true invoke_cleanup_test.__win32ole_integer_invoke_cleanup_bigint_exercise
  else
    skip('mruby-bigint is unavailable')
  end
end

assert('WIN32OLE late and early invoke release output and Dispatch references after RangeError') do
  if invoke_cleanup_test.__win32ole_integer_invoke_cleanup_bigint_enabled?
    skip('mruby-bigint is enabled')
  else
    assert_true invoke_cleanup_test.__win32ole_integer_invoke_cleanup_output_exercise
  end
end
