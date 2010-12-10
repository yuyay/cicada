// -*- mode: c++ -*-
//
//  Copyright(C) 2010 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__MATCHER__WORDNET__HPP__
#define __CICADA__MATCHER__WORDNET__HPP__ 1

#include <string>

#include <cicada/wordnet.hpp>
#include <cicada/matcher.hpp>

namespace cicada
{
  namespace matcher
  {
    class Wordnet : public cicada::Matcher
    {
    public:
      Wordnet(const std::string& path) : { initialize(path); }
      
    public:
      bool operator()(const symbol_type& x, const symbol_type& y) const
      {
	
	return true;
      }
      
    private:
      
      static void initialize(const std::string& path);
    };
  };
};

#endif
