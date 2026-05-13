#pragma once

#include <glad/glad.h>

#include <string>

GLuint loadTexture2DFromFile(const std::string& texturePath, bool flipVertically = true);
