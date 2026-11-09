# tinyvm — 小さくて速い、C99のスタックVM(複数ウィンドウGUI対応)

```
make                      # 要 SDL2 (libsdl2-dev)。tvm と tasm ができる
./tasm examples/hello.asm hello.bin && ./tvm hello.bin
./tasm examples/win2.asm  win2.bin  && ./tvm win2.bin     # TVM_SCALE=3 でズーム変更
```

## 構成
| ファイル | 役割 |
|---|---|
| `tvm.h` | 命令表(X-macro)。VMとアセンブラで共有 |
| `vm.c` | VMコア(libc不使用、確保なし、computed goto / switch自動切替) |
| `main.c` | ホスト: バイナリ読込、`SYS`実装 |
| `gui.h` / `gui_sdl.c` | GUIバックエンド境界 / SDL2実装(最大8ウィンドウ) |
| `tasm.c` | アセンブラ(可変長エンコードを反復パスで解決) |

## サンプル(`examples/`)
| ファイル | 内容 | 使っている機能 |
|---|---|---|
| `hello.asm` | Hello, World! | `CALL` / `LDL` / `LD8` / `.str` |
| `fib.asm` | フィボナッチ数列20個 | ローカル変数、`PUTI` |
| `fact.asm` | 再帰階乗 1!〜10! | `CALL` / `RETV` |
| `primes.asm` | 100未満の素数(エラトステネスの篩) | `MEMSET` / `LD8` / `ST8` |
| `lines.asm` | 256色パレットの放射線 | `LINE` |
| `bounce.asm` | 跳ね返る四角(アニメーション) | `FLIP` / `SLEEP` / イベントループ |
| `paint.asm` | マウスで描くお絵かき(1-9で色、cで消去) | マウス/キーイベント、`RECT` |
| `counter.asm` | 経過秒とフレーム数を2ウィンドウで表示 | 複数ウィンドウ、アセンブリ内itoa |
| `win2.asm` | 2ウィンドウでマウス追従 | `WSEL` / `POLL` |

## テスト
`make test`(または `sh tests/run.sh`)で、全サンプルのアセンブル、コンソール出力の比較、switchディスパッチ版の確認、
擬似イベント(`tests/gui_events.c`)によるヘッドレスGUIテスト(`SDL_VIDEODRIVER=dummy`)を実行します。

## 実行モデル
- 値は32bit。データスタック1024、メモリ64KB(コードとデータ共有、アドレスは16bitで折り返し)。範囲外アクセスでクラッシュしない
- 0除算は0、`INT_MIN/-1` も安全
- フレーム: `CALL`で `fp=sp`。引数は `LDL -1`(最後の引数)、`LDL -2`…。`ENTER n` でローカル確保(`LDL 0`…)。`RET n`/`RETV n` が引数n個を片付ける
- バイナリは生のイメージ(ヘッダなし)、実行は0番地から

## 命令
スタック `PUSH0 PUSH1 PUSHI8 PUSHV DUP DROP SWAP OVER ROT` / 算術 `ADD SUB MUL DIV MOD NEG AND OR XOR SHL SHR INC DEC NOT MIN MAX` /
比較 `EQ NE LT GT LE GE` / 制御 `JMP JZ JNZ CALL CALLI RET RETV ENTER HALT` /
メモリ `LDL STL LD8 ST8 LD16 ST16 LD ST MEMCPY MEMSET` / `SYS n`, `PUTC c`

アセンブラの `PUSH x` は PUSH0/1/I8/V を自動選択。`.equ .byte .word .space .str` が使える。

## SYS(引数は左から積む=最後の引数がスタックトップ)
| n | 名前 | 引数 → 結果 |
|---|---|---|
| 0/1 | PUTC / PUTI | c / n |
| 10 | WIN | w,h → id(作成して選択) |
| 11 | WSEL | id(以降の描画対象) |
| 12 | WCLOSE | id |
| 13 | WTITLE | str |
| 20–24 | PIXEL / RECT / LINE / TEXT / CLEAR | (x,y,c) / (x,y,w,h,c) / (x0,y0,x1,y1,c) / (x,y,str,c) / (c) |
| 25 | PAL | i,rgb |
| 26 | FLIP | 選択ウィンドウを画面更新 |
| 30 | POLL | → win,type,a,b(bが最上段) |
| 31 / 32 | TICK / SLEEP | → ms / ms |
| 33 | RND | n → 0..n-1 |
| 99 | EXIT | |

イベント type: 0なし 1終了 2キー(a=コード,b=押下) 3移動(a=x,b=y) 4/5マウス押/離 6ウィンドウ閉(winに対象)。
キーはASCIIはそのまま、特殊キーは 128+scancode(→207 ←208 ↓209 ↑210)。色は8bitパレット(0–15 ANSI、16–231 色立方体、232–255 グレー)。

## 移植
`gui.h` の関数を実装すれば Win32/X11/Wasm 等へ差し替え可能。`TVM_DUMP=prefix` で FLIP ごとに PPM を書き出せる(ヘッドレステスト用)。
