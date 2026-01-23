#ifndef OSD_H
#define OSD_H

#include <stdint.h>

typedef struct OSD OSD;
OSD *osd_init();
/* IDE and emulink removed - disk hotswap uses INT 13h disk handler directly */
void osd_attach_console(OSD *osd, void *);
void osd_handle_mouse_motion(OSD *osd, int x, int y);
void osd_handle_mouse_button(OSD *osd, int x, int y, int down, int btn);
void osd_handle_key(OSD *osd, int keycode, int down);
void osd_render(OSD *osd, uint8_t *pixels, int w, int h, int pitch);

#endif /* OSD_H */
