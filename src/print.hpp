#pragma once

#include <sstream>
#include <cstdio>

void print_enable_debug(void);
void print_disable_debug(void);
void print_error(const char* str, ...);
void print_info(const char* str, ...);
void GLAPIENTRY GLDebugMessageCallback(GLenum source, GLenum type, GLuint id,
                            GLenum severity, GLsizei length,
                            const GLchar *msg, const void *data);