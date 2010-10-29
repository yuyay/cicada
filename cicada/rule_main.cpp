
#include <iostream>

#include "rule.hpp"

int main(int argc, char** argv)
{
  typedef cicada::Rule rule_type;
  

  std::cout << "rule: " << rule_type("good boy") << std::endl;
  
  std::cout << "rule: " << rule_type("[s] ||| good boy") << std::endl;
  
  std::cout << "rule: " << rule_type("[s] ||| good boy [x,1]") << std::endl;
}
