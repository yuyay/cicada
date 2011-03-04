//
//  Copyright(C) 2011 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#include <iostream>

#include <cicada/parameter.hpp>
#include <cicada/debinarize.hpp>

#include <cicada/operation/debinarize.hpp>

#include <utils/lexical_cast.hpp>
#include <utils/resource.hpp>
#include <utils/piece.hpp>

namespace cicada
{
  namespace operation
  {
    Debinarize::Debinarize(const std::string& parameter, const int __debug)
      : debug(__debug)
    {
      typedef cicada::Parameter param_type;
      
      param_type param(parameter);
      if (utils::ipiece(param.name()) != "debinarize")
	throw std::runtime_error("this is not a debinarizeor");

      for (param_type::const_iterator piter = param.begin(); piter != param.end(); ++ piter)
	std::cerr << "WARNING: unsupported parameter for debinarize: " << piter->first << "=" << piter->second << std::endl;
      
    }

    void Debinarize::operator()(data_type& data) const
    {
      if (! data.hypergraph.is_valid()) return;
      
      hypergraph_type& hypergraph = data.hypergraph;
      
      hypergraph_type debinarized;

      if (debug)
	std::cerr << "debinarize: " << data.id << std::endl;
      
      utils::resource start;
      
      cicada::debinarize(hypergraph, debinarized);
      
      utils::resource end;
      
      if (debug)
	std::cerr << "debinarize cpu time: " << (end.cpu_time() - start.cpu_time())
		  << " user time: " << (end.user_time() - start.user_time())
		  << std::endl;
      
      if (debug)
	std::cerr << "debinarize: " << data.id
		  << " # of nodes: " << debinarized.nodes.size()
		  << " # of edges: " << debinarized.edges.size()
		  << " valid? " << utils::lexical_cast<std::string>(debinarized.is_valid())
		  << std::endl;
    
      hypergraph.swap(debinarized);
    }

  };
};
