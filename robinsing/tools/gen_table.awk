# 由 lyrics 目录下的歌词生成 warble_table.h
# 用法: LC_ALL=C awk -f tools/gen_table.awk lyrics/*.txt
#
# 元数据（# 开头，顺序随意）：
#   # name: sway
#   # title: Sway to My Beat in Cosmos
#
# 段落标记（可和别的标记/正文同行，按出现顺序生效）：
#   [cstart] ... [cend]   高潮段
#   [estart] ... [eend]   尾声
# 段落标记不输出；段内的行既属于整首，也单独组成该段。
#
# 播放动作标记（会被抽成单独的动作行，歌词里不显示）：
#   [sleep]      停顿 1 秒
#   [shortsleep] 停顿 0.5 秒
#   [next]       输出一个换行
#
# 生成规则：
#   - 一行去掉标记后的文字如果非空，先输出这一行；
#   - 同一行里的动作标记按顺序拼成 "[sleep][next]" 这样的动作行，排在文字之后；
#   - 动作行整行只由动作标记组成，C 侧据此执行，不当作歌词。
# name 缺省取文件名；title 必须写。每行只能有一句话（连续的 . ? ! 算一处）。

# 文件名去掉目录和扩展名：lyrics/sway.txt -> sway
function slug(path,   s)
{
    s = path
    sub(/^.*\//, "", s)
    sub(/\.[^.]*$/, "", s)
    return s
}

# sway -> SWAY；非字母数字换成下划线
function symbol(name,   s)
{
    s = toupper(name)
    gsub(/[^A-Z0-9]/, "_", s)
    return s
}

# 去掉首尾空白
function trim(s)
{
    gsub(/^[[:space:]]+/, "", s)
    gsub(/[[:space:]]+$/, "", s)
    return s
}

# 把字面量转义成 C 字符串内容
function cstr(s,   out, i, c)
{
    out = ""
    for (i = 1; i <= length(s); i++) {
        c = substr(s, i, 1)
        if (c == "\\" || c == "\"")
            out = out "\\"
        out = out c
    }
    return out
}

# 一行里有几处句末标点（连续的 . ? ! 算一处）
function sentences(s,   i, c, n, prev)
{
    n = 0
    prev = 0
    for (i = 1; i <= length(s); i++) {
        c = substr(s, i, 1)
        if (c == "." || c == "?" || c == "!") {
            if (!prev) n++
            prev = 1
        } else {
            prev = 0
        }
    }
    return n
}

function blank(s)
{
    return s ~ /^[[:space:]]*$/
}

function runtime_len(rest)
{
    if (substr(rest, 1, 7) == "[sleep]") return 7
    if (substr(rest, 1, 12) == "[shortsleep]") return 12
    if (substr(rest, 1, 6) == "[next]") return 6
    return 0
}

function section_len(rest)
{
    if (substr(rest, 1, 8) == "[cstart]") return 8
    if (substr(rest, 1, 6) == "[cend]") return 6
    if (substr(rest, 1, 8) == "[estart]") return 8
    if (substr(rest, 1, 6) == "[eend]") return 6
    return 0
}

BEGIN {
    print "/* 由 lyrics 目录下的歌词生成，勿手改 */"
    print ""
}

FNR == 1 {
    files[++nfiles] = FILENAME
}

/^[[:space:]]*$/ { next }

/^[[:space:]]*#/ {
    meta = trim(substr($0, index($0, "#") + 1))

    if (meta ~ /^name:/) {
        sub(/^name:[[:space:]]*/, "", meta)
        name[FILENAME] = meta
    } else if (meta ~ /^title:/) {
        sub(/^title:[[:space:]]*/, "", meta)
        title[FILENAME] = meta
    }
    next
}

{
    f = FILENAME
    line = $0
    sub(/\r$/, "", line)

    was_chorus = inchorus[f]
    was_ending = inending[f]
    text = ""
    acts = ""
    had_section = 0

    n = length(line)
    i = 1
    while (i <= n) {
        rest = substr(line, i)

        tok_len = runtime_len(rest)
        if (tok_len > 0) {
            acts = acts substr(rest, 1, tok_len)
            sub(/[[:space:]]+$/, "", text)
            i += tok_len
            continue
        }

        tok_len = section_len(rest)
        if (tok_len > 0) {
            tok = substr(rest, 1, tok_len)
            had_section = 1

            if (tok == "[cstart]") {
                if (inending[f]) {
                    printf "gen_table: %s 第 %d 行：高潮不能嵌在尾声里\n", f, FNR > "/dev/stderr"
                    bad = 1
                } else if (inchorus[f] || chorus_done[f]) {
                    printf "gen_table: %s 第 %d 行：一首歌只能有一段高潮\n", f, FNR > "/dev/stderr"
                    bad = 1
                } else {
                    inchorus[f] = 1
                }
            } else if (tok == "[cend]") {
                if (!inchorus[f]) {
                    printf "gen_table: %s 第 %d 行：[cend] 前面没有 [cstart]\n", f, FNR > "/dev/stderr"
                    bad = 1
                } else {
                    inchorus[f] = 0
                    chorus_done[f] = 1
                }
            } else if (tok == "[estart]") {
                if (inchorus[f]) {
                    printf "gen_table: %s 第 %d 行：尾声不能嵌在高潮里\n", f, FNR > "/dev/stderr"
                    bad = 1
                } else if (inending[f] || ending_done[f]) {
                    printf "gen_table: %s 第 %d 行：一首歌只能有一段尾声\n", f, FNR > "/dev/stderr"
                    bad = 1
                } else {
                    inending[f] = 1
                }
            } else if (tok == "[eend]") {
                if (!inending[f]) {
                    printf "gen_table: %s 第 %d 行：[eend] 前面没有 [estart]\n", f, FNR > "/dev/stderr"
                    bad = 1
                } else {
                    inending[f] = 0
                    ending_done[f] = 1
                }
            }

            sub(/[[:space:]]+$/, "", text)
            i += tok_len
            continue
        }

        text = text substr(line, i, 1)
        i++
    }

    if (had_section) sub(/^[[:space:]]+/, "", text)

    if (!blank(text)) {
        if (sentences(text) > 1) {
            printf "gen_table: %s 第 %d 行有多句话（一行只能一句）\n", f, FNR > "/dev/stderr"
            bad = 1
        }
        ntext[f, ++count[f]] = text
        if (was_chorus) nchorus[f, ++ccount[f]] = text
        if (was_ending) nending[f, ++ecount[f]] = text
    }

    if (acts != "") {
        ntext[f, ++count[f]] = acts
        if (was_chorus) nchorus[f, ++ccount[f]] = acts
        if (was_ending) nending[f, ++ecount[f]] = acts
    }
}

END {
    for (i = 1; i <= nfiles; i++) {
        f = files[i]

        if (inchorus[f]) {
            printf "gen_table: %s 的 [cstart] 没有 [cend]\n", f > "/dev/stderr"
            bad = 1
            continue
        }
        if (inending[f]) {
            printf "gen_table: %s 的 [estart] 没有 [eend]\n", f > "/dev/stderr"
            bad = 1
            continue
        }
        if (name[f] == "") name[f] = slug(f)
        if (name[f] in seen) {
            printf "gen_table: name 重复：%s（%s）\n", name[f], f > "/dev/stderr"
            bad = 1
            continue
        }
        seen[name[f]] = 1

        if (count[f] == 0) {
            printf "gen_table: %s 没有歌词\n", f > "/dev/stderr"
            bad = 1
            continue
        }
        if (title[f] == "") {
            printf "gen_table: %s 缺少 \"# title: ...\"\n", f > "/dev/stderr"
            bad = 1
            continue
        }
        if (chorus_done[f] && ccount[f] == 0) {
            printf "gen_table: %s 的高潮段是空的\n", f > "/dev/stderr"
            bad = 1
            continue
        }
        if (ending_done[f] && ecount[f] == 0) {
            printf "gen_table: %s 的尾声是空的\n", f > "/dev/stderr"
            bad = 1
            continue
        }

        printf "static const char *const WARBLE_LINES_%s[] = {\n", symbol(name[f])
        for (j = 1; j <= count[f]; j++)
            printf "    \"%s\",\n", cstr(ntext[f, j])
        print "};"
        print ""

        if (ccount[f] > 0) {
            printf "static const char *const WARBLE_CHORUS_%s[] = {\n", symbol(name[f])
            for (j = 1; j <= ccount[f]; j++)
                printf "    \"%s\",\n", cstr(nchorus[f, j])
            print "};"
            print ""
        }

        if (ecount[f] > 0) {
            printf "static const char *const WARBLE_END_%s[] = {\n", symbol(name[f])
            for (j = 1; j <= ecount[f]; j++)
                printf "    \"%s\",\n", cstr(nending[f, j])
            print "};"
            print ""
        }
    }

    if (bad) exit 1

    print "static const struct warble_song WARBLE_SONGS[] = {"
    for (i = 1; i <= nfiles; i++) {
        f = files[i]
        printf "    { \"%s\", \"%s\", WARBLE_LINES_%s,\n", cstr(name[f]), cstr(title[f]), symbol(name[f])
        printf "      sizeof WARBLE_LINES_%s / sizeof *WARBLE_LINES_%s,\n", symbol(name[f]), symbol(name[f])
        if (ccount[f] > 0)
            printf "      WARBLE_CHORUS_%s, sizeof WARBLE_CHORUS_%s / sizeof *WARBLE_CHORUS_%s,\n", symbol(name[f]), symbol(name[f]), symbol(name[f])
        else
            printf "      NULL, 0,\n"
        if (ecount[f] > 0)
            printf "      WARBLE_END_%s, sizeof WARBLE_END_%s / sizeof *WARBLE_END_%s },\n", symbol(name[f]), symbol(name[f]), symbol(name[f])
        else
            printf "      NULL, 0 },\n"
    }
    print "};"
}
