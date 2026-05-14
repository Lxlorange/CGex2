#pragma once

#include <glad/glad.h>
#include <string>
#include <stb_image.h>

GLuint loadTexture2DFromFile(const std::string& path, bool flipV = true);
GLuint loadTexture2DFromMemory(const unsigned char* data, int len, bool flipV = true);
GLuint createTexture2DFromRGBAPixels(const void* pixels, int width, int height, bool bgra = false);
