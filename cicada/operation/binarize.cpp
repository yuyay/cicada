//
//  Copyright(C) 2010-2011 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#include <iostream>

#include <cicada/parameter.hpp>
#include <cicada/binarize.hpp>

#include <cicada/operation/binarize.hpp>

#include <utils/lexical_cast.hpp>
#include <utils/resource.hpp>
#include <utils/piece.hpp>

namespace cicada
{
  namespace operation
  {

    Binarize::Binarize(const std::string& parameter, const int __debug)
      : order(-1), horizontal(-1), head(false), label(false),
	left(false), right(false), all(false), cyk(false), dependency(false),
	debug(__debug)
    {
      typedef cicada::Parameter param_type;
    
      param_type param(parameter);
      if (utils::ipiece(param.name()) != "binarize")
	throw std::runtime_error("this is not a binarizer");

      for (param_type::const_iterator piter = param.begin(); piter != param.end(); ++ piter) {
	if (utils::ipiece(piter->first) == "order" || utils::ipiece(piter->first) == "vertical" || utils::ipiece(piter->first) == "order-vertical")
	  order = utils::lexical_cast<int>(piter->second);
	else if (utils::ipiece(piter->first) == "horizontal" || utils::ipiece(piter->first) == "order-horizontal")
	  horizontal = utils::lexical_cast<int>(piter->second);
	else if (utils::ipiece(piter->first) == "head")
	  head = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "label")
	  label = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "direction") {
	  const utils::ipiece dir = piter->second;
	  
	  if (dir == "left")
	    left = true;
	  else if (dir == "right")
	    right = true;
	  else if (dir == "all")
	    all = true;
	  else if (dir == "cyk" || dir == "cky")
	    cyk = true;
	  else if (dir == "dep" || dir == "dependency")
	    dependency = true;
	  else
	    throw std::runtime_error("unuspported direction: " + parameter);
	} else
	  std::cerr << "WARNING: unsupported parameter for binarize: " << piter->first << "=" << piter->second << std::endl;
      }

      if (int(left) + right + all + cyk + dependency == 0)
	throw std::runtime_error("what direction? left, right, all, cyk or dependency");
      
      if (int(left) + right + all + cyk + dependency > 1)
	throw std::runtime_error("we do not binarization in many directions!");

      std::string dep_suffix = "";
      if (head)
	dep_suffix += "-head";
      if (label)
	dep_suffix += "-label";

      name = (std::string("binarize-")
	      + (left
		 ? std::string("left")
		 : (right
		    ? std::string("right")
		    : (all
		       ? std::string("all")
		       : (cyk
			  ? std::string("cyk")
			  : "dependency-head" + dep_suffix)))));
    }

    void Binarize::operator()(data_type& data) const
    {
      if (! data.hypergraph.is_valid()) return;
      
      hypergraph_type binarized;
      
      if (debug)
	std::cerr << name << ": " << data.id << std::endl;
      
      utils::resource start;
    
      if (left)
	cicada::binarize_left(data.hypergraph, binarized, order);
      else if (right)
	cicada::binarize_right(data.hypergraph, binarized, order);
      else if (all)
	cicada::binarize_all(data.hypergraph, binarized);
      else if (cyk)
	cicada::binarize_cyk(data.hypergraph, binarized, order, horizontal);
      else if (dependency)
	cicada::binarize_dependency(data.hypergraph, binarized, head, label);
      else
	throw std::runtime_error("unsupported direction!");
    
      utils::resource end;
    
      if (debug)
	std::cerr << name << ": " << data.id
		  << " cpu time: " << (end.cpu_time() - start.cpu_time())
		  << " user time: " << (end.user_time() - start.user_time())
		  << " thread time: " << (end.thread_time() - start.thread_time())
		  << std::endl;
    
      if (debug)
	std::cerr << name << ": " << data.id
		  << " # of nodes: " << binarized.nodes.size()
		  << " # of edges: " << binarized.edges.size()
		  << " valid? " << utils::lexical_cast<std::string>(binarized.is_valid())
		  << std::endl;

      statistics_type::statistic_type& stat = data.statistics[name];
      
      ++ stat.count;
      stat.node += binarized.nodes.size();
      stat.edge += binarized.edges.size();
      stat.user_time += (end.user_time() - start.user_time());
      stat.cpu_time  += (end.cpu_time() - start.cpu_time());
      stat.thread_time  += (end.thread_time() - start.thread_time());
      
      data.hypergraph.swap(binarized);
    }
    
  };
};
