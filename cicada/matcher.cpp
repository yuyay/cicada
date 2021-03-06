//
//  Copyright(C) 2010-2012 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#include "matcher.hpp"

#include "parameter.hpp"

#include "matcher/lower.hpp"
#include "matcher/stemmer.hpp"
#include "matcher/wordnet.hpp"

#include <utils/unordered_map.hpp>
#include <utils/thread_specific_ptr.hpp>
#include <utils/piece.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

namespace cicada
{
  const char* Matcher::lists()
  {
    static const char* desc = "\
lower: matching by lower-case\n\
stemmer: matching by stemming algorithm\n\
\talgorithm=[stemmer spec]\n\
wordnet: matching by wordnet synsets\n\
\tpath=[path to wordnet database]\n\
";
    return desc;
  }

  typedef boost::shared_ptr<Matcher> matcher_ptr_type;

  typedef utils::unordered_map<std::string, matcher_ptr_type, boost::hash<utils::piece>, std::equal_to<std::string>,
			       std::allocator<std::pair<const std::string, matcher_ptr_type> > >::type matcher_map_type;

#ifdef HAVE_TLS
  static __thread matcher_map_type* __matchers_tls = 0;
  static utils::thread_specific_ptr<matcher_map_type> __matchers;
#else
  static utils::thread_specific_ptr<matcher_map_type> __matchers;
#endif

  
  Matcher& Matcher::create(const utils::piece& parameter)
  {
    typedef cicada::Parameter parameter_type;
    
#ifdef HAVE_TLS
    if (! __matchers_tls) {
      __matchers.reset(new matcher_map_type());
      __matchers_tls = __matchers.get();
    }
    matcher_map_type& matchers_map = *__matchers_tls;    
#else
    if (! __matchers.get())
      __matchers.reset(new matcher_map_type());
    
    matcher_map_type& matchers_map = *__matchers;
#endif
    
    const parameter_type param(parameter);

    if (utils::ipiece(param.name()) == "lower") {
      const std::string name("lower");
      
      matcher_map_type::iterator iter = matchers_map.find(name);
      if (iter == matchers_map.end()) {
	iter = matchers_map.insert(std::make_pair(name, matcher_ptr_type(new matcher::Lower()))).first;
	iter->second->__algorithm = parameter;
      }
      
      return *(iter->second);
    } else if (utils::ipiece(param.name()) == "stemmer") {
      std::string algorithm;
      
      for (parameter_type::const_iterator piter = param.begin(); piter != param.end(); ++ piter) {
	if (utils::ipiece(piter->first) == "algorithm")
	  algorithm = piter->second;
	else
	  std::cerr << "unsupported parameter for stemming matcher: " << piter->first << "=" << piter->second << std::endl;
      }
      
      if (algorithm.empty())
	throw std::runtime_error("no stemming algorithm?");
      
      const std::string name = "stemmer:" + algorithm;
      
      matcher_map_type::iterator iter = matchers_map.find(name);
      if (iter == matchers_map.end()) {
	iter = matchers_map.insert(std::make_pair(name, matcher_ptr_type(new matcher::Stemmer(&cicada::Stemmer::create(algorithm))))).first;
	iter->second->__algorithm = parameter;
      }
      
      return *(iter->second);
    } else if (utils::ipiece(param.name()) == "wordnet" || utils::ipiece(param.name()) == "wn") {
      std::string path;
      
      for (parameter_type::const_iterator piter = param.begin(); piter != param.end(); ++ piter) {
	if (utils::ipiece(piter->first) == "path")
	  path = piter->second;
	else
	  std::cerr << "unsupported parameter for wordnet matcher: " << piter->first << "=" << piter->second << std::endl;
      }
      
      const std::string name("wordnet");
      matcher_map_type::iterator iter = matchers_map.find(name);
      if (iter == matchers_map.end()) {
	iter = matchers_map.insert(std::make_pair(name, matcher_ptr_type(new matcher::WordNet(path)))).first;
	iter->second->__algorithm = parameter;
      }
      
      return *(iter->second);
    } else
      throw std::runtime_error("invalid parameter: " + parameter);
  }

};
