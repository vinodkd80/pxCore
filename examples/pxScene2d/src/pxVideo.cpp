/*

 pxCore Copyright 2005-2018 John Robinson

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/

// pxText.h

#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "pxVideo.h"
#include "pxContext.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <gst/gst.h>
#include <iostream>
#include <string>
#include "pxContext.h"

static const char *VSHADER =
	"attribute vec2 vertexIn;"
	"attribute vec2 textureIn;"
	"varying vec2 textureOut;"
	"uniform mat4 trans;"
	"void main() {"
		"gl_Position = trans * vec4(vertexIn,0, 1);"
		"textureOut = textureIn;"
	"}";

static const char *FSHADER =
	"#ifdef GL_ES \n"
		"  precision mediump float; \n"
	"#endif \n"
	"varying vec2 textureOut;"
	"uniform sampler2D tex_y;"
	"uniform sampler2D tex_u;"
	"uniform sampler2D tex_v;"
	"void main() {"
		"vec3 yuv;"
		"vec3 rgb;"
		"yuv.x = texture2D(tex_y, textureOut).r;"
		"yuv.y = texture2D(tex_u, textureOut).r - 0.5;"
		"yuv.z = texture2D(tex_v, textureOut).r - 0.5;"
		"rgb = mat3( 1, 1, 1, 0, -0.39465, 2.03211, 1.13983, -0.58060,  0) * yuv;"
		"gl_FragColor = vec4(rgb, 1);"
	"}";

#define ATTRIB_VERTEX 0
#define ATTRIB_TEXTURE 1

GLuint mProgramID;
GLuint id_y, id_u, id_v; // texture id
GLuint textureUniformY, textureUniformU,textureUniformV;
static GLuint _vertexArray;
static GLuint _vertexBuffer[2];
static const int FPS = 60;
GLfloat currentAngleOfRotation = 0.0;

static int FBO_W = 1280;
static int FBO_H = 720;
extern pxContext context;
static pxSharedContextRef sharedContext;
static pxContextFramebufferRef gAampFbo = NULL;
rtMutex gAampFboMutex;
extern rtThreadQueue* gUIThreadQueue;
static bool initialized = false;
GThread *aampMainLoopThread = NULL;
static GMainLoop *AAMPGstPlayerMainLoop = NULL;
void *pxVideoObj = NULL;

/**
 * @brief Thread to run mainloop (for standalone mode)
 * @param[in] arg user_data
 * @retval void pointer
 */
static void* AAMPGstPlayer_StreamThread(void *arg)
{
  if (AAMPGstPlayerMainLoop)
  {
    g_main_loop_run(AAMPGstPlayerMainLoop); // blocks
    printf("AAMPGstPlayer_StreamThread: exited main event loop\n");
  }
  g_main_loop_unref(AAMPGstPlayerMainLoop);
  AAMPGstPlayerMainLoop = NULL;
  return NULL;
}

/**
 * @brief To initialize Gstreamer and start mainloop (for standalone mode)
 * @param[in] argc number of arguments
 * @param[in] argv array of arguments
 */
void InitPlayerLoop()
{
  if (!initialized)
  {
    initialized = true;
    gst_init(NULL, NULL);
    AAMPGstPlayerMainLoop = g_main_loop_new(NULL, FALSE);
    aampMainLoopThread = g_thread_new("AAMPGstPlayerLoop", &AAMPGstPlayer_StreamThread, NULL );
  }
}

static GLuint LoadShader( GLenum type )
{
	GLuint shaderHandle = 0;
	const char *sources[1];

	if(GL_VERTEX_SHADER == type)
	{
		sources[0] = VSHADER;
	}
	else
	{
		sources[0] = FSHADER;
	}

	if( sources[0] )
	{
		shaderHandle = glCreateShader(type);
		glShaderSource(shaderHandle, 1, sources, 0);
		glCompileShader(shaderHandle);
		GLint compileSuccess;
		glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &compileSuccess);
		if (compileSuccess == GL_FALSE)
		{
			GLchar msg[1024];
			glGetShaderInfoLog(shaderHandle, sizeof(msg), 0, &msg[0]);
			printf( "%s\n", msg );
		}
	}

	return shaderHandle;
}

void InitYUVShaders()
{
	sharedContext->makeCurrent(true);

    gAampFboMutex.lock();
	if (gAampFbo.getPtr() == NULL)
	{
		gAampFbo = context.createFramebuffer(1280, 720);
	}
	else
	{
		context.updateFramebuffer(gAampFbo, 1280, 720);
	}


	/* if (firstTime || width / height changes)
	{
		gAampOffscreen.init(width, height);
	}*/

    //copy buffer in RGBA format to gAampOffscreen



	//change fbo width and height if desired
	//context.updateFramebuffer(fbo, static_cast<int>(floor(w)), static_cast<int>(floor(h)));
//	pxTextureRef textureRef = GetImageTexture ();

	pxContextFramebufferRef prevFbo = context.getCurrentFramebuffer();
	pxError replacedFbo = PX_NOTINITIALIZED;
	bool existingFbo = false;
	if (prevFbo.getPtr() != gAampFbo.getPtr())
	{
		replacedFbo = context.setFramebuffer(gAampFbo);
	}
	{
		existingFbo = true;
	}
	if (existingFbo || replacedFbo == PX_OK)
	{
		GLint linked;

		GLint vShader = LoadShader(GL_VERTEX_SHADER);
		GLint fShader = LoadShader(GL_FRAGMENT_SHADER);
		mProgramID = glCreateProgram();
		glAttachShader(mProgramID,vShader);
		glAttachShader(mProgramID,fShader);

		glBindAttribLocation(mProgramID, ATTRIB_VERTEX, "vertexIn");
		glBindAttribLocation(mProgramID, ATTRIB_TEXTURE, "textureIn");
		glLinkProgram(mProgramID);
		glValidateProgram(mProgramID);

		glGetProgramiv(mProgramID, GL_LINK_STATUS, &linked);
		if( linked == GL_FALSE )
		{
			GLint logLen;
			glGetProgramiv(mProgramID, GL_INFO_LOG_LENGTH, &logLen);
			GLchar *msg = (GLchar *)malloc(sizeof(GLchar)*logLen);
			glGetProgramInfoLog(mProgramID, logLen, &logLen, msg );
			printf( "%s\n", msg );
			free( msg );
		}
		glUseProgram(mProgramID);
		glDeleteShader(vShader);
		glDeleteShader(fShader);

		//Get Uniform Variables Location
		textureUniformY = glGetUniformLocation(mProgramID, "tex_y");
		textureUniformU = glGetUniformLocation(mProgramID, "tex_u");
		textureUniformV = glGetUniformLocation(mProgramID, "tex_v");

		typedef struct _vertex
		{
			float p[2];
			float uv[2];
		} Vertex;

		static const Vertex vertexPtr[4] =
		{
			{{-1,-1}, {0.0,1 } },
			{{ 1,-1}, {1,1 } },
			{{ 1, 1}, {1,0.0 } },
			{{-1, 1}, {0.0,0.0} }
		};
		static const unsigned short index[6] =
		{
			0,1,2, 2,3,0
		};

		glGenVertexArrays(1, &_vertexArray);
		glBindVertexArray(_vertexArray);
		glGenBuffers(2, _vertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertexPtr), vertexPtr, GL_STATIC_DRAW );
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _vertexBuffer[1]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index), index, GL_STATIC_DRAW );
		glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, GL_FALSE,
								sizeof(Vertex), (const GLvoid *)offsetof(Vertex,p) );
		glEnableVertexAttribArray(ATTRIB_VERTEX);

		glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, GL_FALSE,
							  sizeof(Vertex), (const GLvoid *)offsetof(Vertex, uv ) );
		glEnableVertexAttribArray(ATTRIB_TEXTURE);
		glBindVertexArray(0);

		glGenTextures(1, &id_y);
		glBindTexture(GL_TEXTURE_2D, id_y);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenTextures(1, &id_u);
		glBindTexture(GL_TEXTURE_2D, id_u);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenTextures(1, &id_v);
		glBindTexture(GL_TEXTURE_2D, id_v);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	if(replacedFbo == PX_OK)
	{
		context.setFramebuffer(prevFbo);
	}
	gAampFboMutex.unlock();
}


// Function that does the drawing
// glut calls this function whenever it needs to redraw

void newAampFrame(void* context, void* data)
{
	pxVideo* videoObj = reinterpret_cast<pxVideo*>(context);
	if (videoObj)
	{
		videoObj->onTextureReady();
	}
}

pxVideo::pxVideo(pxScene2d* scene):pxObject(scene), mVideoTexture()
#ifdef ENABLE_SPARK_VIDEO_PUNCHTHROUGH
 ,mEnablePunchThrough(true)
#else
, mEnablePunchThrough(false)
#endif //ENABLE_SPARK_VIDEO_PUNCHTHROUGH
,mAutoPlay(false)
,mUrl("")
{
	  InitPlayerLoop();
	  mAamp = new PlayerInstanceAAMP();
	  assert (nullptr != mAamp);
//	  InitYUVShaders();
	  pxVideoObj = (void *)this;
	  sharedContext = context.createSharedContext();
}

pxVideo::~pxVideo()
{
	delete mAamp;
}

void pxVideo::onInit()
{
	rtLogError("%s:%d.",__FUNCTION__,__LINE__);
	if(mAutoPlay)
	{
		play();
	}
  mReady.send("resolve",this);
  pxObject::onInit();
}

void updateYUVFrame_not(uint8_t *yuvBuffer, int size, int pixel_w, int pixel_h)
{
	/** Input in I420 (YUV420) format.
	  * Buffer structure:
	  * ----------
	  * |        |
	  * |   Y    | size = w*h
	  * |        |
	  * |________|
	  * |   U    |size = w*h/4
	  * |--------|
	  * |   V    |size = w*h/4
	  * ----------*
	  */
	if(yuvBuffer)
	{
		sharedContext->makeCurrent(true);

	    gAampFboMutex.lock();
		if (gAampFbo.getPtr() == NULL)
		{
			gAampFbo = context.createFramebuffer(pixel_w, pixel_h);
		}
		else
		{
			context.updateFramebuffer(gAampFbo, pixel_w, pixel_h);
		}
		pxContextFramebufferRef prevFbo = context.getCurrentFramebuffer();
		pxError replacedFbo = PX_NOTINITIALIZED;
		bool existingFbo = false;
		if (prevFbo.getPtr() != gAampFbo.getPtr())
		{
			replacedFbo = context.setFramebuffer(gAampFbo);
		}
		{
			existingFbo = true;
		}
		if (existingFbo || replacedFbo == PX_OK)
		{
			printf("AAMP: Rendering frame.\n");
			unsigned char *yPlane, *uPlane, *vPlane;
			yPlane = yuvBuffer;
			uPlane = yPlane + (pixel_w*pixel_h);
			vPlane = uPlane + (pixel_w*pixel_h)/4;

			glClearColor(0.0,0.0,0.0,0.0);
			glClear(GL_COLOR_BUFFER_BIT);

			//Y
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, id_y);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pixel_w, pixel_h, 0, GL_RED, GL_UNSIGNED_BYTE, yPlane);
			glUniform1i(textureUniformY, 0);

			//U
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, id_u);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pixel_w/2, pixel_h/2, 0, GL_RED, GL_UNSIGNED_BYTE, uPlane);
			glUniform1i(textureUniformU, 1);

			//V
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, id_v);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pixel_w/2, pixel_h/2, 0, GL_RED, GL_UNSIGNED_BYTE, vPlane);
			glUniform1i(textureUniformV, 2);

			//Rotate
			glm::mat4 trans = glm::rotate(
				glm::mat4(1.0f),
				currentAngleOfRotation * 360,
				glm::vec3(1.0f, 1.0f, 1.0f)
			);
			GLint uniTrans = glGetUniformLocation(mProgramID, "trans");
			glUniformMatrix4fv(uniTrans, 1, GL_FALSE, glm::value_ptr(trans));

			glBindVertexArray(_vertexArray);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0 );
			glBindVertexArray(0);

		}
		if(replacedFbo == PX_OK)
		{
			context.setFramebuffer(prevFbo);
		}
		gAampFboMutex.unlock();
		gUIThreadQueue->addTask(newAampFrame, pxVideoObj, NULL);
	}
}


inline unsigned char RGB_ADJUST(double tmp)
{
	return (unsigned char)((tmp >= 0 && tmp <= 255)?tmp:(tmp < 0 ? 0 : 255));
}
void CONVERT_YUV420PtoRGB24(unsigned char* yuv420buf,unsigned char* rgbOutBuf,int nWidth,int nHeight)
{
	unsigned char Y,U,V,R,G,B;
	unsigned char* yPlane,*uPlane,*vPlane;
	int rgb_width , u_width;
	rgb_width = nWidth * 3;
	u_width = (nWidth >> 1);
	int offSet = 0;

	yPlane = yuv420buf;
	uPlane = yuv420buf + nWidth*nHeight;
	vPlane = uPlane + nWidth*nHeight/4;

	for(int i = 0; i < nHeight; i++)
	{
		for(int j = 0; j < nWidth; j ++)
		{
			Y = *(yPlane + nWidth * i + j);
			offSet = (i>>1) * (u_width) + (j>>1);
			V = *(uPlane + offSet);
			U = *(vPlane + offSet);

			//  R,G,B values
			R = RGB_ADJUST((Y + (1.4075 * (V - 128))));
			G = RGB_ADJUST((Y - (0.3455 * (U - 128) - 0.7169 * (V - 128))));
			B = RGB_ADJUST((Y + (1.7790 * (U - 128))));
			offSet = rgb_width * i + j * 3;

			rgbOutBuf[offSet] = B;
			rgbOutBuf[offSet + 1] = G;
			rgbOutBuf[offSet + 2] = R;
		}
	}
}

pxTextureRef GetImageTexture ()
{
    std::string url = "/tmp/Resources/1.jpeg";

    //url = win.GetOutPath() + url;

    rtRef<rtImageResource> resource = pxImageManager::getImage((char*)url.c_str());
    pxTextureRef texture = resource->getTexture();
    if (texture == NULL)
        return NULL;
    resource->clearDownloadRequest();
    return texture;
}

void updateYUVFrame(uint8_t *yuvBuffer, int size, int pixel_w, int pixel_h)
{
	/** Input in I420 (YUV420) format.
	  * Buffer structure:
	  * ----------
	  * |        |
	  * |   Y    | size = w*h
	  * |        |
	  * |________|
	  * |   U    |size = w*h/4
	  * |--------|
	  * |   V    |size = w*h/4
	  * ----------*
	  */
	if(yuvBuffer)
	{
		sharedContext->makeCurrent(true);

		gAampFboMutex.lock();
		if (gAampFbo.getPtr() == NULL)
		{
			gAampFbo = context.createFramebuffer(1280, 720);
		}
		else
		{
			context.updateFramebuffer(gAampFbo, 1280, 720);
		}
		FBO_W = 1280;
		FBO_H = 720;
		pxContextFramebufferRef prevFbo = context.getCurrentFramebuffer();
		pxError replacedFbo = PX_NOTINITIALIZED;
		bool existingFbo = false;
		if (prevFbo.getPtr() != gAampFbo.getPtr())
		{
			replacedFbo = context.setFramebuffer(gAampFbo);
		}
		{
			existingFbo = true;
		}
		if (existingFbo || replacedFbo == PX_OK)
		{
	#if 0
			glClearColor(0.0,1.0,0.0,1.0);
			glClear(GL_COLOR_BUFFER_BIT);
	#else
			static std::vector<uint8_t> rgbVect;
			int rgbLen = pixel_w*pixel_h*3;
			if(rgbVect.size() < rgbLen)
			{
				rgbVect.resize(rgbLen);
			}
			uint8_t *buffer_convert = rgbVect.data();
	printf("AAMP: Buffer size=%d pixel_w=%d pixel_h=%d.\n",size, pixel_w, pixel_h);
			CONVERT_YUV420PtoRGB24(yuvBuffer,buffer_convert,pixel_w,pixel_h);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, id_y);

			glClearColor(0.0,1.0,0.0,1.0);
			glClear(GL_COLOR_BUFFER_BIT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

			glTexImage2D(GL_TEXTURE_2D, 0, 3, pixel_w, pixel_h, 0, GL_RGB, GL_UNSIGNED_BYTE,buffer_convert);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_TEXTURE_2D);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

			glBegin(GL_QUADS);

				glTexCoord2f(0.0, 1.0);
				glVertex3f(-1.0, -1.0, 0.0);

				glTexCoord2f(0.0, 0.0);
				glVertex3f(-1.0, 1.0, 0.0);

				glTexCoord2f(1.0, 0.0);
				glVertex3f(1.0, 1.0, 0.0);

				glTexCoord2f(1.0,1.0);
				glVertex3f(1.0, -1.0, 0.0);

			glEnd();

			glDisable(GL_TEXTURE_2D);

			printf("AAMP: Rendered frame.\n");
	#endif
			glFlush();
	//		pxTextureRef textureRef = GetImageTexture();
	//		gAampFbo->setTexture(textureRef);
			std::cout << "AAMP: Rendered.\n";
		}
		if(replacedFbo == PX_OK)
		{
			context.setFramebuffer(prevFbo);
		}
		gAampFboMutex.unlock();
		gUIThreadQueue->addTask(newAampFrame, pxVideoObj, NULL);
	}
}

void pxVideo::draw()
{
  //if (!isRotated() && mEnablePunchThrough)
  if (false)
  {
    int screenX = 0;
    int screenY = 0;
    context.mapToScreenCoordinates(0,0,screenX, screenY);
    context.punchThrough(screenX,screenY,mw, mh);
  }
  else
  {
    // TODO - remove red rectangle code and uncomment code below when using video texture
    //static float redColor[4] = {1.0, 0.0, 0.0, 1.0};
    //context.drawRect(mw, mh, 1.0, redColor, redColor);

    //TODO - uncomment code below and remove red rectangle when adding video texture support
    
	  static pxTextureRef nullMaskRef;
	  static float color[4] = {1.0, 0.0, 0.0, 1.0};
//	  static pxTextureRef textureRef = GetImageTexture ();
//      context.drawImage(100, 100, 1280, 720, textureRef, nullMaskRef);
//	  //context.drawImage(0, 0, 1280, 720,  textureRef, nullMaskRef, true, color, pxConstantsStretch::STRETCH, pxConstantsStretch::STRETCH, true, pxConstantsMaskOperation::NORMAL);
	  if(NULL != gAampFbo)
	  {
		gAampFboMutex.lock();
		context.drawImage(0, 0, 1280, 720,  gAampFbo->getTexture(), nullMaskRef);
		gAampFboMutex.unlock();
		printf("SPARK: Rendered frame.\n");
	  }
  }
}

bool pxVideo::isRotated()
{
  pxMatrix4f matrix = context.getMatrix();
  float *f = matrix.data();
  const float e= 1.0e-2;

  if ( (fabsf(f[1]) > e) ||
       (fabsf(f[2]) > e) ||
       (fabsf(f[4]) > e) ||
       (fabsf(f[6]) > e) ||
       (fabsf(f[8]) > e) ||
       (fabsf(f[9]) > e) )
  {
    return true;
  }

  return false;
}

//properties
rtError pxVideo::availableAudioLanguages(rtObjectRef& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::availableClosedCaptionsLanguages(rtObjectRef& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::availableSpeeds(rtObjectRef& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::duration(float& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::zoom(rtString& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::setZoom(const char* /*s*/)
{
  //TODO
  return RT_OK;
}

rtError pxVideo::volume(uint32_t& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::setVolume(uint32_t /*v*/)
{
  //TODO
  return RT_OK;
}

rtError pxVideo::closedCaptionsOptions(rtObjectRef& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::setClosedCaptionsOptions(rtObjectRef /*v*/)
{
  //TODO
  return RT_OK;
}

rtError pxVideo::closedCaptionsLanguage(rtString& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::setClosedCaptionsLanguage(const char* /*s*/)
{
  //TODO
  return RT_OK;
}

rtError pxVideo::contentOptions(rtObjectRef& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::setContentOptions(rtObjectRef /*v*/)
{
  //TODO
  return RT_OK;
}

rtError pxVideo::speed(float& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::setSpeedProperty(float /*v*/)
{
  //TODO
  return RT_OK;
}

rtError pxVideo::position(float& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::setPosition(float /*v*/)
{
  //TODO
  return RT_OK;
}

rtError pxVideo::audioLanguage(rtString& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::setAudioLanguage(const char* /*s*/)
{
  //TODO
  return RT_OK;
}

rtError pxVideo::secondaryAudioLanguage(rtString& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::setSecondaryAudioLanguage(const char* /*s*/)
{
  //TODO
  return RT_OK;
}

rtError pxVideo::url(rtString& url) const
{
  url = mUrl;
  return RT_OK;
}

rtError pxVideo::setUrl(const char* url)
{
  mUrl = rtString(url);
  rtLogError("%s:%d: URL[%s].",__FUNCTION__,__LINE__,url);
  return RT_OK;
}

rtError pxVideo::tsbEnabled(bool& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::setTsbEnabled(bool /*v*/)
{
  //TODO
  return RT_OK;
}

rtError pxVideo::closedCaptionsEnabled(bool& /*v*/) const
{
  //TODO
  return RT_OK;
}

rtError pxVideo::setClosedCaptionsEnabled(bool /*v*/)
{
  //TODO
  return RT_OK;
}

rtError pxVideo::autoPlay(bool& autoPlay) const
{
  autoPlay = mAutoPlay;
  return RT_OK;
}

rtError pxVideo::setAutoPlay(bool value)
{
  mAutoPlay = value;
  rtLogError("%s:%d: autoPlay[%s].",__FUNCTION__,__LINE__,value?"TRUE":"FALSE");
  return RT_OK;
}

rtError pxVideo::play()
{
  rtLogError("%s:%d.",__FUNCTION__,__LINE__);
  if(!mUrl.isEmpty())
  {
	  mAamp->Tune(mUrl.cString());
  }
  return RT_OK;
}

rtError pxVideo::pause()
{
  //TODO
  return RT_OK;
}

rtError pxVideo::stop()
{
  mAamp->Stop();
  return RT_OK;
}

rtError pxVideo::setSpeed(float /*speed*/, float /*overshootCorrection*/)
{
  //TODO
  return RT_OK;
}

rtError pxVideo::setPositionRelative(float /*seconds*/)
{
  //TODO
  return RT_OK;
}

rtError pxVideo::requestStatus()
{
  //TODO
  return RT_OK;
}

rtError pxVideo::setAdditionalAuth(rtObjectRef /*params*/)
{
  //TODO
  return RT_OK;
}

rtDefineObject(pxVideo, pxObject);
rtDefineProperty(pxVideo, availableAudioLanguages);
rtDefineProperty(pxVideo, availableClosedCaptionsLanguages);
rtDefineProperty(pxVideo, availableSpeeds);
rtDefineProperty(pxVideo, duration);
rtDefineProperty(pxVideo, zoom);
rtDefineProperty(pxVideo, volume);
rtDefineProperty(pxVideo, closedCaptionsOptions);
rtDefineProperty(pxVideo, closedCaptionsLanguage);
rtDefineProperty(pxVideo, contentOptions);
rtDefineProperty(pxVideo, speed);
rtDefineProperty(pxVideo, position);
rtDefineProperty(pxVideo, audioLanguage);
rtDefineProperty(pxVideo, secondaryAudioLanguage);
rtDefineProperty(pxVideo, url);
rtDefineProperty(pxVideo, tsbEnabled);
rtDefineProperty(pxVideo, closedCaptionsEnabled);
rtDefineProperty(pxVideo, autoPlay);
rtDefineMethod(pxVideo, play);
rtDefineMethod(pxVideo, pause);
rtDefineMethod(pxVideo, stop);
rtDefineMethod(pxVideo, setSpeed);
rtDefineMethod(pxVideo, setPositionRelative);
rtDefineMethod(pxVideo, requestStatus);
rtDefineMethod(pxVideo, setAdditionalAuth);

