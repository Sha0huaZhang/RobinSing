#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "version.h"
#include "warble.h"

#define CHAR_MS_DEFAULT 50L
#define CHAR_MS_FAST    15L
#define CHAR_MS_MAX     60000L

static void usage(FILE *out, const char *prog)
{
    fprintf(out,
            "用法: %s [曲目] [选项]\n"
            "\n"
            "曲目（不区分大小写，可只写唯一前缀；不写就进入选歌菜单）:\n"
            "  1 | sway | sway-the-beat-in-cosmos\n"
            "  2 | if   | if-i-can-stop-one-heart-from-breaking\n"
            "  3 | hope | hope-is-the-thing-with-feathers\n"
            "  4 | had  | had-i-not-seen-the-sun\n"
            "\n"
            "选项:\n"
            "  -s, --speed <时长>  每字符停顿。单位 ms 或 s，不写单位按 ms\n"
            "                      例: -s 50ms、-s=1.2s、-s500（默认 %ldms）\n"
            "  -f, --fast          加速，等同 -s %ldms\n"
            "  -p, --plain         强制纯文本输出，无颜色无停顿\n"
            "  -c, --chorus, --climax  只唱高潮段（没标记就唱整首）\n"
            "  -e, --end, --ends       只唱尾声（没标记就唱整首）\n"
            "  -C, --color <编号>      256 色编号（默认 53）；NO_COLOR / TERM=dumb 时不上色\n"
            "  --                      后面的参数都当作曲目，不再解析选项\n"
            "  -v, --version       显示版本\n"
            "  -h, --help          显示本帮助\n"
            "\n"
            "带选项时：输出到终端就逐字唱，被重定向或接管道就输出纯文本。\n"
            "注意: -s 后面的值会当作速度；要同时指定曲目写成 -s 1 1 或 -s=1 1。\n"
            "颜色也可以用环境变量 ROBINSING_COLOR=53 指定。\n",
            prog, CHAR_MS_DEFAULT, CHAR_MS_FAST);
}

/* 曲目名统一转小写；argv 的字符串可改 */
static void to_lower(char *s)
{
    for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z')
            *s = (char)(*s - 'A' + 'a');
}

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

/* "50" / "50ms" / "1.2s" -> 毫秒。返回 0 成功，-1 格式错，-2 超范围 */
static int parse_speed(const char *text, long *out_ms)
{
    char *end = NULL;
    double scale = 1.0;
    double value = strtod(text, &end);

    if (end == text) return -1;                        /* 没有数字 */
    if (!(value > 0.0) || value > 1e9) return -1;       /* 负数、NaN、离谱的大数 */

    if (*end != '\0') {
        if (eq_nocase(end, "ms")) scale = 1.0;
        else if (eq_nocase(end, "s")) scale = 1000.0;
        else return -1;                                /* 不认识的单位 */
    }

    value *= scale;
    if (value < 1.0 || value > (double)CHAR_MS_MAX) return -2;
    *out_ms = (long)(value + 0.5);
    return 0;
}

static char color_escape[32];
static const char *g_color = "";

/* 0~255 的 256 色编号 */
static int parse_color(const char *text, long *out)
{
    char *end = NULL;
    long n = strtol(text, &end, 10);

    if (end == text || *end != '\0' || n < 0 || n > 255)
        return -1;
    *out = n;
    return 0;
}

/* NO_COLOR / TERM=dumb 完全不上色；只支持 16 色的终端退回洋红 */
static void setup_color(long index)
{
    const char *term = getenv("TERM");

    if (getenv("NO_COLOR") != NULL ||
        (term != NULL && (term[0] == '\0' || strcmp(term, "dumb") == 0))) {
        g_color = "";
        return;
    }

    if (term != NULL && strstr(term, "256color") == NULL)
        strcpy(color_escape, "\033[35m");
    else
        snprintf(color_escape, sizeof color_escape, "\033[38;5;%ldm", index);

    g_color = color_escape;
}

/* 颜色只在终端里用；重定向 / -p / 不支持颜色时输出纯文本 */
static int want_color(int force_plain)
{
    return !force_plain && plat_is_tty() && g_color[0] != '\0';
}

/* 先报歌名，再按 Columbula 的规则唱；歌名和歌词用同一主色 */
static void play_song(const struct warble_song *song, long char_ms,
                      int force_plain, enum warble_part part)
{
    if (want_color(force_plain)) {
        printf("%s" WARBLE_BOLD "%s" WARBLE_UNBOLD "\n", g_color, song->title);
        fflush(stdout);
        plat_vt_on();
        warble_set_color(g_color);
        warble_sing(song, part, char_ms);
    } else {
        printf("%s\n", song->title);
        warble_plain(song, part);
    }
}

/* 不认识的曲目：不报用法，给一句 Robin 的台词 */
static void unknown_song(int force_plain)
{
    int color = want_color(force_plain);

    if (color) fputs(g_color, stdout);
    fputs("Miss Robin hasn't sung this one yet~ "
          "Why not have a word with Conductor Pom-Pom? \n", stdout);
    fputs("Shall we board the train for Penacony?\n", stdout);
    if (color) fputs(WARBLE_RESET, stdout);
    fflush(stdout);
}

/* 裸 robinsing 或曲目前缀有歧义时进入。读完编号就播；
   输入无效就重画菜单，EOF 返回 0 */
static int run_menu(long char_ms, int force_plain, enum warble_part part)
{
    int color = want_color(force_plain);
    const struct warble_song *song = NULL;
    char line[64];

    for (;;) {
        size_t i;

        if (color) fputs(g_color, stdout);
        puts("Which song would you have Miss Robin sing for us?");
        putchar('\n');
        for (i = 0; i < warble_song_count(); i++) {
            const struct warble_song *item = warble_song_at(i);

            printf("%zu. ", i + 1);
            if (color)
                printf(WARBLE_BOLD "%s" WARBLE_UNBOLD "\n", item->title);
            else
                printf("%s\n", item->title);
        }
        putchar('\n');
        if (color) fputs(WARBLE_RESET, stdout);
        fflush(stdout);

        if (fgets(line, sizeof line, stdin) == NULL)
            return 0;

        {
            char *end;
            long n = strtol(line, &end, 10);

            if (end != line) {
                while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')
                    end++;
                if (*end == '\0' && n >= 1 && n <= (long)warble_song_count()) {
                    song = warble_song_at((size_t)n - 1);
                    break;
                }
            }
        }
    }

    play_song(song, char_ms, force_plain, part);
    return 1;
}

int main(int argc, char **argv)
{
    long char_ms = CHAR_MS_DEFAULT;
    long color_index = 53;
    int force_plain = 0;
    int end_options = 0;
    enum warble_part part = WARBLE_PART_FULL;
    char *selector = NULL;
    const char *env_color = getenv("ROBINSING_COLOR");
    const struct warble_song *song;
    int i;

    if (env_color != NULL && *env_color != '\0' &&
        parse_color(env_color, &color_index) != 0) {
        fprintf(stderr, "%s: ROBINSING_COLOR 不是 0~255 的编号\n", argv[0]);
        return 2;
    }

    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        const char *speed_text = NULL;

        if (!end_options && strcmp(arg, "--") == 0) {
            end_options = 1;
            continue;
        }
        if (end_options) {
            if (selector != NULL) {
                fprintf(stderr, "%s: 一次只能唱一首歌\n", argv[0]);
                return 2;
            }
            selector = arg;
            continue;
        }

        if (strcmp(arg, "-f") == 0 || strcmp(arg, "--fast") == 0) {
            char_ms = CHAR_MS_FAST;
        } else if (strcmp(arg, "-p") == 0 || strcmp(arg, "--plain") == 0) {
            force_plain = 1;
        } else if (strcmp(arg, "-c") == 0 || strcmp(arg, "--chorus") == 0 ||
                   strcmp(arg, "--climax") == 0) {
            if (part != WARBLE_PART_FULL && part != WARBLE_PART_CHORUS) {
                fprintf(stderr, "%s: -c 和 -e 只能选一个\n", argv[0]);
                return 2;
            }
            part = WARBLE_PART_CHORUS;
        } else if (strcmp(arg, "-e") == 0 || strcmp(arg, "--end") == 0 ||
                   strcmp(arg, "--ends") == 0) {
            if (part != WARBLE_PART_FULL && part != WARBLE_PART_END) {
                fprintf(stderr, "%s: -c 和 -e 只能选一个\n", argv[0]);
                return 2;
            }
            part = WARBLE_PART_END;
        } else if (strcmp(arg, "-C") == 0 || strcmp(arg, "--color") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: %s 后面要跟一个 0~255 的编号\n", argv[0], arg);
                return 2;
            }
            if (parse_color(argv[i + 1], &color_index) != 0) {
                fprintf(stderr, "%s: 颜色编号 %s 不合法（0~255）\n", argv[0], argv[i + 1]);
                return 2;
            }
            i++;
        } else if (strncmp(arg, "--color=", 8) == 0) {
            if (parse_color(arg + 8, &color_index) != 0) {
                fprintf(stderr, "%s: 颜色编号 %s 不合法（0~255）\n", argv[0], arg + 8);
                return 2;
            }
        } else if (strncmp(arg, "-C=", 3) == 0) {
            if (parse_color(arg + 3, &color_index) != 0) {
                fprintf(stderr, "%s: 颜色编号 %s 不合法（0~255）\n", argv[0], arg + 3);
                return 2;
            }
        } else if (strncmp(arg, "-C", 2) == 0 && arg[2] != '\0') {
            if (parse_color(arg + 2, &color_index) != 0) {
                fprintf(stderr, "%s: 颜色编号 %s 不合法（0~255）\n", argv[0], arg + 2);
                return 2;
            }
        } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
            puts(ROBINSING_VERSION);
            return 0;
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            usage(stdout, argv[0]);
            return 0;
        } else if (strcmp(arg, "-s") == 0 || strcmp(arg, "--speed") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: %s 后面要跟一个时长\n", argv[0], arg);
                return 2;
            }
            speed_text = argv[++i];
        } else if (strncmp(arg, "--speed=", 8) == 0) {
            speed_text = arg + 8;
        } else if (strncmp(arg, "-s=", 3) == 0) {
            speed_text = arg + 3;
        } else if (strncmp(arg, "-s", 2) == 0 && arg[2] != '\0') {
            speed_text = arg + 2;
        } else if (arg[0] != '-') {
            if (selector != NULL) {
                fprintf(stderr, "%s: 一次只能唱一首歌\n", argv[0]);
                return 2;
            }
            selector = arg;
        } else {
            fprintf(stderr, "%s: 未知选项 %s\n", argv[0], arg);
            usage(stderr, argv[0]);
            return 2;
        }

        if (speed_text != NULL) {
            int rc = parse_speed(speed_text, &char_ms);

            if (rc == -2) {
                fprintf(stderr, "%s: 速度 \"%s\" 超出范围，可用 1ms ~ 60s\n",
                        argv[0], speed_text);
                return 2;
            }
            if (rc != 0) {
                fprintf(stderr,
                        "%s: 速度 \"%s\" 不合法，例: -s 50ms、-s=1.2s、-s500（默认单位 ms）\n",
                        argv[0], speed_text);
                return 2;
            }
        }
    }

    setup_color(color_index);

    if (selector == NULL) {
        return run_menu(char_ms, force_plain, part) ? 0 : 2;
    }

    to_lower(selector);
    switch (warble_find(selector, &song)) {
    case WARBLE_FOUND:
        break;
    case WARBLE_AMBIGUOUS:
        return run_menu(char_ms, force_plain, part) ? 0 : 2;
    default:
        unknown_song(force_plain);
        return 2;
    }

    play_song(song, char_ms, force_plain, part);
    return 0;
}
