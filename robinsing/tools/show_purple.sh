#!/bin/sh
# 展示候选紫色，方便挑选主色。
# 用法: sh tools/show_purple.sh
#
# 每行: --color 编号 / ANSI 参数 / RGB / 染色样例 / 说明
# 选好后改 src/warble.h 里的 WARBLE_PURPLE 即可。

row() {
    n=$1      # --color 接受的 256 色编号
    rgb=$2
    note=$3
    printf '  %3s  38;5;%-3s rgb(%-14s) ' "$n" "$n" "$rgb"
    printf '\033[38;5;%smSway to My Beat in Cosmos\033[0m' "$n"
    printf '  %s\n' "$note"
}

row16() {
    params=$1
    rgb=$2
    note=$3
    printf '  %-4s %-9s rgb(%-14s) ' "std" "$params" "$rgb"
    printf '\033[%smSway to My Beat in Cosmos\033[0m' "$params"
    printf '  %s\n' "$note"
}

echo "标准 16 色里的洋红（没有 256 色编号）："
row16 '35'    '128,0,128' 'ANSI magenta（各终端略有差异）'
row16 '95'    '255,0,255' 'ANSI bright magenta'
row16 '1;35'  '128,0,128' 'bold magenta'

echo
echo "饱和紫（xterm 256）："
row '53'  '95,0,95' '<-- 当前使用'
row '54'  '95,0,135'
row '55'  '95,0,175'
row '56'  '95,0,215'
row '57'  '95,0,255'
row '90'  '135,0,135'
row '91'  '135,0,175'
row '92'  '135,0,215'
row '93'  '135,0,255'
row '127' '175,0,175'
row '128' '175,0,215'
row '129' '175,0,255'
row '164' '215,0,215'
row '165' '215,0,255'
row '200' '255,0,215'
row '201' '255,0,255'

echo
echo "柔和紫（xterm 256）："
row '99'  '135,95,255'
row '105' '135,135,255'
row '111' '135,175,255'
row '135' '175,95,255'
row '141' '175,135,255'
row '147' '175,175,255'
row '171' '215,95,255'
row '177' '215,135,255'
row '183' '215,175,255'
row '207' '255,95,255'
row '213' '255,135,255'
row '219' '255,175,255'

echo
echo "说明: 编号 N 对应转义码 \\033[38;5;Nm；24 位色可用 \\033[38;2;R;G;Bm。"
