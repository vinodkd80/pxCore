#include "rtRpcTransport.h"

rtError
rtValueReader::read(rtValue& to, rapidjson::Document const& doc)
{
  rapidjson::Value const& from = doc["value"];

  rtType const type = static_cast<rtType>(from["type"].GetInt());
  switch (type)
  {
    case RT_voidType:
    to = rtValue();
    break;

    case RT_valueType:
    assert(false);
    break;

    case RT_boolType:
    to = rtValue(from["value"].GetBool());
    break;

    case RT_int8_tType:
    to.setInt8(static_cast<int8_t>(from["value"].GetInt()));
    break;

    case RT_uint8_tType:
    to.setUInt8(static_cast<uint8_t>(from["value"].GetInt()));
    break;

    case RT_int32_tType:
    to.setInt32(static_cast<int32_t>(from["value"].GetInt()));
    break;

    case RT_uint32_tType:
    to.setUInt32(static_cast<uint32_t>(from["value"].GetUint()));
    break;

    case RT_int64_tType:
    to.setInt64(static_cast<int64_t>(from["value"].GetInt64()));
    break;
    
    case RT_uint64_tType:
    to.setUInt64(static_cast<uint64_t>(from["value"].GetUint64()));
    break;

    case RT_floatType:
    to.setFloat(static_cast<float>(from["value"].GetDouble()));
    break;
    
    case RT_doubleType:
    to.setFloat(static_cast<double>(from["value"].GetDouble()));
    break;

    case RT_stringType:
    {
      rtString s(from["value"].GetString());
      to.setString(s);
    }
    break;

    case RT_objectType:
    assert(false);
    break;

    case RT_functionType:
    assert(false);
    break;

    case RT_voidPtrType:
    assert(false);
    break;
  }

  return RT_OK;
}
