#pragma once

#include <string.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GLES/glext.h>
#include <GLES3/gl31.h>
#include <GLES3/gl3ext.h>

class SimpleRenderer
{
public:
    SimpleRenderer();
    ~SimpleRenderer();
    void CreateTextureArray();
    void Draw();
    void UpdateWindowSize(GLsizei width, GLsizei height);

private:
    GLsizei mWindowWidth;
    GLsizei mWindowHeight;
    int mDrawCount;
};
