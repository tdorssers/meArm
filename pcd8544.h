/*
 * pcd8544.h
 * 48x84 LCD Driver
 *
 * Created: 6-11-2019 18:05:28
 *  Author: Tim Dorssers
 */ 


#ifndef PCD8544_H_
#define PCD8544_H_

#define pcd8544_write_string_P(__str, __sca) pcd8544_write_string_p(PSTR(__str), __sca)

extern void pcd8544_init(void);
extern void pcd8544_clear(void);
extern void pcd8544_power(uint8_t on);
extern void pcd8544_set_pixel(uint8_t x, uint8_t y, uint8_t value);
extern void pcd8544_write_char(char code, uint8_t scale);
extern void pcd8544_write_string(char *str, uint8_t scale);
extern void pcd8544_write_string_p(const char *str, uint8_t scale);
extern void pcd8544_set_cursor(uint8_t x, uint8_t y);
extern void pcd8544_render(void);
extern void pcd8544_draw_hline(uint8_t x, uint8_t y, uint8_t length);
extern void pcd8544_draw_vline(uint8_t x, uint8_t y, uint8_t length);
extern void pcd8544_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

#endif /* PCD8544_H_ */