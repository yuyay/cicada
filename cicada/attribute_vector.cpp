//
//  Copyright(C) 2010 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#include "attribute_vector.hpp"

namespace cicada
{
  
  // format????
  // attribute value
  
  std::ostream& operator<<(sstd::ostream& os, const AttributeVector& x)
  {
    
    return os;
  }

  std::istream& operator>>(std::istream& is, AttributeVector& x)
  {
    return is;
  }
};
