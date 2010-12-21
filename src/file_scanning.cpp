// Copyright 2008 John Maddock
//
// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "auto_index.hpp"

bool need_defaults = true;

void install_default_scanners()
{
   need_defaults = false;
   //
   // Set the default scanners if they're not defined already:
   //
   file_scanner s;
   s.type = "class_name";
   if(file_scanner_set.find(s) == file_scanner_set.end())
   {
      add_file_scanner(
         "class_name",  // Index type
         // Header file scanner regex:
         // possibly leading whitespace:   
         "^[[:space:]]*" 
         // possible template declaration:
         "(template[[:space:]]*<[^;:{]+>[[:space:]]*)?"
         // class or struct:
         "(class|struct)[[:space:]]*" 
         // leading declspec macros etc:
         "("
            "\\<\\w+\\>"
            "("
               "[[:blank:]]*\\([^)]*\\)"
            ")?"
            "[[:space:]]*"
         ")*" 
         // the class name
         "(\\<\\w*\\>)[[:space:]]*" 
         // template specialisation parameters
         "(<[^;:{]+>)?[[:space:]]*"
         // terminate in { or :
         "(\\{|:[^;\\{()]*\\{)",

         "class[^;{]+\\\\<\\5\\\\>[^;{]+\\\\{",  // Format string to create indexing regex.
         "\\5",   // Format string to create index term.
         "",  // Filter regex for section id's.
         ""   // Filter regex for filenames.
         );
   }

   s.type = "typedef_name";
   if(file_scanner_set.find(s) == file_scanner_set.end())
   {
      add_file_scanner(
         "typedef_name",  // Index type
         "typedef[^;{}#]+?(\\w+)\\s*;", // scanner regex
         "typedef[^;]+\\\\<\\1\\\\>\\\\s*;",  // Format string to create indexing regex.
         "\\1",   // Format string to create index term.
         "",  // Filter regex for section id's.
         ""   // Filter regex for filenames.
         );
   }

   s.type = "macro_name";
   if(file_scanner_set.find(s) == file_scanner_set.end())
   {
      add_file_scanner(
         "macro_name",  // Index type
         "^\\s*#\\s*define\\s+(\\w+)", // scanner regex
         "\\\\<\\1\\\\>",  // Format string to create indexing regex.
         "\\1",   // Format string to create index term.
         "",  // Filter regex for section id's.
         ""   // Filter regex for filenames.
         );
   }

   s.type = "function_name";
   if(file_scanner_set.find(s) == file_scanner_set.end())
   {
      add_file_scanner(
         "function_name",  // Index type
         "\\w+\\s+(\\w+)\\s*\\([^\\)]*\\)\\s*[;{]", // scanner regex
         "\\\\<\\\\w+\\\\>\\\\s+\\\\<\\1\\\\>\\\\s*\\\\([^;{]*\\\\)\\\\s*[;{]",  // Format string to create indexing regex.
         "\\1",   // Format string to create index term.
         "",  // Filter regex for section id's.
         ""   // Filter regex for filenames.
         );
   }
}

//
// Helper to dump file contents into a std::string:
//
void load_file(std::string& s, std::istream& is)
{
   s.erase();
   if(is.bad()) return;
   s.reserve(is.rdbuf()->in_avail());
   char c;
   while(is.get(c))
   {
      if(s.capacity() == s.size())
         s.reserve(s.capacity() * 3);
      s.append(1, c);
   }
}
//
// Scan a source file for things to index:
//
void scan_file(const char* file)
{
   if(need_defaults)
      install_default_scanners();
   if(verbose)
      std::cout << "Scanning file... " << file << std::endl;
   std::string text;
   std::ifstream is(file);
   load_file(text, is);

   for(file_scanner_set_type::iterator pscan = file_scanner_set.begin(); pscan != file_scanner_set.end(); ++pscan)
   {
      if(!pscan->file_name_filter.empty())
      {
         if(!regex_match(file, pscan->file_name_filter))
            continue;  // skip this file
      }
      if(verbose)
         std::cout << "Scanning for type \"" << (*pscan).type << "\" ... " << std::endl;
      boost::sregex_iterator i(text.begin(), text.end(), (*pscan).scanner), j;
      while(i != j)
      {
         try
         {
            index_info info;
            info.term = i->format(pscan->term_formatter);
            info.search_text = i->format(pscan->format_string);
            info.category = pscan->type;
            if(!pscan->section_filter.empty())
               info.search_id = pscan->section_filter;
            if(index_terms.count(info) == 0)
            {
               if(verbose)
                  std::cout << "Indexing " << info.term << " as type " << info.category << std::endl;
               index_terms.insert(info);
            }
         }
         catch(const boost::regex_error& e)
         {
            std::cerr << "Unable to create regular expression from found index term:\""
               << i->format(pscan->term_formatter) << "\" In file " << file << std::endl;
            std::cerr << e.what() << std::endl;
         }
         catch(const std::exception& e)
         {
            std::cerr << "Unable to create index term:\""
               << i->format(pscan->term_formatter) << "\" In file " << file << std::endl;
            std::cerr << e.what() << std::endl;
            throw;
         }
         ++i;
      }
   }
}
//
// Scan a whole directory for files to search:
//
void scan_dir(const std::string& dir, const std::string& mask, bool recurse)
{
   using namespace boost::filesystem;
   boost::regex e(mask);
   directory_iterator i(dir), j;

   while(i != j)
   {
      if(regex_match(i->path().filename().string(), e))
      {
         scan_file(i->path().string().c_str());
      }
      else if(recurse && is_directory(i->status()))
      {
         scan_dir(i->path().string(), mask, recurse);
      }
      ++i;
   }
}
//
// Remove quotes from a string:
//
std::string unquote(const std::string& s)
{
   std::string result(s);
   if((s.size() >= 2) && (*s.begin() == '\"') && (*s.rbegin() == '\"'))
   {
      result.erase(result.begin());
      result.erase(result.end() - 1);
   }
   return result;
}
//
// Load and process a script file:
//
void process_script(const char* script)
{
   static const boost::regex comment_parser(
      "\\s*(?:#.*)?$"
      );
   static const boost::regex scan_parser(
      "!scan[[:space:]]+"
      "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")\\s*"
      );
   static const boost::regex scan_dir_parser(
      "!scan-path[[:space:]]+"
      "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")"
      "[[:space:]]+"
      "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")"
      "(?:"
         "[[:space:]]+"
         "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")"
      ")?\\s*"
      );
   static const boost::regex entry_parser( 
      "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")"
      "(?:"
         "[[:space:]]+"
         "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)*\")"
         "(?:"
            "[[:space:]]+"
            "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)*\")"
            "(?:"
               "[[:space:]]+"
               "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)*\")"
            ")?"
         ")?"
      ")?"
      "[[:space:]]*");
   static const boost::regex rewrite_parser(
      "!(rewrite-name|rewrite-id)\\s+"
      "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")\\s+"
      "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")\\s*"
      );
   static const boost::regex debug_parser(
      "!debug\\s+"
      "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")\\s*"
      );
   static const boost::regex define_scanner_parser(
      "!define-scanner\\s+"
      "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")\\s+"  // type, index 1
      "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")\\s+"  // scanner regex, index 2
      "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")\\s+"  // format string, index 3
      "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")\\s+"  // format string for name, index 4
      "(?:"
         "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")\\s+" // id-filter, index 5
         "(?:"
            "([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")\\s+" // filename-filter, index 6
         ")?"
      ")?"
      );

   if(verbose)
      std::cout << "Processing script " << script << std::endl;
   boost::smatch what;
   std::string line;
   std::ifstream is(script);
   if(is.bad())
   {
      throw std::runtime_error("Could not open script file");
   }
   while(std::getline(is, line).good())
   {
      if(regex_match(line, what, comment_parser))
      {
         // Nothing to do here...
      }
      else if(regex_match(line, what, scan_parser))
      {
         std::string f = unquote(what[1].str());
         if(!boost::filesystem::path(f).is_complete())
         {
            if(prefix.size())
            {
               boost::filesystem::path base(prefix);
               base /= f;
               f = base.string();
            }
            else
            {
               boost::filesystem::path base(script);
               base.remove_filename();
               base /= f;
               f = base.string();
            }
         }
         scan_file(f.c_str());
      }
      else if(regex_match(line, what, debug_parser))
      {
         debug = unquote(what[1].str());
      }
      else if(regex_match(line, what, scan_dir_parser))
      {
         std::string d = unquote(what[1].str());
         std::string m = unquote(what[2].str());
         bool r = unquote(what[3].str()) == "true";
         if(!boost::filesystem::path(d).is_complete())
         {
            if(prefix.size())
            {
               boost::filesystem::path base(prefix);
               base /= d;
               d = base.string();
            }
            else
            {
               boost::filesystem::path base(script);
               base.remove_filename();
               base /= d;
               d = base.string();
            }
         }
         if(verbose)
            std::cout << "Scanning directory " << d << std::endl;
         scan_dir(d, m, r);
      }
      else if(regex_match(line, what, rewrite_parser))
      {
         bool id = what[1] == "rewrite-id";
         std::string a = unquote(what[2].str());
         std::string b = unquote(what[3].str());
         id_rewrite_list.push_back(id_rewrite_rule(a, b, id));
      }
      else if(line.compare(0, 9, "!exclude ") == 0)
      {
         static const boost::regex delim("([^\"[:space:]]+|\"(?:[^\"\\\\]|\\\\.)+\")");
         boost::sregex_token_iterator i(line.begin() + 9, line.end(), delim, 0), j;
         while(i != j)
         {
            index_info info;
            info.term = unquote(*i);
            // Erase all entries that have a category in our scanner set,
            // plus any entry with no category at all:
            index_terms.erase(info);
            for(file_scanner_set_type::iterator pscan = file_scanner_set.begin(); pscan != file_scanner_set.end(); ++pscan)
            {
               info.category = (*pscan).type;
               index_terms.erase(info);
            }
            ++i;
         }
      }
      else if(regex_match(line, what, entry_parser))
      {
         try{
            // what[1] is the Index entry
            // what[2] is the regex to search for (optional)
            // what[3] is a section id that must be matched 
            // in order for the term to be indexed (optional)
            // what[4] is the index category to place the term in (optional).
            index_info info;
            info.term = unquote(what.str(1));
            std::string s = unquote(what.str(2));
            if(s.size())
               info.search_text = boost::regex(s, boost::regex::icase|boost::regex::perl);
            else
               info.search_text = boost::regex("\\<" + what.str(1) + "\\>", boost::regex::icase|boost::regex::perl);

            s = unquote(what.str(3));
            if(s.size())
               info.search_id = s;
            if(what[4].matched)
               info.category = unquote(what.str(4));
            index_terms.insert(info);
         }
         catch(const boost::regex_error&)
         {
            std::cerr << "Unable to process regular expression in script line:\n  \""
               << line << "\"" << std::endl;
            throw;
         }
         catch(const std::exception&)
         {
            std::cerr << "Unable to process script line:\n  \""
               << line << "\"" << std::endl;
            throw;
         }
      }
   }
}

