#define STB_IMAGE_IMPLEMENTATION
#include "Texture.h"

#include <stb_image.h>

#include <iostream>

static GLuint createGLTexture(unsigned char* pixels, int w, int h, int ch)
{
    GLenum fmt = GL_RGB;
    if (ch == 1) fmt = GL_RED;
    else if (ch == 3) fmt = GL_RGB;
    else if (ch == 4) fmt = GL_RGBA;

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

GLuint loadTexture2DFromFile(const std::string& path, bool flipV)
{
    if (path.empty()) { std::cerr << "[Tex] empty path\n"; return 0; }
    stbi_set_flip_vertically_on_load(flipV ? 1 : 0);

    int w, h, ch;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (!data) {
        std::cerr << "[Tex] load failed: " << path << " (" << stbi_failure_reason() << ")\n";
        return 0;
    }
    GLuint id = createGLTexture(data, w, h, ch);
    stbi_image_free(data);
    return id;
}

GLuint loadTexture2DFromMemory(const unsigned char* data, int len, bool flipV)
{
    if (!data || len <= 0) return 0;
    stbi_set_flip_vertically_on_load(flipV ? 1 : 0);

    int w, h, ch;
    unsigned char* pixels = stbi_load_from_memory(data, len, &w, &h, &ch, 0);
    if (!pixels) {
        std::cerr << "[Tex] memory decode failed (" << stbi_failure_reason() << ")\n";
        return 0;
    }
    GLuint id = createGLTexture(pixels, w, h, ch);
    stbi_image_free(pixels);
    return id;
}
