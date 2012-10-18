//
//  Copyright(C) 2010-2012 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#include <iterator>
#include <iostream>

#include "tree_rule.hpp"
#include "tree_rule_compact.hpp"

#ifdef HAVE_MSGPACK_HPP
#include <msgpack.hpp>
#include <cicada/msgpack/tree_rule.hpp>
#endif

typedef cicada::TreeRule rule_type;


void pre_order(const rule_type& rule)
{
  std::cout << "label: " << rule.label << std::endl;
  for (rule_type::antecedent_set_type::const_iterator riter = rule.antecedents.begin(); riter != rule.antecedents.end(); ++ riter)
    pre_order(*riter);
}

void post_order(const rule_type& rule)
{
  for (rule_type::antecedent_set_type::const_iterator riter = rule.antecedents.begin(); riter != rule.antecedents.end(); ++ riter)
    post_order(*riter);
  std::cout << "label: " << rule.label << std::endl;
}

size_t rule_bytes(const rule_type& rule)
{
  size_t bytes = sizeof(rule.label) + sizeof(void*);
  for (rule_type::antecedent_set_type::const_iterator riter = rule.antecedents.begin(); riter != rule.antecedents.end(); ++ riter)
    bytes += rule_bytes(*riter);
  return bytes;
}

void compact(const rule_type& rule)
{
  cicada::TreeRuleCompact compact(rule);
  
  rule_type decoded(compact.decode());
  
  if (decoded != rule)
    std::cerr << "rule differ?" << std::endl;

  std::cout << "raw: " << rule_bytes(rule) << std::endl;
  std::cout << "compressed: " << compact.size_compressed() << std::endl;
}

void process(const utils::piece& rule_str)
{
  const rule_type rule(rule_str);

  std::cerr << "trivial assign: " << boost::has_trivial_assign<rule_type>::value << std::endl
	    << "trivial construct: " << boost::has_trivial_constructor<rule_type>::value << std::endl
	    << "trivial copy: " << boost::has_trivial_copy<rule_type>::value << std::endl
	    << "trivial copy-construct: " << boost::has_trivial_copy_constructor<rule_type>::value << std::endl
	    << "trivial default-construct: " << boost::has_trivial_default_constructor<rule_type>::value << std::endl
	    << "trivial destructor: " << boost::has_trivial_destructor<rule_type>::value << std::endl;
  
  std::cout << "string: " << rule_str << std::endl;
  std::cout << "tree rule: " << rule << std::endl;
  
  std::cout << "pre-order" << std::endl;
  pre_order(rule);
  std::cout << "post-order" << std::endl;
  post_order(rule);
  
  compact(rule);
  
  std::cout << "frontier: ";
  std::ostream_iterator<std::string> iter(std::cout, " ");
  rule.frontier(iter);
  std::cout << std::endl;

  std::cout << "internal size: " << rule.size_internal() << std::endl;
  std::cout << "frontier size: " << rule.size_frontier() << std::endl;
}

int main(int argc, char** argv)
{
  process("a");
  process("a(b c d)");
  process("a(b(e) c(f g) d)");
  process("a(b(e) c(f g) \\()");
  process("a(b(e) \\)(f g) \\()");
  process(" ");
  //process(" ||| ");

  std::string line;
  while (std::getline(std::cin, line))
    process(line);
}
