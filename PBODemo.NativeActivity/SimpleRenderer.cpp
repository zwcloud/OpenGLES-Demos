//
// This file is used by the template to render a basic scene using GL.
//
#include "pch.h"

#include "SimpleRenderer.h"

// These are used by the shader compilation methods.
#include <vector>
#include <iostream>
#include <stdexcept>
#include <cassert>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "PBODemo.NativeActivity", __VA_ARGS__))

#define err(content) ((void)__android_log_print(ANDROID_LOG_ERROR, "PBODemo.NativeActivity", "%s(%d): error: %s", __FILE__ , __LINE__ , content))
#define check(value, content) if(value == 0) {err(content);}

class FScopePerformanceCounter
{
public:
	FScopePerformanceCounter()
	{
		if(clock_gettime(CLOCK_MONOTONIC_RAW, &Start) < 0)
		{
			err("clock_gettime failed");
		}
	}

	~FScopePerformanceCounter()
	{
		if(clock_gettime(CLOCK_MONOTONIC_RAW, &End) < 0)
		{
			err("clock_gettime failed");
		}

		long nanoseconds = End.tv_nsec - Start.tv_nsec;
		double FrameTimeMs = nanoseconds / 1000000.0;
		LOGI("FrameTime: %.2lfms\tFPS: %.1lf", FrameTimeMs, 1000/FrameTimeMs);
	}

protected:
	timespec Start;
	timespec End;
};

struct FOpenGLContext
{
	int window_width = 800;
	int window_height = 600;

	GLuint FBO = 0;
	GLuint PBO = 0;
	GLuint FBOTexture = 0;
	int FBO_width = 2048;
	int FBO_height = 1024;
};

FOpenGLContext GOpenGLContext;

void CheckExtensions()
{    
    LOGI("GL_EXTENSIONS =");
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    std::string extensionsStr{extensions};
    int nextPos = 0;
    int whiteSpaceCounter = 0;
    for (size_t i = 0; i < extensionsStr.length(); i++)
    {
        if(extensionsStr[i] == ' ')
        {
            whiteSpaceCounter++;
        }
        if(whiteSpaceCounter == 5)
        {
            std::string splitStr {extensionsStr.begin() + nextPos, extensionsStr.begin() + i};
            LOGI("    %s", splitStr.c_str());
            nextPos = i+1;
            whiteSpaceCounter = 0;
        }
    }
}

GLuint PersisentSrcBuffer;
GLuint PersisentTargetBuffer;
GLuint PersisentBufferSize = 128*128;

SimpleRenderer::SimpleRenderer() :
    mWindowWidth(0),
    mWindowHeight(0),
    mDrawCount(0)
{
    CheckExtensions();
    
	glGenFramebuffers(1, &GOpenGLContext.FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, GOpenGLContext.FBO);

	glGenTextures(1, &GOpenGLContext.FBOTexture);
	glBindTexture(GL_TEXTURE_2D, GOpenGLContext.FBOTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GOpenGLContext.FBO_width, GOpenGLContext.FBO_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, GOpenGLContext.FBOTexture, 0);

	glGenBuffers(1, &GOpenGLContext.PBO);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, GOpenGLContext.PBO);
	glBufferData(GL_PIXEL_PACK_BUFFER, GOpenGLContext.FBO_width * GOpenGLContext.FBO_height * 3, nullptr, GL_STATIC_READ);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		err("framebuffer is not complete");
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void testCopyBuffer()
{
	glGenBuffers(1, &PersisentSrcBuffer);
	glBindBuffer(GL_COPY_READ_BUFFER, PersisentSrcBuffer);
	glBufferData(GL_COPY_READ_BUFFER, PersisentBufferSize, NULL, GL_STATIC_COPY);
	GLenum err = glGetError();
	unsigned char* ptr = (unsigned char*)glMapBufferRange(GL_COPY_READ_BUFFER, 0, PersisentBufferSize, GL_MAP_WRITE_BIT);
	
	for (int Index = 0; Index < PersisentBufferSize; Index++)
	{
		ptr[Index] = Index%255;
	}
	glUnmapBuffer(GL_COPY_READ_BUFFER);

	glGenBuffers(1, &PersisentTargetBuffer);
	glBindBuffer(GL_COPY_READ_BUFFER, PersisentSrcBuffer);
	glBindBuffer(GL_COPY_WRITE_BUFFER, PersisentTargetBuffer);
	glBufferData(GL_COPY_WRITE_BUFFER, PersisentBufferSize, NULL, GL_STATIC_DRAW);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, PersisentBufferSize);
	glBindBuffer(GL_COPY_WRITE_BUFFER, PersisentTargetBuffer);
	glBindBuffer(GL_COPY_READ_BUFFER, PersisentTargetBuffer);

	glBindBuffer(GL_COPY_WRITE_BUFFER, PersisentTargetBuffer);
	unsigned char* targetPtr = (unsigned char*)glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, PersisentBufferSize, GL_MAP_READ_BIT);
	for (int Index = 0; Index < PersisentBufferSize; Index++)
	{
		check((targetPtr[Index] == ptr[Index]), "");
	}
	glUnmapBuffer(GL_COPY_WRITE_BUFFER);
	glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

SimpleRenderer::~SimpleRenderer()
{
	glDeleteFramebuffers(1, &GOpenGLContext.FBO);
}

void test()
{
	//testCopyBuffer();
}

void SimpleRenderer::Draw()
{
	FScopePerformanceCounter FreqScope;

	static unsigned char R = 125;
	static unsigned char G = 26;
	static unsigned char B = 78;

	R++;
	G++;
	B++;

	glBindFramebuffer(GL_FRAMEBUFFER, GOpenGLContext.FBO);
	glClearColor(R/255.0f, G/255.0f, B/255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, GOpenGLContext.PBO);
	glReadPixels(0, 0, GOpenGLContext.FBO_width, GOpenGLContext.FBO_height, GL_RGB, GL_UNSIGNED_BYTE, 0);
	//glReadPixels returns immediately
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, GOpenGLContext.PBO);
	check(glGetError() == GL_NO_ERROR, "Bind buffer error");

	unsigned char* pixelsData = (unsigned char*)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, PersisentBufferSize, GL_MAP_READ_BIT);
	GLenum error = glGetError();
	int pixelNum = GOpenGLContext.FBO_width * GOpenGLContext.FBO_height;
	for (int i = 0; i < pixelNum; i++)
	{
		int startIdx = 3 * i;
		if ((pixelsData[startIdx] != R) || (pixelsData[startIdx + 1] != G) || (pixelsData[startIdx + 2] != B))
		{
			err("Value is mismatch");
			break;
		}
	}
	glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0.2f, 0.3f, 0.6f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	test();

    mDrawCount += 1;
}

void SimpleRenderer::UpdateWindowSize(GLsizei width, GLsizei height)
{
	GOpenGLContext.window_width = width;
	GOpenGLContext.window_height = height;
	glViewport(0, 0, GOpenGLContext.window_width, GOpenGLContext.window_height);
}
