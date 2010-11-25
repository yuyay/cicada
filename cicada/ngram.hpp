// -*- mode: c++ -*-
//
//  Copyright(C) 2010 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__NGRAM__HPP__
#define __CICADA__NGRAM__HPP__ 1

#include <stdint.h>

#include <cicada/symbol.hpp>
#include <cicada/vocab.hpp>
#include <cicada/ngram_index.hpp>

#include <boost/array.hpp>

#include <utils/hashmurmur.hpp>
#include <utils/packed_vector.hpp>
#include <utils/succinct_vector.hpp>
#include <utils/map_file.hpp>
#include <utils/mathop.hpp>

namespace cicada
{
  class NGram
  {
  public:
    typedef Symbol             word_type;
    typedef Vocab              vocab_type;
    
    typedef size_t             size_type;
    typedef ptrdiff_t          difference_type;
    
    typedef word_type::id_type id_type;
    typedef uint64_t           count_type;
    typedef float              logprob_type;
    typedef double             prob_type;
    typedef uint8_t            quantized_type;
    
    typedef boost::filesystem::path path_type;

  public:
    struct ShardData
    {
    public:
      typedef utils::map_file<logprob_type, std::allocator<logprob_type> >                 logprob_set_type;
      typedef utils::packed_vector_mapped<quantized_type, std::allocator<quantized_type> > quantized_set_type;
      typedef boost::array<logprob_type, 256>                                              logprob_map_type;
      typedef std::vector<logprob_map_type, std::allocator<logprob_map_type> >             logprob_map_set_type;
      
      ShardData()
	: logprobs(), quantized(), maps(), offset(0) {}
      ShardData(const path_type& path)
      	: logprobs(), quantized(), maps(), offset(0) { open(path); }
      
      void open(const path_type& path);
      path_type path() const { return (quantized.is_open() ? quantized.path().parent_path() : logprobs.path().parent_path()); }
      
      void close() { clear(); }
      void clear()
      {
	logprobs.clear();
	quantized.clear();
	maps.clear();
	offset = 0;
      }
      
      logprob_type operator()(size_type pos, int order) const
      {
	return (quantized.is_open() ? maps[order][quantized[pos - offset]] : logprobs[pos - offset]);
      }
      
      size_type size() const
      {
	return (quantized.is_open() ? quantized.size() + offset : logprobs.size() + offset);
      }

      bool is_quantized() const { return quantized.is_open(); }
            
      logprob_set_type     logprobs;
      quantized_set_type   quantized;
      logprob_map_set_type maps;
      size_type            offset;
    };
    
    typedef ShardData  shard_data_type;
    typedef std::vector<shard_data_type, std::allocator<shard_data_type> > shard_data_set_type;
    typedef NGramIndex shard_index_type;
    
  public:
    NGram(const int _debug=0) : debug(_debug) { clear(); }
    NGram(const path_type& path, const int _debug=0) : debug(_debug) { open(path); }
    
  public:
    static const logprob_type logprob_min() { return boost::numeric::bounds<logprob_type>::lowest(); }
    
    
  public:
    template <typename Iterator>
    std::pair<Iterator, Iterator> ngram_prefix(Iterator first, Iterator last) const
    {
      if (first == last || first + 1 == last) return std::make_pair(first, last);
      
      const size_type shard_index = index.shard_index(first, last);
      std::pair<Iterator, size_type> result = index.traverse(shard_index, first, last);
      
      return std::make_pair(first, std::min(result.first + 1, last));
    }

    template <typename Iterator>
    std::pair<Iterator, Iterator> ngram_suffix(Iterator first, Iterator last) const
    {
      if (first == last || first + 1 == last) return std::make_pair(first, last);
      
      first = std::max(first, last - index.order());
      
      int       shard_prev = -1;
      size_type node_prev = size_type(-1);
      
      for (/**/; first != last - 1; ++ first) {
	const size_type shard_index = index.shard_index(first, last);
	
	std::pair<Iterator, size_type> result = index.traverse(shard_index, first, last, shard_prev, node_prev);
	
	shard_prev = -1;
	node_prev = size_type(-1);
	
	if (result.first == last)
	  return std::make_pair(first, last);
	else if (result.first == last - 1) {
	  shard_prev = shard_index;
	  node_prev = result.second;
	}
      }
      
      return std::make_pair(first, last);
    }

    template <typename Iterator>
    bool exists(Iterator first, Iterator last) const
    {
      if (first == last) return false;
      return index.traverse(first, last).first == last;
    }

    template <typename Iterator>
    logprob_type logbound(Iterator first, Iterator last) const {
      bool estimated = false;
      return logbound(first, last, estimated);
    }
    
    template <typename Iterator>
    logprob_type logbound(Iterator first, Iterator last, bool& estimated) const
    {
      estimated = false;
      
      if (first == last) return 0.0;
      
      const int order = last - first;
      
      if (order >= index.order())
	return logprob(first, last);
      
      if (order >= 2) { 
	const size_type shard_index = index.shard_index(first, last);
	const size_type shard_index_backoff = size_type((order == 2) - 1) & shard_index;
	std::pair<Iterator, size_type> result = index.traverse(shard_index, first, last);
	
	if (result.first == last) {
	  const logprob_type __logbound = (result.second < logbounds[shard_index].size()
					   ? logbounds[shard_index](result.second, order)
					   : logprobs[shard_index](result.second, order));
	  if(__logbound != logprob_min()) {
	    estimated = true;
	    return __logbound;
	  } else {
	    // backoff...
	    const size_type parent = index[shard_index].parent(result.second);
	    const logprob_type logbackoff = (parent != size_type(-1)
					     ? backoffs[shard_index_backoff](parent, order - 1)
					     : logprob_type(0.0));
	    return logprob(first + 1, last) + logbackoff; 
	  }
	} else {
	  // backoff...
	  const logprob_type logbackoff = (result.first == last - 1
					   ? backoffs[shard_index_backoff](result.second, order - 1)
					   : logprob_type(0.0));
	  return logprob(first + 1, last) + logbackoff; 
	}
	
      } else {
	const size_type shard_index = index.shard_index(first, last);
	std::pair<Iterator, size_type> result = index.traverse(shard_index, first, last);
	
	if (result.first == last) {
	  const logprob_type __logbound = (result.second < logbounds[shard_index].size()
					   ? logbounds[shard_index](result.second, order)
					   : logprobs[shard_index](result.second, order));
	  
	  if (__logbound != logprob_min()) {
	    estimated = true;
	    return __logbound;
	  } else
	    return smooth;
	} else
	  return smooth;
      }
    }
    
    template <typename Iterator>
    logprob_type operator()(Iterator first, Iterator last) const
    {
      return logprob(first, last);
    }
    
    template <typename Iterator>
    logprob_type logprob(Iterator first, Iterator last) const
    {
      if (first == last) return 0.0;
      
      first = std::max(first, last - index.order());

      int       shard_prev = -1;
      size_type node_prev = size_type(-1);
      
      logprob_type logbackoff = 0.0;
      for (/**/; first != last - 1; ++ first) {
	const int order = last - first;
	const size_type shard_index = index.shard_index(first, last);
	const size_type shard_index_backoff = size_type((order == 2) - 1) & shard_index;
	
	std::pair<Iterator, size_type> result = index.traverse(shard_index, first, last, shard_prev, node_prev);
	//std::pair<Iterator, size_type> result2 = index.traverse(shard_index, first, last);
	
	shard_prev = -1;
	node_prev = size_type(-1);
	
	if (result.first == last) {
#if 0
	  if (result2.first != result.first)
	    std::cerr << "no iterator match???" << std::endl;
	  if (result2.second != result.second)
	    std::cerr << "no node match???" << std::endl;
#endif
	  
	  const logprob_type __logprob = logprobs[shard_index](result.second, order);
	  if (__logprob != logprob_min())
	    return logbackoff + __logprob;
	  else {
	    const size_type parent = index[shard_index].parent(result.second);
	    if (parent != size_type(-1)) {
	      logbackoff += backoffs[shard_index_backoff](parent, order - 1);
	      
	      shard_prev = shard_index;
	      node_prev = parent;
	    }
	  }
	} else if (result.first == last - 1) {
#if 0
	  if (result2.first != result.first)
	    std::cerr << "no backoff iterator match???" << std::endl;
	  if (result2.second != result.second)
	    std::cerr << "no backoff node match???" << std::endl;
#endif
	  
	  logbackoff += backoffs[shard_index_backoff](result.second, order - 1);
	  
	  shard_prev = shard_index;
	  node_prev = result.second;
	} else {
#if 0
	  if (result2.first == last || result2.first == last - 1)
	    std::cerr << "no match..." << std::endl;
#endif
	}
      }
      
      const int order = last - first;
      const size_type shard_index = index.shard_index(first, last);
      std::pair<Iterator, size_type> result = index.traverse(shard_index, first, last);
      
      return logbackoff + (result.first == last && logprobs[shard_index](result.second, order) != logprob_min()
			   ? logprobs[shard_index](result.second, order)
			   : smooth);
    }
    
    
  public:
    path_type path() const { return index.path().parent_path(); }
    size_type size() const { return index.size(); }
    bool empty() const { return index.empty(); }
    
    void open(const path_type& path);
    
    void close() { clear(); }
    void clear()
    {
      index.clear();
      logprobs.clear();
      backoffs.clear();
      logbounds.clear();
      smooth = utils::mathop::log(1e-7);
      
      bound_exact = false;
    }
    
    bool is_open() const { return index.is_open(); }
    bool has_bounds() const { return ! logbounds.empty(); }
    
  public:
    shard_index_type    index;
    shard_data_set_type logprobs;
    shard_data_set_type backoffs;
    shard_data_set_type logbounds;
    
    logprob_type   smooth;
    bool bound_exact;
    int debug;
  };
  
};

#endif
