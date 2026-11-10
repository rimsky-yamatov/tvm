# C風簡易言語(tvc)

`.tc` のソースを `tvc` が tvm アセンブリ(`.tas`)に変換し、`tasm` が `.tvb` にします。

```
./tvc prog.tc          # → prog.tas  (出力名は第2引数でも指定可)
./tasm prog.tas        # → prog.tvb
./tvm prog.tvb         # ファイルI/Oを使うなら  ./tvm -fs DIR prog.tvb
```

```c
// hello.tc
void main() { puts("Hello, World!"); }
```

## 型と宣言
- `int`(32bit符号付き)、`char`(符号なし8bit。スカラー変数は `int` と同じ)、`int *`、`char *`(どちらもただのアドレス)
- グローバル: `int n = 5;`  `int a[10];`  `int t[] = {1, 2, 3};`  `char s[64];`  `char name[] = "abc";`  `char *msg = "hi";`(初期値は定数式)
- ローカル: `int x;`  `int x = 1, y;`  `char *p = "text";`(ブロック内のどこでも宣言可)
  - ローカル変数は**関数に入るときに一度だけ0で初期化**されます(ループのたびには初期化されません)
- **ローカル配列と `&` 演算子はありません**(スタックはアドレスを持たないため)。配列はグローバルに置いてください
- 配列名は式の中でアドレスになります: `memset(buf, 0, 32)`, `bubble(arr, n)`
- 添字 `p[i]` は変数の型で決まります。`int` の配列/ポインタは4バイト単位、`char` は1バイト単位。`*p` は `p[0]` と同じ。ポインタ演算(`p + 1`)はありません

## 式
優先順位(高い順): 単項 `- + ! ~ *` → `* / %` → `+ -` → `<< >>` → `< > <= >=` → `== !=` → `&` → `^` → `|` → `&&` → `||` → `?:`

- `&&` `||` は短絡評価、比較は 0/1 を返します。`>>` は算術シフト。`/` `%` は0で割ると0
- 数値は10進・16進(`0x1F`)・文字リテラル(`'A'` `'\n'`)。文字列リテラルは NUL 終端のアドレス
- 代入は文です(式ではありません): `x = e;` `a[i] += 2;` `*p = 0;` `x++;` `x--;`
  - `++` `--` は文の中(と `for` の3番目)だけで使えます。式の中では `x += 1` を使ってください

## 文
`if / else`、`while`、`for (init; cond; step)`、`break`、`continue`、`return`、`{ ... }`、式文(代入・関数呼び出し)

## 関数
```c
int add(int a, int b) { return a + b; }     // 引数は左から順に評価、戻り値は int
void show(char *s) { puts(s); }              // void は値を返さない
int main() { ... return 0; }                 // main は引数なし。戻り値がプロセスの終了コード
```
- 前方参照できます(定義順は自由)。再帰もOK
- 呼ばれない関数はバイトコードに含まれません

## プリプロセッサ
`#define NAME トークン...` だけ使えます(定数や式の置き換え)。`#define W 160` / `#define N (W * 2)`

## 組み込み関数(SYS呼び出しに直結)
| 分類 | 関数 |
|---|---|
| 入出力・終了 | `putc(c)` `puti(n)` `getc()` → byte/-1 `exit(code)` `halt()` |
| ウィンドウ | `win(w,h)` → id `wsel(id)` `wclose(id)` `wtitle(str)` |
| 描画 | `clear(c)` `pixel(x,y,c)` `rect(x,y,w,h,c)` `recto(x,y,w,h,c)` `line(x0,y0,x1,y1,c)` `circle(x,y,r,c)` `fcircle(x,y,r,c)` `text(x,y,str,c)` `blit(x,y,w,h,ptr,key)` `getpixel(x,y)` `pal(i,rgb)` `flip()` |
| 入力 | `poll()` `mouse()` `key(code)` |
| 時間・乱数・音 | `tick()` `sleep(ms)` `rnd(n)` `beep(freq,ms)` |
| 数学 | `abs(x)` `min(a,b)` `max(a,b)` `sqrt(n)` `sin(deg)` `cos(deg)`(16.16固定小数点)`fmul(a,b)` `fdiv(a,b)` |
| メモリ | `memset(p,v,n)` `memcpy(dst,src,n)` |
| ファイル(`-fs DIR` 時のみ) | `fopen(path,mode)` `fclose(h)` `fread(h,ptr,n)` `fwrite(h,ptr,n)` `fgetc(h)` `fputc(h,c)` `fseek(h,pos)` `fsize(h)` `fremove(path)` |

- `poll()` はイベントを1つ取り出して種類を返し、`ev_win` `ev_a` `ev_b` に詳細を入れます。種類: 0なし 1終了 2キー(`ev_a`=コード,`ev_b`=押下) 3マウス移動(`ev_a`,`ev_b`=x,y) 4/5マウス押/離 6ウィンドウを閉じる。種類は `ev_type` にも入ります
- `mouse()` は現在の状態を `mouse_win` `mouse_x` `mouse_y` `mouse_btn`(bit0=左 bit1=中 bit2=右)に入れます
- `key(code)` は押されている間 1。コードはASCII、特殊キーは 128+scancode(→207 ←208 ↓209 ↑210)
- 色は8bitパレット(0〜15 ANSI、16〜231 色立方体、232〜255 グレー)
- ファイルの `mode`: 0 読み / 1 書き(作り直し) / 2 追記 / 3 読み書き

## ライブラリ関数(使ったものだけコンパイルされます)
`strlen(s)` `strcmp(a,b)` `print(s)`(改行なし)`puts(s)`(改行あり)`itoa(n, buf)`(10進文字列を `buf` に書く)。同名の関数を自分で定義すれば、そちらが優先されます。

## 制限
ローカル配列なし、`&` なし、ポインタ演算なし、構造体・浮動小数点・`switch`・`do-while` なし、1つの `#define` は1行。エラーは `ファイル名:行: error: メッセージ` で報告され、コンパイルは止まります。
