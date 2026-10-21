mruby-win32ole
==============

A port of CRuby's WIN32OLE to mruby.

## Installation

In the mruby `build_config.rb` file add:

```ruby
conf.gem :git => 'git://github.com/sdottaka/mruby-win32ole.git'
```

## Examples

See `mruby-win32ole/example` directory.

## Object lifetime

After `WIN32OLE#ole_free`, methods that require a live COM interface raise
`RuntimeError`. `inspect` and repeated calls to `ole_free` remain safe, including
when failed reinitialization has left the object without native data.

Methods on type information, parameter, method, record, and variant wrappers that
require native data also raise `RuntimeError` if that data is missing after failed
reinitialization. Initializers may release previous native data before validating
new arguments; a failed reinitialization is not guaranteed to preserve previous
object state.

## Callback return values

COM callback return values own their data independently of Ruby objects. BYREF
values are returned by value, including VARIANT elements inside SAFEARRAYs. For
example, a `VT_BSTR | VT_BYREF` result is returned as `VT_BSTR`.

Records are returned as independent copies with their own `IRecordInfo` reference.
The COM caller can release the result with `VariantClear`; keeping the original
Ruby object alive is unnecessary. BYREF argument passing is unchanged.

## License

GNU General Public License or Artistic License

See `mruby-win32ole/src/win32ole.c`
