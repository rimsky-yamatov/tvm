# tinyvm — 小さくて速い、C99のスタックVM(GUI・音・ファイルI/O・C風言語つき)

```
make                                    # 要 SDL2 と libm。tvm / tasm / tdis / tvc ができる
./tasm examples/hello.tas && ./tvm examples/hello.tvb
./tvc examples/tc/hello.tc && ./tasm examples/tc/hello.tas && ./tvm examples/tc/hello.tvb
./tdis examples/hello.tvb               # 逆アセンブル
make test                               # 全サンプルとテスト
```

## ツール
| コマンド | 役割 |
|---|---|
| `tvm [-fs DIR] prog.tvb` | VM本体。`-fs DIR` でそのディレクトリ配下のファイルI/Oを許可 |
| `tasm in.tas [out.tvb]` | アセンブラ(`.tas` → `.tvb`) |
| `tvc in.tc [out.tas]` | C風言語のコンパイラ(`.tc` → `.tas`)。言語仕様は [LANGUAGE.md](LANGUAGE.md) |
| `tdis file.tvb [start [end]]` | 逆アセンブラ |

拡張子は、C風ソースが `.tc`、アセンブリが `.tas`、バイトコードが `.tvb`。

## ビルド済みバイナリとCI
GitHub Actions(`.github/workflows/build.yml`)が Linux / macOS / Windows でビルドとテストを行い、成果物(`tvm` `tasm` `tdis` `tvc` + examples)をアーティファクトとして保存します。
`v1.0.0` のような `v*` タグをpushすると、各OS向けのアーカイブがReleasesに自動で公開されます。

```
git tag v1.0.0 && git push origin v1.0.0
```

- Linux / macOS: `tar xzf tvm-linux-x86_64.tar.gz` のあと、SDL2が必要(`apt install libsdl2-2.0-0` / `brew install sdl2`)
- Windows: zipを展開するだけ(`SDL2.dll` 同梱)
- 自分でビルドする場合: Linux/macOS は `make`、Windows は MSYS2 の MINGW64 シェルで `pacman -S make mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2` のあと `make`

## 構成
| ファイル | 役割 |
|---|---|
| `tvm.h` | 命令表(X-macro)、ファイルヘッダ定義。VM・アセンブラ・逆アセンブラで共有 |
| `vm.c` | VMコア(libc不使用、確保なし、computed goto / switch自動切替) |
| `main.c` | ホスト: 読込、`SYS`実装(コンソール、GUI、音、数学、ファイル) |
| `gui.h` / `gui_sdl.c` | GUIバックエンド境界 / SDL2実装(最大8ウィンドウ) |
| `tasm.c` / `tdis.c` / `tvc.c` | アセンブラ / 逆アセンブラ / C風言語コンパイラ |

## ファイル形式
`[0xB1][開始アドレス ULEB128][イメージ…]`(通常ヘッダ2バイト)。イメージは0番地にロードされ、開始アドレスから実行。
アセンブラは `.entry` 指定、なければラベル `main`、なければ0を開始アドレスにする。`0xB1` で始まらないファイルはヘッダなしの生イメージとして0番地から実行。
イメージ末尾のゼロバイトは保存されません(VMのメモリは0で始まるため同じ動作になります)。そのため、ゼロ初期化の配列はファイルサイズに影響しません。

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
| 60 | FOPEN | path,mode → h / -1(mode: 0読 1書 2追記 3読書) |
| 61 | FCLOSE | h → 0 / -1 |
| 62 / 63 | FREAD / FWRITE | h,ptr,n → バイト数 / -1 |
| 64 / 65 | FGETC / FPUTC | h → byte / -1(EOF) ・ h,c → c / -1 |
| 66 / 67 | FSEEK / FSIZE | h,pos → 0 / -1 ・ h → バイト数 / -1 |
| 68 | FREMOVE | path → 0 / -1 |
| 99 | EXIT | code(プロセスの終了コードになる) |

イベント type: 0なし 1終了 2キー(a=コード,b=押下) 3移動(a=x,b=y) 4/5マウス押/離 6ウィンドウ閉(winに対象)。
キーはASCIIはそのまま、特殊キーは 128+scancode(→207 ←208 ↓209 ↑210)。色は8bitパレット(0–15 ANSI、16–231 色立方体、232–255 グレー)。

### ファイルI/Oのサンドボックス
`tvm -fs DIR` を付けたときだけ有効で、付けなければ全ての呼び出しが -1 を返します。DIR配下の相対パスだけが使え、絶対パス・`..`・`\`・`:` を含むパスは拒否されます。同時に開けるのは8ファイル。
シンボリックリンクの先まではチェックしないため、信頼できないバイトコードを動かすときは DIR を空の専用ディレクトリにしてください。

## サンプル(`examples/`)
アセンブリ(`examples/*.tas`):

| ファイル | 内容 | 使っている機能 |
|---|---|---|
| `hello` | Hello, World! | `CALL` / `LD8` / `.str` |
| `fib` | フィボナッチ数列20個 | ローカル変数、`PUTI` |
| `fact` | 再帰階乗 1!〜10! | `CALL` / `RETV` |
| `primes` | 100未満の素数(篩) | `MEMSET` / `LD8` / `ST8` |
| `fixed` | 固定小数点と数学関数 | `FMUL` / `FDIV` / `SIN` / `COS` / `SQRT` / `ABS` |
| `echo` | 標準入力を大文字にして出力、行数を終了コードに | `GETC` / `EXIT` |
| `files` | 書く→読む→`..`の拒否→削除 | `FOPEN` 〜 `FREMOVE`(`-fs` 必須) |
| `music` | ドレミファソラシド | `BEEP` |
| `lines` `bounce` `paint` | 放射線 / 跳ね返る四角 / お絵かき | `LINE` / アニメーション / マウス・キー |
| `circles` `sprite` `stopwatch` | 回る点 / 透過スプライト / アナログ時計 | `CIRCLE` `SIN` `COS` / `BLIT` `MOUSE` / `TICK` |
| `counter` `win2` | 2ウィンドウ(経過秒とフレーム数 / マウス追従) | 複数ウィンドウ、アセンブリ内itoa |

C風言語(`examples/tc/*.tc`): `hello` `fizzbuzz` `primes` `sort`(クイックソート)`files` `bounce` `mandelbrot`(固定小数点)`life`(ライフゲーム)`paint`

## テスト
`make test`(または `sh tests/run.sh`)で、全サンプルのアセンブルとコンパイル、出力と終了コードの比較、ファイルI/Oのサンドボックス、逆アセンブラ、
switchディスパッチ版、コンパイラのエラーメッセージ、擬似イベント(`tests/gui_events.c`)によるヘッドレスGUIテスト、GUI/音サンプルの起動確認(`SDL_VIDEODRIVER=dummy`)を実行します。

## 移植
`gui.h` の関数を実装すれば Win32/X11/Wasm 等へ差し替え可能。`TVM_DUMP=prefix` で FLIP ごとに PPM を書き出せる(ヘッドレステスト用)。
