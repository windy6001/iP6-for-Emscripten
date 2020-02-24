# iP6 for Emscripten    (PC-6000/6600 Series Emulator) 

by windy6001
First commit: 2020/2/22

## 概要

isio さんの PC-6000/6600 シリーズエミュレータの iP6 を、Unix/X11から、SDL2 に対応した上で、Emscriptenを使用して、ブラウザで動くようにしてみました。
あくまで、コンセプトモデルです。ソースリストがすごいことになっています。正確な処理ができてない部分がありますが、ご容赦ください。

## ビルドの方法 [Emscripten 版]

Mac mini  (Mojave)でビルドしています。

- Emscripten のビルド環境を作成してください。この辺りを参考にしてください。→（https://emscripten.org/docs/getting_started/downloads.html）
- ソースリストのディレクトリの下にある rom というサブディレクトリにROMファイルをコピーしてください。
- make を実行してください。
- うまくいけば、iP6.html / iP6.data / iP6.js / iP6.wasm ファイルが出来上がります。

## ビルドの方法 [SDL2 版]

- SDL2をインストールしてください。
- make CC=gcc を実行してください。
- うまくいけば、実行ファイルの iP6 ファイルが出来上がります。


## 使い方 [Emscripten 版]

- ターミナルで、ソースリストのあるディレクトリに移動してください。
- 下記のワンライナーで、Web サーバーを起動してください。<br>
  $ ruby -rwebrick -e 'WEBrick::HTTPServer.new(:DocumentRoot => "./", :Port => 8000).start'
- ブラウザで、http://localhost:8000/iP6.html   にアクセスしてください。

## 気をつけること

Emscripten版は、iP6.data という生成物にROMファイルが含まれてしまっているようです。実機ROMで試すときは、外部サーバーに置かない方がいいと思います。

## 主なできないこと 

- キー入力できますが、まだ正しく入力できないキーが沢山あります。
- キー入力が速いと、取りこぼすことがあるようです。
- 音楽が鳴りません。
- 起動中に設定をいじれません。機種変更もできません。
- その他、iP6 がサポートしていない機能は、使えません。

## ライセンス

This software is licensed under Marat Fayzullin's fmsx license.
commercial use is prohibited.

This software has no warranty. The author assumes no responsibility whatsoever for any problems that may arise from using this software.


## 試してみた環境 [Emscripten 版]

- Macと、Windowsの Firefox 
- Macの Safari と、iPhoneのモバイルSafari
- Microsoft Edge

## 免責事項

このソフトウエアには保証がありません。このソフトウエアを使用したことで問題が発生したとしても、作者は責任を負いません。

## 謝辞

- エミュレータを作っていただいた、isio さんに感謝します。
- Marat Fayzullin さんに感謝します。
