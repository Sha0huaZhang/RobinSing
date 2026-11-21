#ifndef ROBINSING_PLATFORM_H
#define ROBINSING_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

int       plat_is_tty(void);
void      plat_nap(long ms);
void      plat_vt_on(void);

/* 单调时钟，单位毫秒；只用于算时间差，绝对值无意义 */
long long plat_now_ms(void);

#ifdef __cplusplus
}
#endif

#endif
