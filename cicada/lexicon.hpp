// -*- mode: c++ -*-
//
//  Copyright(C) 2011 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__LEXICON__HPP__
#define __CICADA__LEXICON__HPP__ 1

//
// lexical table
//

#include <stdint.h>

#include <cicada/symbol.hpp>
#include <cicada/vocab.hpp>

#include <succinct_db/succinct_trie_db.hpp>

#include <boost/filesystem/path.hpp>

namespace cicada
{
  class Lexicon : public utils::hashmurmur<uint64_t>
  {
  public:
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;
    
    typedef Symbol             word_type;
    typedef Vocab              vocab_type;
    typedef word_type::id_type word_id_type;

    typedef float weight_type;
    
    typedef uint64_t                           hash_value_type;
    typedef utils::hashmurmur<hash_value_type> hasher_type;

    typedef boost::filesystem::path path_type;
    
  private:
    typedef word_id_type key_type;
    typedef weight_type  mapped_type;
    
    typedef std::allocator<std::pair<key_type, mapped_type> >  lexicon_alloc_type;
    typedef succinctdb::succinct_trie_db<key_type, mapped_type, lexicon_alloc_type > lexicon_type;
    typedef lexicon_type::size_type node_type;
    
  public:
    Lexicon() : lexicon(), vocab(), smooth(1e-40) {}
    Lexicon(const std::string& path) { open(path); }
    
  public:
    
    weight_type operator()(const word_type& prev, const word_type& word) const
    {
      return operator()(&prev, (&prev) + 1, word);
    }

    template <typename Iterator>
    weight_type operator()(Iterator first, Iterator last, const word_type& word) const
    {
      node_type node = traverse(first, last);
      
      if (! lexicon.is_valid(node)) return smooth;
      
      node = traverse(node, word);
      
      return (lexicon.is_valid(node) && lexicon.exists(node) ? lexicon[node] : smooth);
    }

    bool exists(const word_type& word) const
    {
      return exists(&word, (&word) + 1);
    }
        
    template <typename Iterator>
    bool exists(Iterator first, Iterator last) const
    {
      const node_type node = traverse(first, last);
      return lexicon.is_valid(node);
    }
    
    template <typename Iterator>
    bool exists(Iterator first, Iterator last, const word_type& word) const
    {
      node_type node = traverse(first, last);
      
      if (! lexicon.is_valid(node)) return false;
      
      node = traverse(node, word);
      
      return lexicon.is_valid(node) && lexicon.exists(node);
    }

  private:
    node_type traverse(node_type node, const word_type& word) const
    {
      if (! lexicon.is_valid(node)) return node;
      
      const word_id_type word_id = vocab[word];
      
      return lexicon.find(&word_id, 1, node);
    }

    template <typename Iterator>
    node_type traverse(Iterator first, Iterator last) const
    {
      node_type node = 0;
      for (/**/; first != last; ++ first) {
	const word_id_type word_id = vocab[*first];
	
	node = lexicon.find(&word_id, 1, node);
	
	if (! lexicon.is_valid(node))
	  return node;
      }
      return node;
    }

    
  public:
    void open(const std::string& path);
    void write(const path_type& file) const;
    
    void close() { clear(); }
    void clear()
    {
      lexicon.clear();
      vocab.clear();
      smooth = 1e-40;
    }

    path_type path() const { return lexicon.path().parent_path(); }
    bool empty() const { return lexicon.empty(); }

  public:
    static Lexicon& create(const std::string& path);
    
  private:
    lexicon_type lexicon;
    vocab_type   vocab;
    weight_type  smooth;
  };
};

#endif
