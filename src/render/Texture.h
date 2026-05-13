#pragma once

#include <glad/glad.h>
#include <string>

GLuint loadTexture2DFromFile(const std::string& path, bool flipV = true);
GLuint loadTexture2DFromMemory(const unsigned char* data, int len, bool flipV = true);
