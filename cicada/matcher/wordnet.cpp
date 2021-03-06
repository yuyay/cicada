//
//  Copyright(C) 2010-2011 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#include <cstdlib>
#include <stdexcept>

#include "cicada/matcher/wordnet.hpp"

#include "wn/wordnet.hpp"

namespace cicada
{
  namespace matcher
  {
    template <typename Iterator1, typename Iterator2>
    inline
    bool __match(Iterator1 first1, Iterator1 last1, Iterator2 first2, Iterator2 last2)
    {
      if (first2 != last2)
	first1 = std::lower_bound(first1, last1, *first2);
      if (first1 != last1)
	first2 = std::lower_bound(first2, last2, *first1);
      
      while (first1 != last1 && first2 != last2) {
	if (*first1 < *first2)
	  ++ first1;
	else if (*first2 < *first1)
	  ++ first2;
	else
	  return true;
      }
      return false;
    }
    
    bool WordNet::operator()(const symbol_type& x, const symbol_type& y) const
    {
      if (x == y) return true;
      if (x.empty() || y.empty()) return false;
      
      const size_type pos_x = hash_value(x) & (caches.size() - 1);
      const size_type pos_y = hash_value(y) & (caches.size() - 1);

      if (pos_x != pos_y) {
	cache_type& cache_x = const_cast<cache_type&>(caches[pos_x]);
	cache_type& cache_y = const_cast<cache_type&>(caches[pos_y]);
	
	if (cache_x.word != x) {
	  cache_x.word = x;
	  wordnet(x, cache_x.synsets);
	  std::sort(cache_x.synsets.begin(), cache_x.synsets.end());
	}
	
	if (cache_y.word != y) {
	  cache_y.word = y;
	  wordnet(y, cache_y.synsets);
	  std::sort(cache_y.synsets.begin(), cache_y.synsets.end());
	}

	return __match(cache_x.synsets.begin(), cache_x.synsets.end(), cache_y.synsets.begin(), cache_y.synsets.end());
      } else {
	// check if we have an entry in cache... otherwise...

	synset_set_type synsets;
	cache_type& cache = const_cast<cache_type&>(caches[pos_x]);
	
	if (cache.word == x) {
	  wordnet(y, synsets);
	  std::sort(synsets.begin(), synsets.end());

	  return __match(cache.synsets.begin(), cache.synsets.end(), synsets.begin(), synsets.end());
	} else if (cache.word == y) {
	  wordnet(x, synsets);
	  std::sort(synsets.begin(), synsets.end());
	  
	  return __match(cache.synsets.begin(), cache.synsets.end(), synsets.begin(), synsets.end());
	} else {
	  cache.word = x;
	  wordnet(x, cache.synsets);
	  std::sort(cache.synsets.begin(), cache.synsets.end());
	  
	  wordnet(y, synsets);
	  std::sort(synsets.begin(), synsets.end());
	  
	  return __match(cache.synsets.begin(), cache.synsets.end(), synsets.begin(), synsets.end());
	}
      }
    }
  };
};
