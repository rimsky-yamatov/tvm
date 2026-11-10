# tinyvm — 小さくて速い、C99のスタックVM(複数ウィンドウGUI・音・固定小数点対応)

```
make                      # 要 SDL2 と libm。tvm / tasm / tdis ができる
./tasm examples/hello.tas hello.tvm && ./tvm hello.tvm
./tasm examples/win2.tas  win2.tvm  && ./tvm win2.tvm     # TVM_SCALE=3 でズーム変更
./tdis hello.tvm                                          # 逆アセンブル
make test                                                 # 全サンプルとテスト
```

## 構成
| ファイル | 役割 |
|---|---|
| `tvm.h` | 命令表(X-macro)、ファイルヘッダ定義。VM・アセンブラ・逆アセンブラで共有 |
| `vm.c` | VMコア(libc不使用、確保なし、computed goto / switch自動切替) |
| `main.c` | ホスト: 読込、`SYS`実装(コンソール、GUI、音、数学) |
| `gui.h` / `gui_sdl.c` | GUIバックエンド境界 / SDL2実装(最大8ウィンドウ) |
| `tasm.c` / `tdis.c` | アセンブラ / 逆アセンブラ |

## ファイル形式
`[0xB1][開始アドレス ULEB128][イメージ…]`(通常ヘッダ2バイト)。イメージは0番地にロードされ、開始アドレスから実行。
アセンブラは `.entry` 指定、なければラベル `main`、なければ0を開始アドレスにする。`0xB1` で始まらないファイルはヘッダなしの生イメージとして0番地から実行。

## 実行モデル
- 値は32bit。データスタック1024、メモリ64KB(コードとデータ共有、アドレスは16bitで折り返し)。範囲外アクセスでクラッシュしない
- 0除算は0、`INT_MIN/-1` も安全
- フレーム: `CALL`で `fp=sp`。引数は `LDL -1`(最後の引数)、`LDL -2`…。`ENTER n` でローカル確保(`LDL 0`…)。`RET n`/`RETV n` が引数n個を片付ける

## 命令
スタック `PUSH0 PUSH1 PUSHI8 PUSHV DUP DROP SWAP OVER ROT` / 算術 `ADD SUB MUL DIV MOD NEG AND OR XOR SHL SHR INC DEC NOT MIN MAX ABS` /
固定小数点16.16 `FMUL FDIV` / 比較 `EQ NE LT GT LE GE` / 制御 `JMP JZ JNZ CALL CALLI RET RETV ENTER HALT` /
メモリ `LDL STL LD8 ST8 LD16 ST16 LD ST MEMCPY MEMSET` / `SYS n`, `PUTC c`

## アセンブラ
`label: MNEMONIC operand ; comment`。`PUSH x` は PUSH0/1/I8/V を自動選択。
ディレクティブ `.equ .byte .word .space .fill .org .str .entry`。式は `( ) * / + - << >> & |` と文字リテラル `'A'`、ラベル。

## SYS(引数は左から積む=最後の引数がスタックトップ)
| n | 名前 | 引数 → 結果 |
|---|---|---|
| 0 / 1 / 2 | PUTC / PUTI / GETC | c / n / → byte(EOFは-1) |
| 10 | WIN | w,h → id(作成して選択) |
| 11 / 12 / 13 | WSEL / WCLOSE / WTITLE | id / id / str |
| 14 | GETPIXEL | x,y → c(範囲外は-1) |
| 15 | BLIT | x,y,w,h,ptr,key(keyと同じ色は透過、256以上で透過なし) |
| 16 / 17 | CIRCLE / FCIRCLE | x,y,r,c |
| 18 | RECTO | x,y,w,h,c(枠のみ) |
| 20–24 | PIXEL / RECT / LINE / TEXT / CLEAR | (x,y,c) / (x,y,w,h,c) / (x0,y0,x1,y1,c) / (x,y,str,c) / (c) |
| 25 | PAL | i,rgb |
| 26 | FLIP | 選択ウィンドウを画面更新 |
| 30 | POLL | → win,type,a,b(bが最上段) |
| 31 / 32 | TICK / SLEEP | → ms / ms |
| 33 | RND | n → 0..n-1 |
| 34 | MOUSE | → win,x,y,buttons(bit0=左 bit1=中 bit2=右) |
| 35 | KEY | code → 0/1(押下中。コードはイベントと同じ) |
| 40 / 41 | SIN / COS | 度 → 16.16固定小数点 |
| 42 | SQRT | n → 整数平方根 |
| 50 | BEEP | freq,ms(矩形波、即座に戻る) |
| 99 | EXIT | code(プロセスの終了コードになる) |

イベント type: 0なし 1終了 2キー(a=コード,b=押下) 3移動(a=x,b=y) 4/5マウス押/離 6ウィンドウ閉(winに対象)。
キーはASCIIはそのまま、特殊キーは 128+scancode(→207 ←208 ↓209 ↑210)。色は8bitパレット(0–15 ANSI、16–231 色立方体、232–255 グレー)。

## サンプル(`examples/`)
| ファイル | 内容 | 使っている機能 |
|---|---|---|
| `hello` | Hello, World! | `CALL` / `LD8` / `.str` |
| `fib` | フィボナッチ数列20個 | ローカル変数、`PUTI` |
| `fact` | 再帰階乗 1!〜10! | `CALL` / `RETV` |
| `primes` | 100未満の素数(篩) | `MEMSET` / `LD8` / `ST8` |
| `fixed` | 固定小数点と数学関数 | `FMUL` / `FDIV` / `SIN` / `COS` / `SQRT` / `ABS` |
| `echo` | 標準入力を大文字にして出力、行数を終了コードに | `GETC` / `EXIT` |
| `music` | ドレミファソラシド | `BEEP` |
| `lines` | 256色の放射線 | `LINE` |
| `bounce` | 跳ね返る四角 | `FLIP` / `SLEEP` / イベントループ |
| `paint` | マウスで描く(1-9で色、cで消去) | マウス/キーイベント、`RECT` |
| `circles` | 12個の点が回る | `CIRCLE` / `FCIRCLE` / `SIN` / `COS` |
| `sprite` | 透過スプライトのスタンプ(右ボタンで消去) | `BLIT` / `MOUSE` |
| `stopwatch` | アナログのストップウォッチ | `LINE` / `TICK` / `SIN` / `COS` |
| `counter` | 経過秒とフレーム数を2ウィンドウで表示 | 複数ウィンドウ、アセンブリ内itoa |
| `win2` | 2ウィンドウでマウス追従 | `WSEL` / `POLL` |

## テスト
`make test`(または `sh tests/run.sh`)で、全サンプルのアセンブル、コンソール出力の比較、標準入力と終了コード、逆アセンブラ、
switchディスパッチ版、擬似イベント(`tests/gui_events.c`)によるヘッドレスGUIテスト、GUI/音サンプルの起動確認(`SDL_VIDEODRIVER=dummy`)を実行します。

## 移植
`gui.h` の関数を実装すれば Win32/X11/Wasm 等へ差し替え可能。`TVM_DUMP=prefix` で FLIP ごとに PPM を書き出せる(ヘッドレステスト用)。
