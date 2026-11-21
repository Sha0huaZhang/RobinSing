#ifndef ROBINSING_WARBLE_H
#define ROBINSING_WARBLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 终端配色：紫色，歌名加粗 */
#define WARBLE_PURPLE "\033[38;5;53m"
#define WARBLE_BOLD   "\033[1m"
#define WARBLE_UNBOLD "\033[22m"
#define WARBLE_RESET  "\033[0m"

struct warble_song {
    const char *name;             /* 命令里的短名，如 sway */
    const char *title;            /* 展示名，如 Sway to My Beat in Cosmos */
    const char *const *lines;
    size_t count;
    const char *const *chorus;    /* 没有 [cstart]/[cend] 时为 NULL */
    size_t chorus_count;
    const char *const *ending;    /* 没有 [estart]/[eend] 时为 NULL */
    size_t ending_count;
};

enum warble_part {
    WARBLE_PART_FULL = 0,
    WARBLE_PART_CHORUS = 1,
    WARBLE_PART_END = 2
};

enum warble_lookup {
    WARBLE_FOUND = 0,
    WARBLE_UNKNOWN = -1,
    WARBLE_AMBIGUOUS = -2
};

/* 按命令别名找歌：不区分大小写，完整名字优先，其次是唯一前缀。
   找不到返回 WARBLE_UNKNOWN；前缀同时命中多首歌返回 WARBLE_AMBIGUOUS */
enum warble_lookup warble_find(const char *arg, const struct warble_song **out);

/* 菜单按这里的顺序编号，编号总是从 1 开始连续 */
size_t warble_song_count(void);
const struct warble_song *warble_song_at(size_t i);

/* 设置歌词主色；传空串表示不上色。默认 WARBLE_PURPLE */
void warble_set_color(const char *escape);

/* 逐字唱出，每个字符停 char_ms，换行停 6 * char_ms。行尾加 ~ 表示拖腔，最后一行
加 ~~，然后直接换行（不再补空格对齐）。
   part 选高潮或尾声；那一部分没标记就退回整首 */
void warble_sing(const struct warble_song *song, enum warble_part part, long char_ms);

/* 非终端场景：一次性输出纯文本，无颜色无停顿 */
void warble_plain(const struct warble_song *song, enum warble_part part);

#ifdef __cplusplus
}
#endif

#endif
