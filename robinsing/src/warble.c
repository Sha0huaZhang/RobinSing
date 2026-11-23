#include <stdio.h>
#include <string.h>

#include "warble.h"
#include "platform.h"
#include "warble_table.h"     /* 由 lyrics.txt 生成 */

/* 换行停顿占几个字符的时间 */
#define NAP_LINE_UNITS 6

/* 最后一行唱完再停几个字符的时间才结束 */
#define LAST_HOLD_UNITS 2

/* [sleep] 1 秒，[shortsleep] 半秒 */
#define SLEEP_MS 1000L

static const char *lyric_color = WARBLE_PURPLE;

void warble_set_color(const char *escape)
{
    lyric_color = (escape != NULL) ? escape : "";
}

/* 一行里实际输出什么：
   - ',' 始终不输出；'.' 只有出现在行尾的 .?! 串里才输出；
   - 行尾空白去掉；结尾是 . ? ! 时原样输出且不再补 ~；
   - 其它行统一在 ~ 前留一个空格。 */
struct visible_span {
    size_t run_start;  /* 行尾 .?! 串的起点 */
    size_t end;        /* 最后一个可见（非空格）字符之后的位置 */
    int has_end;       /* 行尾有 . ? ! */
};

static void scan_visible(const char *line, struct visible_span *v)
{
    size_t len = strlen(line);
    size_t i, last = 0;

    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t'))
        len--;

    v->run_start = len;
    while (v->run_start > 0 &&
           (line[v->run_start - 1] == '.' || line[v->run_start - 1] == '?' ||
            line[v->run_start - 1] == '!'))
        v->run_start--;
    v->has_end = (v->run_start < len);

    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)line[i];

        if (c == ',') continue;
        if (c == '.' && i < v->run_start) continue;
        if (c != ' ' && c != '\t')
            last = i + 1;
    }
    v->end = last;
}

enum action { A_NONE = 0, A_SLEEP, A_SHORT_SLEEP, A_NEXT };

static enum action action_at(const char *p, int *len)
{
    if (strncmp(p, "[sleep]", 7) == 0) { *len = 7; return A_SLEEP; }
    if (strncmp(p, "[shortsleep]", 12) == 0) { *len = 12; return A_SHORT_SLEEP; }
    if (strncmp(p, "[next]", 6) == 0) { *len = 6; return A_NEXT; }
    return A_NONE;
}

/* 生成器把同一行的动作标记抽成了单独一行，整行只由这些标记组成 */
static int is_action_line(const char *line)
{
    int len;

    if (*line != '[') return 0;
    while (*line) {
        if (action_at(line, &len) == A_NONE) return 0;
        line += len;
    }
    return 1;
}

static int utf8_len(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x06) return 2;
    if ((c >> 4) == 0x0e) return 3;
    if ((c >> 3) == 0x1e) return 4;
    return 1;
}

/* 按截止时间推进一个字符。系统 sleep 总会多睡一点点（macOS 上约 20%），
   盯着绝对时间点睡，误差就不会一行行累积 */
static void pace(long long *deadline, long ms)
{
    long long left;

    *deadline += ms;
    left = *deadline - plat_now_ms();
    if (left > 0)
        plat_nap((long)left);
}

/* 一行的收尾方式 */
struct line_plan {
    int  tildes;     /* 行尾补几个 ~ */
    long hold_ms;    /* 换行前静默多久 */
    long gap_ms;     /* 换行后静默多久 */
};

/* 行尾的 ~ 表示拖腔，每个 ~ 停留一个字符的时间，然后直接换行 */
static void sing_line(const char *line, long char_ms,
                      const struct line_plan *plan, long long *deadline)
{
    struct visible_span v;
    size_t i = 0;
    int tildes = plan->tildes;

    scan_visible(line, &v);

    while (i < v.end) {
        unsigned char c = (unsigned char)line[i];
        int n;

        if (c == ',') { i++; continue; }
        if (c == '.' && i < v.run_start) { i++; continue; }

        n = utf8_len(c);
        fwrite(line + i, 1, (size_t)n, stdout);
        fflush(stdout);
        i += (size_t)n;
        pace(deadline, char_ms);
    }

    if (!v.has_end) {
        if (v.end > 0) {
            putchar(' ');
            fflush(stdout);
        }
        while (tildes-- > 0) {
            putchar('~');
            fflush(stdout);
            pace(deadline, char_ms);
        }
    }
    if (plan->hold_ms > 0)
        pace(deadline, plan->hold_ms);
    putchar('\n');
    fflush(stdout);
    if (plan->gap_ms > 0)
        pace(deadline, plan->gap_ms);
}

/* 选整首、高潮还是尾声；那一段没标记就退回整首 */
static const char *const *pick_lines(const struct warble_song *song,
                                     enum warble_part part, size_t *count)
{
    if (part == WARBLE_PART_CHORUS && song->chorus_count > 0) {
        *count = song->chorus_count;
        return song->chorus;
    }
    if (part == WARBLE_PART_END && song->ending_count > 0) {
        *count = song->ending_count;
        return song->ending;
    }
    *count = song->count;
    return song->lines;
}

/* 按顺序执行动作；plain 为真时只保留换行，不做停顿 */
static void run_actions(const char *line, long long *deadline, int plain)
{
    int len;

    while (*line) {
        switch (action_at(line, &len)) {
        case A_SLEEP:
            if (!plain) pace(deadline, SLEEP_MS);
            break;
        case A_SHORT_SLEEP:
            if (!plain) pace(deadline, SLEEP_MS / 2);
            break;
        case A_NEXT:
            putchar('\n');
            fflush(stdout);
            break;
        default:
            return;
        }
        line += len;
    }
}

static void sing_lines(const char *const *lines, size_t count, long char_ms)
{
    size_t i;
    size_t last = count;          /* 最后一条真正会唱的行 */
    long long deadline;

    for (i = 0; i < count; i++)
        if (!is_action_line(lines[i]))
            last = i;
    if (last == count)            /* 没有歌词，只有动作 */
        return;

    deadline = plat_now_ms();
    if (*lyric_color)
        fputs(lyric_color, stdout);     /* 歌词主色 */
    for (i = 0; i < count; i++) {
        int is_last;
        struct line_plan plan;

        if (is_action_line(lines[i])) {
            run_actions(lines[i], &deadline, 0);
            continue;
        }

        is_last = (i == last);
        plan.tildes = is_last ? 2 : 1;
        plan.hold_ms = is_last ? char_ms * LAST_HOLD_UNITS : 0;
        plan.gap_ms = char_ms * NAP_LINE_UNITS;   /* 最后一行也算一次换行，只是不再真的换行 */
        sing_line(lines[i], char_ms, &plan, &deadline);
    }
    if (*lyric_color)
        fputs(WARBLE_RESET, stdout);
    fflush(stdout);
}

static void plain_lines(const char *const *lines, size_t count)
{
    size_t i;
    size_t last = count;          /* 最后一条真正会输出的行 */

    for (i = 0; i < count; i++)
        if (!is_action_line(lines[i]))
            last = i;
    if (last == count)
        return;

    for (i = 0; i < count; i++) {
        struct visible_span v;
        size_t j = 0;

        if (is_action_line(lines[i])) {
            run_actions(lines[i], NULL, 1);
            continue;
        }
        if (i > last)
            continue;

        scan_visible(lines[i], &v);
        while (j < v.end) {
            unsigned char c = (unsigned char)lines[i][j];
            int n;

            if (c == ',') { j++; continue; }
            if (c == '.' && j < v.run_start) { j++; continue; }

            n = utf8_len(c);
            fwrite(lines[i] + j, 1, (size_t)n, stdout);
            j += (size_t)n;
        }

        if (!v.has_end) {
            if (v.end > 0)
                putchar(' ');
            putchar('~');
            if (i == last)
                putchar('~');
        }
        putchar('\n');
    }
}

void warble_sing(const struct warble_song *song, enum warble_part part,
                 long char_ms)
{
    size_t count;

    sing_lines(pick_lines(song, part, &count), count, char_ms);
}

void warble_plain(const struct warble_song *song, enum warble_part part)
{
    size_t count;
    const char *const *lines = pick_lines(song, part, &count);

    plain_lines(lines, count);
}

/* 命令别名：数字、简短名、完整标题。完整标题也支持唯一前缀，如 sway-t、if-i */
static const struct song_alias {
    const char *alias;
    const char *song;
} ALIASES[] = {
    { "1", "sway" },
    { "sway", "sway" },
    { "sway-the-beat-in-cosmos", "sway" },
    { "2", "if" },
    { "if", "if" },
    { "if-i-can-stop-one-heart-from-breaking", "if" },
    { "3", "hope" },
    { "hope", "hope" },
    { "hope-is-the-thing-with-feathers", "hope" },
    { "4", "had" },
    { "had", "had" },
    { "had-i-not-seen-the-sun", "had" },
};

static int eq_nocase(const char *a, const char *b)
{
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;

        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
        if (ca != cb) return 0;
    }
    return *a == *b;
}

/* text 是否以 prefix 开头（都不区分大小写） */
static int starts_with_nocase(const char *text, const char *prefix)
{
    while (*prefix) {
        unsigned char a = (unsigned char)*text++;
        unsigned char b = (unsigned char)*prefix++;

        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

static const struct warble_song *song_by_name(const char *name)
{
    size_t i;

    for (i = 0; i < sizeof WARBLE_SONGS / sizeof *WARBLE_SONGS; i++)
        if (eq_nocase(WARBLE_SONGS[i].name, name))
            return &WARBLE_SONGS[i];
    return NULL;
}

enum warble_lookup warble_find(const char *arg, const struct warble_song **out)
{
    const struct warble_song *song;
    const struct warble_song *match = NULL;
    size_t i;

    if (arg[0] == '\0')
        return WARBLE_UNKNOWN;

    /* 完整名字优先：if 不会因为同时是长标题的前缀而歧义 */
    for (i = 0; i < sizeof ALIASES / sizeof *ALIASES; i++) {
        if (!eq_nocase(ALIASES[i].alias, arg)) continue;
        song = song_by_name(ALIASES[i].song);
        if (song == NULL) return WARBLE_UNKNOWN;
        *out = song;
        return WARBLE_FOUND;
    }
    song = song_by_name(arg);
    if (song != NULL) {
        *out = song;
        return WARBLE_FOUND;
    }

    /* 没有完整匹配，再按唯一前缀找：s -> sway，h -> hope / had 不唯一 */
    for (i = 0; i < sizeof ALIASES / sizeof *ALIASES; i++) {
        if (!starts_with_nocase(ALIASES[i].alias, arg)) continue;
        song = song_by_name(ALIASES[i].song);
        if (song == NULL) continue;
        if (match == NULL) match = song;
        else if (match != song) return WARBLE_AMBIGUOUS;
    }
    for (i = 0; i < sizeof WARBLE_SONGS / sizeof *WARBLE_SONGS; i++) {
        if (!starts_with_nocase(WARBLE_SONGS[i].name, arg)) continue;
        if (match == NULL) match = &WARBLE_SONGS[i];
        else if (match != &WARBLE_SONGS[i]) return WARBLE_AMBIGUOUS;
    }

    if (match == NULL) return WARBLE_UNKNOWN;
    *out = match;
    return WARBLE_FOUND;
}

size_t warble_song_count(void)
{
    return sizeof WARBLE_SONGS / sizeof *WARBLE_SONGS;
}

const struct warble_song *warble_song_at(size_t i)
{
    return &WARBLE_SONGS[i];
}
