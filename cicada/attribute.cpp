//
//  Copyright(C) 2010-2011 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#include <utils/config.hpp>

#include "attribute.hpp"

namespace cicada
{
  struct AttributeImpl
  {
    typedef Attribute::attribute_set_type attribute_set_type;
    
    static boost::once_flag once;

    static attribute_set_type* attributes;
    
    static void initialize()
    {
      attributes = new attribute_set_type();
    }
  };

  boost::once_flag AttributeImpl::once = BOOST_ONCE_INIT;
  
  AttributeImpl::attribute_set_type* AttributeImpl::attributes = 0;
  
  Attribute::mutex_type    Attribute::__mutex;
  
  Attribute::attribute_set_type& Attribute::__attributes()
  {
    boost::call_once(AttributeImpl::once, AttributeImpl::initialize);
    
    return *AttributeImpl::attributes;
  }

  Attribute::attribute_map_type& Attribute::__attribute_maps()
  {
#ifdef HAVE_TLS
    static __thread attribute_map_type* attribute_maps_tls = 0;
#endif
    static boost::thread_specific_ptr<attribute_map_type> attribute_maps;

#ifdef HAVE_TLS
    if (! attribute_maps_tls) {
      attribute_maps.reset(new attribute_map_type());
      attribute_maps->reserve(allocated());
      
      attribute_maps_tls = attribute_maps.get();
    }
    
    return *attribute_maps_tls;
#else
    if (! attribute_maps.get()) {
      attribute_maps.reset(new attribute_map_type());
      attribute_maps->reserve(allocated());
    }
    
    return *attribute_maps;
#endif
  }
};
