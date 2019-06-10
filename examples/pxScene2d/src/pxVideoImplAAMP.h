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

// pxVideoImplAAMP.h

#ifndef PX_VIDEO_IMPL_AAMP_H
#define PX_VIDEO_IMPL_AAMP_H

#include <pthread.h>
//#include "main_aamp.h"
#include "pxVideo.h"

class pxVideoImplAAMP: public pxVideo
{
public:
  pxVideoImplAAMP(pxScene2d* scene);
  ~pxVideoImplAAMP();
  void onInit() override;
  rtError play() override;
  rtError pause() override;
  rtError stop() override;
private:
  pthread_t AAMPrenderThreadID;
};

#endif // PX_VIDEO_IMPL_AAMP_H
