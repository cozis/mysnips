#include <stdio.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "graphics.h"

static char *load_file(const char *file, size_t *size)
{
    FILE *stream = fopen(file, "rb");
    if (stream == NULL) return NULL;

    fseek(stream, 0, SEEK_END);
    long size2 = ftell(stream);
    fseek(stream, 0, SEEK_SET);

    char *dst = malloc(size2+1);
    if (dst == NULL) {
        fclose(stream);
        return NULL;
    }

    fread(dst, 1, size2, stream);
    if (ferror(stream)) {
        free(dst);
        fclose(stream);
        return NULL;
    }
    dst[size2] = '\0';

    fclose(stream);
    if (size) *size = size2;
    return dst;
}

unsigned int compile_shader(const char *vertex_file,
                            const char *fragment_file)
{
    int  success;
    char infolog[512];

    char *vertex_str = load_file(vertex_file, NULL);
    if (vertex_str == NULL) return 0;

    char *fragment_str = load_file(fragment_file, NULL);
    if (fragment_str == NULL) {
        free(vertex_str);
        return 0;
    }

    unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_str, NULL);
    glCompileShader(vertex_shader);

    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(vertex_shader, sizeof(infolog), NULL, infolog);
        fprintf(stderr, "Couldn't compile vertex shader (%s)\n", infolog);
        free(vertex_str);
        free(fragment_str);
        return 0;
    }

    unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_str, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(fragment_shader, sizeof(infolog), NULL, infolog);
        fprintf(stderr, "Couldn't compile fragment shader (%s)\n", infolog);
        free(vertex_str);
        free(fragment_str);
        return 0;
    }

    unsigned int shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(shader_program, sizeof(infolog), NULL, infolog);
        fprintf(stderr, "Couldn't link shader program (%s)\n", infolog);
        free(vertex_str);
        free(fragment_str);
        return 0;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    free(vertex_str);
    free(fragment_str);
    return shader_program;
}

Texture load_texture(const char *file, int num_chans_out)
{
    assert(num_chans_out == 3 || num_chans_out == 4);

    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    stbi_set_flip_vertically_on_load(1);

    int width, height, nrChannels;
    unsigned char *data = stbi_load(file, &width, &height, &nrChannels, 0); 
    if (!data) return (Texture) {.id=0, .w=0, .h=0};
    assert(nrChannels == 3 || nrChannels == 4);

    int iformat =    nrChannels == 3 ? GL_RGB : GL_RGBA;
    int oformat = num_chans_out == 3 ? GL_RGB : GL_RGBA;

    glTexImage2D(GL_TEXTURE_2D, 0, iformat, width, height, 0, oformat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    return (Texture) {.id=texture, .w=width, .h=height};
}

unsigned int draw_rectangle_vao;
unsigned int draw_rectangle_vbo;
unsigned int draw_rectangle_program;

void draw_rectangle_init(void)
{
    float vertices[] = {
         1.0, -1.0,
         1.0,  1.0,
        -1.0, -1.0,
         1.0,  1.0,
        -1.0,  1.0,
        -1.0, -1.0,
    };

    glGenVertexArrays(1, &draw_rectangle_vao);
    glGenBuffers(1, &draw_rectangle_vbo);

    glBindVertexArray(draw_rectangle_vao);

    glBindBuffer(GL_ARRAY_BUFFER, draw_rectangle_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(0);

    draw_rectangle_program = compile_shader("assets/shaders/rectangle.vs", "assets/shaders/rectangle.fs");
}

void draw_rectangle_free(void)
{
    glDeleteBuffers(1, &draw_rectangle_vbo);
    glDeleteVertexArrays(1, &draw_rectangle_vao);
    glDeleteProgram(draw_rectangle_program);
}

GLFWwindow *window;

void draw_rectangle(float x, float y, float w, float h,
                    uint8_t r, uint8_t g, uint8_t b)
{
    int screen_w;
    int screen_h;
    glfwGetWindowSize(window, &screen_w, &screen_h);
    glUseProgram(draw_rectangle_program);
    glUniform2f(glGetUniformLocation(draw_rectangle_program, "screen"), screen_w, screen_h);
    glUniform2f(glGetUniformLocation(draw_rectangle_program, "offset"), x, y);
    glUniform2f(glGetUniformLocation(draw_rectangle_program, "size"), w, h);
    glUniform3f(glGetUniformLocation(draw_rectangle_program, "color"), (float) r / 255, (float) g / 255, (float) b / 255);
    glBindVertexArray(draw_rectangle_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

unsigned int draw_texture_vao;
unsigned int draw_texture_vbo;
unsigned int draw_texture_program;

void draw_texture_init_or_abort(void)
{
    float vertices[] = {
         1.0, -1.0,  1.0, 0.0,
         1.0,  1.0,  1.0, 1.0,
        -1.0, -1.0,  0.0, 0.0,
         1.0,  1.0,  1.0, 1.0,
        -1.0,  1.0,  0.0, 1.0,
        -1.0, -1.0,  0.0, 0.0,
    };

    glGenVertexArrays(1, &draw_texture_vao);
    glGenBuffers(1, &draw_texture_vbo);

    glBindVertexArray(draw_texture_vao);

    glBindBuffer(GL_ARRAY_BUFFER, draw_texture_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*) (2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    draw_texture_program = compile_shader("assets/shaders/texture.vs", "assets/shaders/texture.fs");
}

void draw_texture_free(void)
{
    glDeleteBuffers(1, &draw_texture_vbo);
    glDeleteVertexArrays(1, &draw_texture_vao);
    glDeleteProgram(draw_texture_program);
}

void draw_texture(unsigned int texture,
                  float dst_tx, float dst_ty,
                  float dst_sx, float dst_sy,
                  float src_x, float src_y,
                  float src_w, float src_h,
                  bool flip_x)
{
    int screen_w;
    int screen_h;
    glfwGetWindowSize(window, &screen_w, &screen_h);
    glUseProgram(draw_texture_program);
    glUniform1i(glGetUniformLocation(draw_texture_program, "flip_x"), flip_x);
    glUniform2f(glGetUniformLocation(draw_texture_program, "source_offset"), src_x, src_y);
    glUniform2f(glGetUniformLocation(draw_texture_program, "source_size"), src_w, src_h);
    glUniform2f(glGetUniformLocation(draw_texture_program, "screen"), screen_w, screen_h);
    glUniform2f(glGetUniformLocation(draw_texture_program, "offset"), dst_tx, dst_ty);
    glUniform2f(glGetUniformLocation(draw_texture_program, "scale"), dst_sx, dst_sy);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(draw_texture_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void clear_screen(void)
{
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void graphics_init(void *window_)
{
    window = window_;
    draw_texture_init_or_abort();
    draw_rectangle_init();
}

void graphics_free(void)
{
    draw_rectangle_free();
    draw_texture_free();
}
