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

## コールバックの戻り値

COM コールバックの戻り値は、Ruby オブジェクトから独立した独自のデータとして保持されます。SAFEARRAY 内の VARIANT 要素を含め、BYREF 値は値渡し（実体化された値）として返されます。例えば、`VT_BSTR | VT_BYREF` の戻り値は `VT_BSTR` として返されます。

レコード（VT_RECORD）は独自の `IRecordInfo` 参照を持つ独立した複製として返されます。COM 呼び出し元は `VariantClear` で戻り値を安全に解放でき、元の Ruby オブジェクトを生存させておく必要はありません。なお、引数としての BYREF 渡しについては変更ありません。

## ライセンス

GNU General Public License または Artistic License

詳細は `mruby-win32ole/src/win32ole.c` を参照してください。
