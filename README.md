# mruby-win32ole

CRuby の WIN32OLE を mruby に移植した mrbgem です。

## インストール

mruby の `build_config.rb` に以下を追加してください:

```ruby
conf.gem :github => 'nattzn/mruby-win32ole'
```

## 使用例

`mruby-win32ole/example` ディレクトリを参照してください。

## オブジェクトの生存期間

`WIN32OLE#ole_free` の呼び出し後、有効な COM インターフェースを必要とするメソッドは `RuntimeError` を発生させます。`inspect` や `ole_free` の複数回呼び出しは安全に実行できます（再初期化の失敗によってオブジェクトが内部のネイティブデータを失っている場合も含みます）。

型情報、パラメータ、メソッド、レコード、および VARIANT のラッパーオブジェクトにおいて、ネイティブデータを必要とするメソッドも、再初期化の失敗によりデータが失われている場合は `RuntimeError` を発生させます。イニシャライザは新しい引数を検証する前に既存のネイティブデータを解放することがあるため、再初期化に失敗した場合に以前のオブジェクト状態が維持される保証はありません。

## 整数の変換

ビルドには、`mrb_int64_value`、`mrb_uint64_value`、`mrb_as_int64`、`mrb_as_uint64` を提供する mruby が必要です。bigint 有効時は `mrb_integer_to_bytes` も使用します。このワークスペースの mruby はこれらの API を提供しています。

COM の整数値は Float を経由せず、正確な Integer として読み書きします。Ruby 側で値を保持できない場合や、指定した `VT_I8`（符号付き64 bit）・`VT_UI8`（符号なし64 bit）の範囲を超える場合は `RangeError` を発生させます。負の整数を `VT_UI8` に変換する場合も `RangeError` になります。

`mruby-bigint` が有効なビルドでは、`VT_UI8` の最大値 `2**64 - 1` まで正確に扱えます。bigint がないビルドでは、COM から受け取った値が `mrb_int` の範囲を超えると `RangeError` になります。即値として格納できる範囲を超えても、`mrb_int` に収まる整数は正確に扱えます。

整数の VARIANT 型を省略した場合は、符号付き32 bit に収まる値を `VT_I4`、それを超えて符号付き64 bit に収まる値を `VT_I8`、bigint の正の値でそれを超えるものを `VT_UI8` に変換します。COM の64 bit 整数にも収まらない値は `RangeError` になります。

明示した整数型への `nil` は従来どおりゼロになります。`VT_I8`・`VT_UI8` への Float 入力は小数部をゼロ方向に切り捨てますが、Float 自体が失った精度は復元されません。NaN、無限大、および変換できる範囲を超える値は `RangeError` になります。BYREF と SAFEARRAY の整数要素にも同じ正確性と範囲の規則が適用されます。

## コールバックの戻り値

COM コールバックの戻り値は、Ruby オブジェクトから独立した独自のデータとして保持されます。SAFEARRAY 内の VARIANT 要素を含め、BYREF 値は値渡し（実体化された値）として返されます。例えば、`VT_BSTR | VT_BYREF` の戻り値は `VT_BSTR` として返されます。

レコード（VT_RECORD）は独自の `IRecordInfo` 参照を持つ独立した複製として返されます。COM 呼び出し元は `VariantClear` で戻り値を安全に解放でき、元の Ruby オブジェクトを生存させておく必要はありません。なお、引数としての BYREF 渡しについては変更ありません。

## ライセンス

GNU General Public License または Artistic License

詳細は `mruby-win32ole/src/win32ole.c` を参照してください。
