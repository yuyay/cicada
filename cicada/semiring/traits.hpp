// -*- mode: c++ -*-

#ifndef __CICADA__SEMIRING__TRAITS__HPP__
#define __CICADA__SEMIRING__TRAITS__HPP__ 1

#include <boost/numeric/conversion/bounds.hpp>

namespace cicada
{
  namespace semiring
  {
    template <typename Tp>
    struct traits
    {
      static inline Tp zero() { return Tp();  }
      static inline Tp one()  { return Tp(1); }
    };

  };
};


#endif
