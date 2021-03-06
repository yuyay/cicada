// -*- mode: c++ -*-
//
//  Copyright(C) 2010-2012 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__STEMMER__HPP__
#define __CICADA__STEMMER__HPP__ 1

#include <string>
#include <algorithm>

#include <cicada/symbol.hpp>
#include <cicada/vocab.hpp>

#include <utils/piece.hpp>
#include <utils/array_power2.hpp>

namespace cicada
{
  class Stemmer
  {
  public:
    typedef Symbol    symbol_type;
    typedef Vocab     vocab_type;
    
    typedef symbol_type          word_type;
    typedef symbol_type::id_type id_type;
    
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;

  private:
    typedef std::pair<symbol_type, symbol_type> cache_type;
    typedef utils::array_power2<cache_type, 1024 * 8, std::allocator<cache_type> > cache_set_type;
    
  public:
    Stemmer() {}
    virtual ~Stemmer() {}
    
  private:
    // we do not allow copy/construct
    Stemmer& operator=(const Stemmer& x) { return *this; }
    Stemmer(const Stemmer& x) {}
    
  public:
    static Stemmer&    create(const utils::piece& parameter);
    static const char* lists();
    
  public:
    virtual std::string operator()(const utils::piece& word) const = 0;
    
    std::string operator()(const std::string& word) const
    {
      return operator()(utils::piece(word));
    }
    
    symbol_type operator()(const symbol_type& word) const
    {
      cache_set_type& __caches = const_cast<cache_set_type&>(caches);
      cache_type& cache = __caches[word.id() & (__caches.size() - 1)];
      
      if (cache.first != word) {
	cache.first = word;
	cache.second = operator()(static_cast<const utils::piece&>(word));
      }
      
      return cache.second;
    }
    
    const std::string& algorithm() const { return __algorithm; }

  private:
    cache_set_type caches;

  protected:
    std::string __algorithm;
  };
};

#endif
