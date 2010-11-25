// -*- mode: c++ -*-
//
//  Copyright(C) 2010 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__STEMMER_LOWER__HPP__
#define __CICADA__STEMMER_LOWER__HPP__ 1

#include <vector>

#include <cicada/stemmer.hpp>

namespace cicada
{
  namespace stemmer
  {
    class Lower : public Stemmer
    {
    private:
      typedef std::vector<symbol_type, std::allocator<symbol_type> > symbol_set_type;
      
    public:
      Lower();
      ~Lower();
      
    public:
      symbol_type operator[](const symbol_type& x) const;
      
    private:
      symbol_set_type cache;
      void*           pimpl;
    };
  };
};

#endif
