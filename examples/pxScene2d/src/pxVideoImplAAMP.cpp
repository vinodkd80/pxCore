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

#include <gst/gst.h>
#include "pxVideoImplAAMP.h"


static bool initialized = false;
GThread *aampMainLoopThread = NULL;
static GMainLoop *AAMPGstPlayerMainLoop = NULL;
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


pxVideoImplAAMP::pxVideoImplAAMP(pxScene2d* scene):pxVideo(scene)
{
  InitPlayerLoop();
  mAamp = new PlayerInstanceAAMP();
  assert (nullptr != mAamp);
}

pxVideoImplAAMP::~pxVideoImplAAMP()
{
  delete mAamp;
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

rtError pxVideoImplAAMP::play()
{
  rtLogError("%s:%d.",__FUNCTION__,__LINE__);
  rtString manifestUrl;
  if(RT_OK == url(manifestUrl))
  {
	  mAamp->Tune(manifestUrl.cString());
  }
  return RT_OK;
}

rtError pxVideoImplAAMP::pause()
{
   mAamp->Stop();
  return RT_OK;
}

rtError pxVideoImplAAMP::stop()
{
  //TODO
  return RT_OK;
}



