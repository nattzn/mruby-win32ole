integer_test = Object.new
integer_vt = WIN32OLE::VARIANT

assert('WIN32OLE integer conversion preserves exact signed and unsigned COM values') do
  integer_test.__win32ole_integer_heap_boundary_values.each do |value|
    converted = integer_test.__win32ole_integer_to_variant(value, 'auto')
    assert_kind_of(Integer, converted[1])
    assert_equal(value, converted[1])
  end

  if integer_test.__win32ole_integer_wide_supported?
    [
      ['positive_2p53_minus1', 'i8_p2p53_minus1'],
      ['positive_2p53_plus1', 'i8_p2p53'],
      ['negative_2p53_minus1', 'i8_n2p53_minus1'],
      ['negative_2p53_plus1', 'i8_n2p53'],
      ['i64_min', 'i8_min'], ['i64_max', 'i8_max']
    ].each do |name, com_name|
      value = integer_test.__win32ole_integer_fixture(name)
      converted = integer_test.__win32ole_integer_to_variant(value, 'i8')
      assert_equal(integer_vt::VT_I8, converted[0])
      assert_kind_of(Integer, converted[1])
      assert_equal(value, converted[1])
      from_com = integer_test.__win32ole_integer_from_com(com_name)
      assert_kind_of(Integer, from_com)
      assert_equal(value, from_com)
    end
  end

  ['ui64_zero', 'ui64_42'].each do |name|
    value = integer_test.__win32ole_integer_fixture(name)
    converted = integer_test.__win32ole_integer_to_variant(value, 'ui8')
    assert_equal(integer_vt::VT_UI8, converted[0])
    assert_kind_of(Integer, converted[1])
    assert_equal(value, converted[1])
  end

  if integer_test.__win32ole_integer_wide_supported?
    value = integer_test.__win32ole_integer_fixture('ui64_i64_max')
    converted = integer_test.__win32ole_integer_to_variant(value, 'ui8')
    assert_equal(integer_vt::VT_UI8, converted[0])
    assert_kind_of(Integer, converted[1])
    assert_equal(value, converted[1])
    from_com = integer_test.__win32ole_integer_from_com('ui8_i64_max')
    assert_kind_of(Integer, from_com)
    assert_equal(value, from_com)
  end

  if integer_test.__win32ole_integer_bigint_enabled?
    ['ui64_i64_max_plus1', 'ui64_max'].each do |name|
      value = integer_test.__win32ole_integer_fixture(name)
      converted = integer_test.__win32ole_integer_to_variant(value, 'ui8')
      assert_equal(integer_vt::VT_UI8, converted[0])
      assert_kind_of(Integer, converted[1])
      assert_equal(value, converted[1])
    end
    from_com = integer_test.__win32ole_integer_from_com('ui8_max')
    assert_kind_of(Integer, from_com)
    assert_equal(integer_test.__win32ole_integer_fixture('ui64_max'), from_com)
    from_com = integer_test.__win32ole_integer_from_com('ui8_i64_max_plus1')
    assert_kind_of(Integer, from_com)
    assert_equal(integer_test.__win32ole_integer_fixture('ui64_i64_max_plus1'), from_com)
    assert_raise(RangeError) do
      integer_test.__win32ole_integer_to_variant(
        integer_test.__win32ole_integer_fixture('ui64_max'), 'i8'
      )
    end
    assert_raise(RangeError) do
      integer_test.__win32ole_integer_to_variant(
        integer_test.__win32ole_integer_fixture('i64_min_minus1'), 'i8'
      )
    end
    assert_raise(RangeError) do
      integer_test.__win32ole_integer_to_variant(
        integer_test.__win32ole_integer_fixture('ui64_max_plus1'), 'ui8'
      )
    end
    assert_raise(RangeError) do
      integer_test.__win32ole_integer_to_variant(-1, 'ui8')
    end
    assert_raise(RangeError) do
      integer_test.__win32ole_integer_to_variant(
        integer_test.__win32ole_integer_fixture('ui64_max_plus1'), 'auto'
      )
    end
  else
    assert_raise(RangeError) { integer_test.__win32ole_integer_from_com('ui8_max') }
    assert_raise(RangeError) { integer_test.__win32ole_integer_from_com('i8_max') } unless integer_test.__win32ole_integer_wide_supported?
    assert_raise(RangeError) { integer_test.__win32ole_integer_from_com('ui8_i64_max_plus1') }
  end

  true
end

assert('WIN32OLE integer auto selection follows COM signed and unsigned boundaries') do
  [
    ['i32_min', integer_vt::VT_I4], ['i32_max', integer_vt::VT_I4]
  ].each do |name, expected_type|
    value = integer_test.__win32ole_integer_fixture(name)
    converted = integer_test.__win32ole_integer_to_variant(value, 'auto')
    assert_equal(expected_type, converted[0])
    assert_kind_of(Integer, converted[1])
    assert_equal(value, converted[1])
  end

  if integer_test.__win32ole_integer_wide_supported?
    ['i32_min_minus1', 'i32_max_plus1'].each do |name|
      value = integer_test.__win32ole_integer_fixture(name)
      converted = integer_test.__win32ole_integer_to_variant(value, 'auto')
      assert_equal(integer_vt::VT_I8, converted[0])
      assert_kind_of(Integer, converted[1])
      assert_equal(value, converted[1])
    end
  end

  if integer_test.__win32ole_integer_bigint_enabled?
    value = integer_test.__win32ole_integer_fixture('ui64_i64_max_plus1')
    converted = integer_test.__win32ole_integer_to_variant(value, 'auto')
    assert_equal(integer_vt::VT_UI8, converted[0])
    assert_kind_of(Integer, converted[1])
    assert_equal(value, converted[1])
  end
  true
end

assert('WIN32OLE restores nil conversion after an integer RangeError') do
  before = integer_test.__win32ole_integer_to_variant(nil, 'auto')
  if integer_test.__win32ole_integer_bigint_enabled?
    overflow = integer_test.__win32ole_integer_fixture('ui64_max_plus1')
    assert_raise(RangeError) { WIN32OLE_VARIANT.new(overflow, integer_vt::VT_VARIANT) }
  end
  assert_equal(before, integer_test.__win32ole_integer_to_variant(nil, 'auto'))
  true
end

assert('WIN32OLE integer conversion handles nil, finite floats, errors, BYREF, arrays, and value updates') do
  [
    ['i8', integer_vt::VT_I8], ['ui8', integer_vt::VT_UI8]
  ].each do |kind, expected_type|
    converted = integer_test.__win32ole_integer_to_variant(nil, kind)
    assert_equal(expected_type, converted[0])
    assert_kind_of(Integer, converted[1])
    assert_equal(0, converted[1])
  end

  [[42.9, 42], [-42.9, -42]].each do |value, truncated|
    converted = integer_test.__win32ole_integer_to_variant(value, 'i8')
    assert_kind_of(Integer, converted[1])
    assert_equal(truncated, converted[1])
  end
  assert_raise(TypeError) { integer_test.__win32ole_integer_to_variant('not an integer', 'i8') }
  [Float::NAN, Float::INFINITY, -Float::INFINITY].each do |value|
    assert_raise(RangeError) { integer_test.__win32ole_integer_to_variant(value, 'i8') }
  end

  if integer_test.__win32ole_integer_wide_supported?
    expected = integer_test.__win32ole_integer_fixture('positive_2p53_plus1')
    from_com = integer_test.__win32ole_integer_from_com('byref_i8_p2p53')
    assert_kind_of(Integer, from_com)
    assert_equal(expected, from_com)
  end
  array_result = integer_test.__win32ole_integer_from_com('array_ui8_42')
  assert_equal([42], array_result)
  assert_kind_of(Integer, array_result[0])

  unless integer_test.__win32ole_integer_bigint_enabled?
    assert_true integer_test.__win32ole_integer_array_overflow_cleanup_exercise
  end

  if integer_test.__win32ole_integer_wide_supported?
    variant = WIN32OLE_VARIANT.new(0, integer_vt::VT_I8)
    expected = integer_test.__win32ole_integer_fixture('positive_2p53_plus1')
    variant.value = expected
    assert_kind_of(Integer, variant.value)
    assert_equal(expected, variant.value)

    [integer_vt::VT_I8, integer_vt::VT_UI8].each do |type|
      byref = WIN32OLE_VARIANT.new(expected, type | integer_vt::VT_BYREF)
      assert_kind_of(Integer, byref.value)
      assert_equal(expected, byref.value)
      array = WIN32OLE_VARIANT.new([expected], type | integer_vt::VT_ARRAY)
      assert_kind_of(Integer, array[0])
      assert_equal([expected], array.value)
      assert_equal(expected, array[0])
      array[0] = expected + 2
      assert_equal(expected + 2, array[0])
      assert_raise(RangeError) { array[0] = -1 } if type == integer_vt::VT_UI8
      assert_equal(expected + 2, array[0])
    end
  end
  true
end

assert('WIN32OLE Float integer conversion checks signed and unsigned 64-bit bounds before casting') do
  assert_true integer_test.__win32ole_integer_float_to_com(2.0 ** 63, 'ui8')
  assert_true integer_test.__win32ole_integer_float_to_com(-0.5, 'ui8')
  assert_raise(RangeError) do
    integer_test.__win32ole_integer_float_to_com(2.0 ** 64, 'ui8')
  end
  assert_raise(RangeError) do
    integer_test.__win32ole_integer_float_to_com(2.0 ** 63, 'i8')
  end

  [Float::NAN, Float::INFINITY, -Float::INFINITY].each do |value|
    ['i8', 'ui8'].each do |kind|
      assert_raise(RangeError) { integer_test.__win32ole_integer_float_to_com(value, kind) }
    end
  end
  true
end
