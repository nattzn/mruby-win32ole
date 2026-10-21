array = WIN32OLE_VARIANT.new([[1, 2], [3, 4]])

assert('WIN32OLE_VARIANT array index conversion errors leave the array usable') do
  assert_raise(TypeError) { array["invalid", 0] }
  assert_raise(TypeError) { array[0, "invalid"] }
  assert_raise(TypeError) { array["invalid", 0] = 9 }
  assert_raise(TypeError) { array[0, "invalid"] = 9 }

  assert_equal(1, array[0, 0])
  assert_equal(4, array[1, 1])
  array[0, 1] = 8
  array[1, 0] = 7
  assert_equal(8, array[0, 1])
  assert_equal(7, array[1, 0])
end
