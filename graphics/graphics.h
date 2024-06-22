#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    unsigned int id;
    int w;
    int h;
} Texture;

void graphics_init(void *window_);
void graphics_free(void);

void draw_rectangle(float x, float y, float w, float h,
                    uint8_t r, uint8_t g, uint8_t b);

void draw_texture(unsigned int texture,
                  float dst_tx, float dst_ty,
                  float dst_sx, float dst_sy,
                  float src_x, float src_y,
                  float src_w, float src_h,
                  bool flip_x);

Texture load_texture(const char *file, int num_chans_out);
// TODO: free_texture

void clear_screen(void);

#endif
