#ifndef ROBINSING_PLATFORM_H
#define ROBINSING_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

int       plat_is_tty(void);
void      plat_nap(long ms);

/* 打开终端的 ANSI 转义支持。成功返回 1；Windows 老控制台返回 0，
   调用方应据此关闭颜色，避免把转义序列当文字打出来 */
int       plat_vt_on(void);

/* 默认颜色编号。Windows 控制台是纯黑底，53 那种深紫几乎看不见，
   所以那边用 141（淡紫 / 紫罗兰）；macOS / Linux 沿用 53 */
int       plat_default_color(void);

/* 终端能显示的颜色数：16 或 256。不看环境变量，只看平台 */
int       plat_color_depth(void);

/* 单调时钟，单位毫秒；只用于算时间差，绝对值无意义 */
long long plat_now_ms(void);

#ifdef __cplusplus
}
#endif

#endif
