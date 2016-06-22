// rtCore Copyright 2007-2015 John Robinson
// rtError.cpp

#include "rtError.h"
 #include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define RT_ERROR_CASE(ERR) case ERR: s = # ERR; break;
#define RT_ERROR_CLASS(ERR) (((ERR) >> 16) & 0x0000ffff)
#define RT_ERROR_CODE(ERR) ((ERR) & 0x0000ffff)

static pthread_once_t once = PTHREAD_ONCE_INIT;
static pthread_key_t key;

static const char* rtStrError_BuiltIn(rtError e);
static const char* rtStrError_SystemError(int e);

void rtErrorInitThreadSpecificKey()
{
  pthread_key_create(&key, NULL);
}

const char* rtStrError(rtError e)
{
  const char* s = NULL;

  int klass = RT_ERROR_CLASS(e);
  switch (klass)
  {
    case RT_ERROR_CLASS_BUILTIN:
    s = rtStrError_BuiltIn(RT_ERROR_CODE(e));
    break;

    case RT_ERROR_CLASS_SYSERROR:
    s = rtStrError_SystemError(RT_ERROR_CODE(e));
    break;
  }
  return s;
}

const char* rtStrError_SystemError(int e)
{
  pthread_once(&once, rtErrorInitThreadSpecificKey);

  char* buff = reinterpret_cast<char *>(pthread_getspecific(key));
  if (buff == NULL)
  {
    buff = reinterpret_cast<char *>(malloc(256));
    if (buff)
      buff[0] = '\0';
    pthread_setspecific(key, buff);
  }

  assert(buff != NULL);
  if (buff)
    buff = strerror_r(e, buff, 256);

  return buff;
}

const char* rtStrError_BuiltIn(rtError e)
{
  const char* s = "UNKNOWN";
  switch (e)
  {
    RT_ERROR_CASE(RT_OK);
    RT_ERROR_CASE(RT_FAIL);
    RT_ERROR_CASE(RT_ERROR_NOT_ENOUGH_ARGS);
    RT_ERROR_CASE(RT_ERROR_INVALID_ARG);
    RT_ERROR_CASE(RT_PROP_NOT_FOUND);
    RT_ERROR_CASE(RT_OBJECT_NOT_INITIALIZED);
    RT_ERROR_CASE(RT_PROPERTY_NOT_FOUND);
    RT_ERROR_CASE(RT_OBJECT_NO_LONGER_AVAILABLE);
    RT_ERROR_CASE(RT_RESOURCE_NOT_FOUND);
    RT_ERROR_CASE(RT_NO_CONNECTION);
    RT_ERROR_CASE(RT_TIMEOUT);
    default:
      break;
  }
  return s;
}

