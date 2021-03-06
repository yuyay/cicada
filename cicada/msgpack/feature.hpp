// -*- mode: c++ -*-
//
//  Copyright(C) 2012 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__MSGPACK_FEATURE__HPP__
#define __CICADA__MSGPACK_FEATURE__HPP__ 1

#include <utils/config.hpp>

#include <cicada/feature.hpp>

#ifdef HAVE_MSGPACK_HPP

#include <msgpack/object.hpp>

namespace msgpack
{
  inline
  cicada::Feature& operator>>(msgpack::object o, cicada::Feature& v)
  {
    if (o.type != msgpack::type::RAW)
      throw msgpack::type_error();
    v.assign(utils::piece(o.via.raw.ptr, o.via.raw.size));
    return v;
  }
    
  template <typename Stream>
  inline
  msgpack::packer<Stream>& operator<<(msgpack::packer<Stream>& o, const cicada::Feature& v)
  {
    const std::string& str = static_cast<const std::string&>(v);
      
    o.pack_raw(str.size());
    o.pack_raw_body(str.data(), str.size());
    return o;
  }
    
  inline
  void operator<<(msgpack::object::with_zone& o, const cicada::Feature& v)
  {
    const std::string& str = static_cast<const std::string&>(v);
      
    o.type = msgpack::type::RAW;
    char* ptr = (char*)o.zone->malloc(str.size());
    o.via.raw.ptr = ptr;
    o.via.raw.size = (uint32_t) str.size();
    ::memcpy(ptr, str.data(), str.size());
  }

  inline
  void operator<<(msgpack::object& o, const cicada::Feature& v)
  {
    const std::string& str = static_cast<const std::string&>(v);
      
    o.type = msgpack::type::RAW;
    o.via.raw.ptr = str.data();
    o.via.raw.size = (uint32_t) str.size();
  }
    
};

#endif
#endif
