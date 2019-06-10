/*

   pxCore Copyright 2005-2019 Christo Joseph

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

// pxVideoImplAAMP.cpp

#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <GLUT/glut.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include "pxVideoImplAAMP.h"
#include "pxContext.h"

std::string vertexShader =
						"attribute vec2 pos;"
						"uniform mat4 trans;"
						"void main() {"
						"gl_Position = trans * vec4(pos,0, 1);"
						"}";

std::string fragmentShader =
							"void main() {"
							"gl_FragColor = vec4(1, 0, 0, 1);"
							"}";

static GLuint aampGLprogramId = 0;
static int pixel_w = 500;
static int pixel_h = 500;
GLfloat currentAngleOfRotation = 0.0;
extern pxContext context;

// Compile and create shader object and returns its id
static GLuint compileShaders(std::string shader, GLenum type)
{

	const char* shaderCode = shader.c_str();
	GLuint shaderId = glCreateShader(type);

	if (shaderId == 0) { // Error: Cannot create shader object
		std::cout << "AAMP: Error creating shaders\n";
		return 0;
	}

	// Attach source code to this object
	glShaderSource(shaderId, 1, &shaderCode, NULL);
	glCompileShader(shaderId); // compile the shader object

	GLint compileStatus;

	// check for compilation status
	glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compileStatus);

	if (!compileStatus) { // If compilation was not successfull
		int length;
		glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &length);
		char* cMessage = new char[length];

		// Get additional information
		glGetShaderInfoLog(shaderId, length, &length, cMessage);
		std::cout << "AAMP: Cannot Compile Shader: " << cMessage<<std::endl;
		delete[] cMessage;
		glDeleteShader(shaderId);
		return 0;
	}

	return shaderId;
}

// Initialize and put everything together
void AAMPGLinit()
{
	static pxSharedContextRef sharedContext = context.createSharedContext();
	sharedContext->makeCurrent(true);

	static pxContextFramebufferRef fbo = context.createFramebuffer(pixel_w, pixel_h, false);

	//change fbo width and height if desired
	//context.updateFramebuffer(fbo, static_cast<int>(floor(w)), static_cast<int>(floor(h)));
	if (context.setFramebuffer(fbo) != PX_OK)
	{
		return;
	}
	// clear the framebuffer each frame with black color
	glClearColor(0, 0, 0, 0);

	GLfloat vertices[] = { // vertex coordinates
						-0.7, -0.7, 0,
						0.7, -0.7, 0,
						0, 0.7, 0
	};

	GLuint vboId;
	glGenBuffers(1, &vboId);
	glBindBuffer(GL_ARRAY_BUFFER, vboId);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// unbind the active buffer
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GLuint vShaderId = compileShaders(vertexShader, GL_VERTEX_SHADER);
	GLuint fShaderId = compileShaders(fragmentShader, GL_FRAGMENT_SHADER);

	aampGLprogramId = glCreateProgram(); // crate a program

	if (aampGLprogramId == 0) {
		std::cout << "AAMP: Error Creating Shader Program.\n";
	}

	// Attach both the shaders to it
	glAttachShader(aampGLprogramId, vShaderId);
	glAttachShader(aampGLprogramId, fShaderId);

	// Create executable of this program
	glLinkProgram(aampGLprogramId);

	GLint linkStatus;

	// Get the link status for this program
	glGetProgramiv(aampGLprogramId, GL_LINK_STATUS, &linkStatus);

	if (!linkStatus) { // If the linking failed
		std::cout << "AAMP: Error Linking program\n";
		glDetachShader(aampGLprogramId, vShaderId);
		glDetachShader(aampGLprogramId, fShaderId);
		glDeleteProgram(aampGLprogramId);
	}

	// Get the 'pos' variable location inside this program
	GLuint posAttributePosition = glGetAttribLocation(aampGLprogramId, "pos");

	GLuint vaoId;
	glGenVertexArrays(1, &vaoId); // Generate VAO

	// Bind it so that rest of vao operations affect this vao
	glBindVertexArray(vaoId);

	// buffer from which 'pos' will recive its data and the format of that data
	glBindBuffer(GL_ARRAY_BUFFER, vboId);
	glVertexAttribPointer(posAttributePosition, 3, GL_FLOAT, false, 0, 0);

	// Enable this attribute array linked to 'pos'
	glEnableVertexAttribArray(posAttributePosition);

	// Use this program for rendering.
	glUseProgram(aampGLprogramId);
}

// Function that does the drawing
// glut calls this function whenever it needs to redraw
static void AAMPdisplay()
{
	static pxSharedContextRef sharedContext = context.createSharedContext();
	sharedContext->makeCurrent(true);

	static pxContextFramebufferRef fbo = context.createFramebuffer(pixel_w, pixel_h, false);

	//change fbo width and height if desired
	//context.updateFramebuffer(fbo, static_cast<int>(floor(w)), static_cast<int>(floor(h)));
	if (context.setFramebuffer(fbo) == PX_OK)
	{
		// clear the color buffer before each drawing
		glClear(GL_COLOR_BUFFER_BIT);

		glm::mat4 trans = glm::rotate(
			glm::mat4(1.0f),
			currentAngleOfRotation * 360,
			glm::vec3(1.0f, 1.0f, 1.0f)
		);
		currentAngleOfRotation += 0.001;
		GLint uniTrans = glGetUniformLocation(aampGLprogramId, "trans");
		glUniformMatrix4fv(uniTrans, 1, GL_FALSE, glm::value_ptr(trans));

		// draw triangles starting from index 0 and
		// using 3 indices
		glDrawArrays(GL_TRIANGLES, 0, 3);

		// swap the buffers and hence show the buffers
		// content to the screen
//		glutSwapBuffers();
		std::cout << "AAMP: Rendered.\n";
	}
}

pxVideoImplAAMP::pxVideoImplAAMP(pxScene2d* scene):pxVideo(scene)
{
	AAMPGLinit();
}

pxVideoImplAAMP::~pxVideoImplAAMP()
{
}

void pxVideoImplAAMP::onInit()
{
  rtLogError("%s:%d.",__FUNCTION__,__LINE__);
  bool start = false;
  if((RT_OK == autoPlay(start)) && start)
  {
	  play();
  }
  pxVideo::onInit();
}

void * AAMPRenderer(void *arg)
{
	do
	{
		AAMPdisplay();
		usleep(1000*1000/60);
	}while(true);
	return NULL;
}

rtError pxVideoImplAAMP::play()
{
  rtLogError("%s:%d.",__FUNCTION__,__LINE__);
  rtString manifestUrl;
  if(RT_OK == url(manifestUrl))
  {
//	  Starting dummy thread
	  int ret = pthread_create(&AAMPrenderThreadID, NULL, AAMPRenderer, NULL);
  }
  return RT_OK;
}

rtError pxVideoImplAAMP::pause()
{
  return RT_OK;
}

rtError pxVideoImplAAMP::stop()
{
  //TODO
  return RT_OK;
}



