#include "pch.h"
#include "SimpleRenderer.h"
#include <iostream>
#include <sys/time.h>
#include <algorithm>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "PBODemo.NativeActivity", __VA_ARGS__))
#define err(content) ((void)__android_log_print(ANDROID_LOG_ERROR, "PBODemo.NativeActivity", "%s(%d): error: %s", __FILE__ , __LINE__ , content))

void fnCheckGLError(const char* szFile, int nLine)
{
    GLenum ErrCode = glGetError();
    if (GL_NO_ERROR != ErrCode)
    {
        const char* szErr = "UNKNOWN ERROR";
        switch (ErrCode)
        {
			case GL_INVALID_ENUM: szErr = "GL_INVALID_ENUM"; break;
			case GL_INVALID_VALUE: szErr = "GL_INVALID_VALUE"; break;
			case GL_INVALID_OPERATION: szErr = "GL_INVALID_OPERATION"; break;
			case GL_OUT_OF_MEMORY: szErr = "GL_OUT_OF_MEMORY"; break;
			case GL_INVALID_FRAMEBUFFER_OPERATION: szErr = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
        }
        __android_log_print(ANDROID_LOG_ERROR, "PBODemo.NativeActivity", "%s(%d):glError %s\n", szFile, nLine, szErr);
    }
}
#define _CheckGLError_ fnCheckGLError(__FILE__,__LINE__);

//https://gist.github.com/diabloneo/9619917
void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }

    return;
}

//https://stackoverflow.com/a/32334103/3427520
bool nearly_equal(
  float a, float b,
  float epsilon = 128 * FLT_EPSILON, float abs_th = FLT_MIN)
  // those defaults are arbitrary and could be removed
{
  assert(std::numeric_limits<float>::epsilon() <= epsilon);
  assert(epsilon < 1.f);

  if (a == b) return true;

  auto diff = std::abs(a-b);
  auto norm = std::min(float(fabs(a) + fabs(b)), std::numeric_limits<float>::max());
  // or even faster: std::min(std::abs(a + b), std::numeric_limits<float>::max());
  // keeping this commented out until I update figures below
  return diff < std::max(abs_th, epsilon * norm);
}

//not used, put in Draw() 
class FScopePerformanceCounter
{
public:
	FScopePerformanceCounter()
	{
		if(clock_gettime(CLOCK_MONOTONIC_RAW, &Start) < 0)
		{
			err("clock_gettime failed");
			abort();
		}
	}

	~FScopePerformanceCounter()
	{
		if(clock_gettime(CLOCK_MONOTONIC_RAW, &End) < 0)
		{
			err("clock_gettime failed");
			abort();
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
	int FBO_width = 128;
	int FBO_height = 128;

#define RGBA8Byte 1
#define RGBAHalf 2
#define RGBAFloat 3

#define FORMAT RGBAFloat
	//See Engine\Source\Runtime\OpenGLDrv\Private\OpenGLDevice.cpp in PLATFORM_ANDROID
	//See https://www.khronos.org/registry/OpenGL-Refpages/es3.0/html/glTexImage2D.xhtml
#if FORMAT == RGBA8Byte
	//PF_B8G8R8A8 RTF_RGBA8
	GLenum internalFormat = GL_RGBA;
	GLenum format = GL_RGBA;
	GLenum pixelDataType = GL_UNSIGNED_BYTE;
	int pixelByteSize = 4;// sizeof(uint8_t) * 4
#elif FORMAT == RGBAHalf
	//PF_FloatRGBA RTF_RGBA16f
	GLenum internalFormat = GL_RGBA16F;
	GLenum format = GL_RGBA;
	GLenum pixelDataType = GL_HALF_FLOAT;
	int pixelByteSize = 8;// sizeof(half) * 4, sizeof(half) = 16bit = 2byte
#elif FORMAT == RGBAFloat
	//PF_A32B32G32R32F RTF_RGBA32f
	GLenum internalFormat = GL_RGBA32F;
	GLenum format = GL_RGBA;
	GLenum pixelDataType = GL_FLOAT;
	int pixelByteSize = 16;// sizeof(float) * 4, sizeof(float) = 32bit = 4byte
#else
# error Unknown format
#endif

	int pixelNum = FBO_width * FBO_height;
	GLsizeiptr pixelDataSize = pixelNum * pixelByteSize;
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

SimpleRenderer::SimpleRenderer() :
    mWindowWidth(0),
    mWindowHeight(0),
    mDrawCount(0)
{
    CheckExtensions();
    
	//create FBO
	glGenFramebuffers(1, &GOpenGLContext.FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, GOpenGLContext.FBO);

	glGenTextures(1, &GOpenGLContext.FBOTexture);
	glBindTexture(GL_TEXTURE_2D, GOpenGLContext.FBOTexture);
	glTexImage2D(GL_TEXTURE_2D, 0,
		GOpenGLContext.internalFormat, 
		GOpenGLContext.FBO_width, GOpenGLContext.FBO_height,
		0,
		GOpenGLContext.format, GOpenGLContext.pixelDataType, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		GOpenGLContext.FBOTexture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		err("framebuffer is not complete");
		abort();
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//create PBO
	glGenBuffers(1, &GOpenGLContext.PBO);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, GOpenGLContext.PBO);
	glBufferData(GL_PIXEL_PACK_BUFFER, GOpenGLContext.pixelDataSize, nullptr, GL_STATIC_READ);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

SimpleRenderer::~SimpleRenderer()
{
	glDeleteBuffers(1, &GOpenGLContext.PBO);
	glDeleteTextures(1, &GOpenGLContext.FBOTexture);
	glDeleteFramebuffers(1, &GOpenGLContext.FBO);
}

#pragma region Readback FBO via PBO
bool requestPending = false;
GLsync readbackFence = 0;
unsigned char R = 125;
unsigned char G = 26;
unsigned char B = 78;
unsigned char A = 255;

void RequestReadback()
{
	//fill FBO with a new random color
	R = rand()%255;
	G = rand()%255;
	B = rand()%255;
	
	glBindFramebuffer(GL_FRAMEBUFFER, GOpenGLContext.FBO);
	glClearColor(R/255.0f, G/255.0f, B/255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	//request readback via glReadPixels on PBO
	glBindBuffer(GL_PIXEL_PACK_BUFFER, GOpenGLContext.PBO);
	_CheckGLError_
	glReadPixels(0, 0, GOpenGLContext.FBO_width, GOpenGLContext.FBO_height,
		GOpenGLContext.format, GOpenGLContext.pixelDataType, 0);
	_CheckGLError_
	//glReadPixels returns immediately
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	
	_CheckGLError_
	//create fence, which will be signaled when glReadPixels finished in GPU
	readbackFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	_CheckGLError_
	if(readbackFence == 0)
	{
		err("glFenceSync failed");
		abort();
	}
	
	timespec timeNow;
	clock_gettime(CLOCK_MONOTONIC_RAW, &timeNow);
	LOGI("Request readback at %lds %.3fms", timeNow.tv_sec, timeNow.tv_nsec / 1000000.0);

	requestPending = true;
}

//check if glReadPixels on PBO completed
bool IsReadbackReady()
{
	if(!requestPending) err("No request is pending!");

	GLint result;
	glGetSynciv(readbackFence, GL_SYNC_STATUS, sizeof(result), nullptr, &result);
	_CheckGLError_
	bool isReady = result == GL_SIGNALED;
	if(isReady)
	{
		timespec timeNow;
		clock_gettime(CLOCK_MONOTONIC_RAW, &timeNow);
		LOGI("Readback is ready at %lds %.3fms", timeNow.tv_sec, timeNow.tv_nsec / 1000000.0);
	}
	return isReady;
}

void Readback()
{
	if(!requestPending)
	{
		err("No request is pending!");
	}
	
	timespec start;
	clock_gettime(CLOCK_MONOTONIC_RAW, &start);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, GOpenGLContext.PBO);
	_CheckGLError_
		
#if FORMAT == RGBA8Byte
	unsigned char* pixelsData = (unsigned char*)
#elif FORMAT == RGBAHalf
#error not implemented
#elif FORMAT == RGBAFloat
	float* pixelsData = (float*)
#else
# error Unknown format
#endif
		glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, GOpenGLContext.pixelDataSize,
			GL_MAP_READ_BIT);
	if(pixelsData == nullptr)
	{
		err("glMapBufferRange failed");
		abort();
	}
	_CheckGLError_

#if FORMAT == RGBA8Byte
	for (int i = 0; i < GOpenGLContext.pixelNum; i++)
	{
		int startIdx = 4 * i;
		if ((pixelsData[startIdx] != R)
			|| (pixelsData[startIdx + 1] != G)
			|| (pixelsData[startIdx + 2] != B)
			|| (pixelsData[startIdx + 3] != A))
		{
			err("Value is mismatch");
			abort();
		}
	}
#elif FORMAT == RGBAHalf
#error not implemented
#elif FORMAT == RGBAFloat
	for (int i = 0; i < GOpenGLContext.pixelNum; i++)
	{
		int startIdx = 4 * i;
		if ( !nearly_equal(pixelsData[startIdx], R/255.0f)
			|| !nearly_equal(pixelsData[startIdx + 1], G/255.0f)
			|| !nearly_equal(pixelsData[startIdx + 2], B/255.0f)
			|| !nearly_equal(pixelsData[startIdx + 3], A/255.0f))
		{
			err("Value is mismatch");
			abort();
		}
	}
#else
# error Unknown format
#endif
	glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	
	timespec end;
	clock_gettime(CLOCK_MONOTONIC_RAW, &end);
	
	timespec usedTime;
	timespec_diff(&start, &end, &usedTime);
	LOGI("Readback finished in %lds %.3fms", usedTime.tv_sec, usedTime.tv_nsec / 1000000.0);

	//no longer needed since the fence has already been signaled
	glDeleteSync(readbackFence);
	readbackFence = 0;

	requestPending = false;
}
#pragma endregion

timespec timeLastRequestFinished;
const int RequestIntervalSeconds = 3;

void SimpleRenderer::Draw()
{
	//Remove comment of next line to log frame time and fps.
	//FScopePerformanceCounter FreqScope;

	if(!requestPending)
	{
		timespec timeNow;
		clock_gettime(CLOCK_MONOTONIC_RAW, &timeNow);
		if(timeNow.tv_sec - timeLastRequestFinished.tv_sec > RequestIntervalSeconds)
		{
			RequestReadback();
		}
	}
	else
	{
		if(IsReadbackReady())
		{
			Readback();
			clock_gettime(CLOCK_MONOTONIC_RAW, &timeLastRequestFinished);
		}
	}

	//display being requested framebuffer color
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(R/255.0f, G/255.0f, B/255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

    mDrawCount += 1;
}

void SimpleRenderer::UpdateWindowSize(GLsizei width, GLsizei height)
{
	GOpenGLContext.window_width = width;
	GOpenGLContext.window_height = height;
	glViewport(0, 0, GOpenGLContext.window_width, GOpenGLContext.window_height);
}
