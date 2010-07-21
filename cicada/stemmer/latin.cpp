
#include "stemmer/latin.hpp"

#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/bytestream.h>
#include <unicode/translit.h>

namespace cicada
{

  namespace stemmer
  {
    class AnyLatin
    {
    private:
      Transliterator* trans;
    
    public:
      AnyLatin() : trans(0) { open(); }
      ~AnyLatin() { close(); }
    
    private:
      AnyLatin(const AnyLatin& x) { }
      AnyLatin& operator=(const AnyLatin& x) { return *this; }
    
    public:
      void operator()(UnicodeString& data) { trans->transliterate(data); }
    
    private:
      void open()
      {
	close();
      
	__initialize();
      
	UErrorCode status = U_ZERO_ERROR;
	trans = Transliterator::createInstance(UnicodeString::fromUTF8("AnyLatinNoAccents"),
					       UTRANS_FORWARD,
					       status);
	if (U_FAILURE(status))
	  throw std::runtime_error(std::string("transliterator::create_instance(): ") + u_errorName(status));
      
      }
    
      void close()
      {
	if (trans) 
	  delete trans;
	trans = 0;
      }
    
    private:
      void __initialize()
      {
	static bool __initialized = false;
      
	if (__initialized) return;
      
	// Any-Latin, NFKD, remove accents, NFKC
	UErrorCode status = U_ZERO_ERROR;
	UParseError status_parse;
	Transliterator* __trans = Transliterator::createFromRules(UnicodeString::fromUTF8("AnyLatinNoAccents"),
								  UnicodeString::fromUTF8(":: Any-Latin; :: NFKD; [[:Z:][:M:][:C:]] > ; :: NFKC;"),
								  UTRANS_FORWARD, status_parse, status);
	if (U_FAILURE(status))
	  throw std::runtime_error(std::string("transliterator::create_from_rules(): ") + u_errorName(status));
      
	// register here...
	Transliterator::registerInstance(__trans);
      
	__initialized = true;
      }
    };

    Stemmer::symbol_type Latin::operator[](const symbol_type& word) const
    {
      if (word == vocab_type::EMPTY || word.is_non_terminal()) return word;
    
      const size_type word_size = word.size();
    
      // SGML-like symbols are not prefixed
      if (word_size >= 3 && word[0] == '<' && word[word_size - 1] == '>')
	return word;
    
      symbol_set_type& __cache = const_cast<symbol_set_type&>(cache);
    
      if (word.id() >= __cache.size())
	__cache.resize(word.id() + 1, vocab_type::EMPTY);
    
      if (__cache[word.id()] == vocab_type::EMPTY) {
#ifdef HAVE_TLS
	static __thread AnyLatin* __any_latin_tls = 0;
	static boost::thread_specific_ptr<AnyLatin> __any_latin_specific;
      
	if (! __any_latin_tls) {
	  __any_latin_specific.reset(new AnyLatin());
	  __any_latin_tls = __any_latin_specific.get();
	}
      
	AnyLatin& any_latin = *__any_latin_tls;
#else
	static boost::thread_specific_ptr<AnyLatin> __any_latin_specific;
      
	if (! __any_latin_specific.get())
	  __any_latin_specific.reset(new AnyLatin());
      
	AnyLatin& any_latin = *__any_latin_specific;
#endif
      
	UnicodeString uword = UnicodeString::fromUTF8(static_cast<const std::string&>(word));
      
	any_latin(uword);
      
	if (! uword.isEmpty()) {
	  std::string word_latin;
	  StringByteSink<std::string> __sink(&word_latin);
	  uword.toUTF8(__sink);
	
	  __cache[word.id()] = word_latin;
	} else
	  __cache[word.id()] = word;
      }
    
      return __cache[word.id()];
    }

  };
};
