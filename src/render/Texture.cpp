#define STB_IMAGE_IMPLEMENTATION
#include "Texture.h"

#include <stb_image.h>

#include <iostream>

GLuint loadTexture2DFromFile(const std::string& texturePath, bool flipVertically)
{
    if (texturePath.empty()) {
        std::cerr << "[Texture] Load error: empty texture path.\n";
        return 0;
    }

    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(texturePath.c_str(), &width, &height, &channels, 0);
    if (data == nullptr) {
        std::cerr << "[Texture] Load error: failed to decode image.\n"
                  << "  Path: " << texturePath << '\n'
                  << "  Reason: " << stbi_failure_reason() << '\n';
        return 0;
    }

    GLenum format = GL_RGB;
    if (channels == 1) {
        format = GL_RED;
    } else if (channels == 3) {
        format = GL_RGB;
    } else if (channels == 4) {
        format = GL_RGBA;
    } else {
        std::cerr << "[Texture] Load warning: unsupported channel count (" << channels
                  << "), treating as RGB.\n"
                  << "  Path: " << texturePath << '\n';
    }

    GLuint textureId = 0;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    return textureId;
}
