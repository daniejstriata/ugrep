/******************************************************************************\
* Copyright (c) 2019, Robert van Engelen, Genivia Inc. All rights reserved.    *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions are met:  *
*                                                                              *
*   (1) Redistributions of source code must retain the above copyright notice, *
*       this list of conditions and the following disclaimer.                  *
*                                                                              *
*   (2) Redistributions in binary form must reproduce the above copyright      *
*       notice, this list of conditions and the following disclaimer in the    *
*       documentation and/or other materials provided with the distribution.   *
*                                                                              *
*   (3) The name of the author may not be used to endorse or promote products  *
*       derived from this software without specific prior written permission.  *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED *
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF         *
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO   *
* EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,       *
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, *
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;  *
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,     *
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR      *
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF       *
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                   *
\******************************************************************************/

/**
@file      ugrep.cpp
@brief     Universal grep - a pattern search utility
@author    Robert van Engelen - engelen@genivia.com
@copyright (c) 2019-2019, Robert van Engelen, Genivia Inc. All rights reserved.
@copyright (c) BSD-3 License - see LICENSE.txt

Universal grep: a high-performance universal file search utility matches
Unicode patterns.  Offers powerful predefined search patterns and quick
options to selectively search source code files in large directory trees.

Download and installation:

  https://github.com/Genivia/ugrep

Requires RE/flex 1.3.7 or greater:

  https://github.com/Genivia/RE-flex

Optional libraries:

  zlib
  Boost.Regex

Compile:

  c++ -std=c++11 -O2 -o ugrep ugrep.cpp glob.cpp zstream.cpp -lreflex -lz -lboost_regex

*/

#include <reflex/input.h>
#include <reflex/matcher.h>
#include <iomanip>
#include <cctype>
#include <cstring>
#include <cerrno>

// check if we are on a windows OS
#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(__BORLANDC__)
# define OS_WIN
#endif

// windows has no isatty() or stat()
#ifdef OS_WIN

#include <windows.h>
#include <tchar.h> 
#include <stdio.h>
#include <strsafe.h>

#define isatty(fildes) ((fildes) == 1)
#define PATHSEPCHR '\\'
#define PATHSEPSTR "\\"

#else

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include "config.h"

#ifdef HAVE_BOOST_REGEX
#include <reflex/boostmatcher.h>
#endif

#ifdef HAVE_LIBZ
#include "zstream.h"
#endif

#define PATHSEPCHR '/'
#define PATHSEPSTR "/"

#endif

// ugrep version info
#define UGREP_VERSION "1.3.7"

// ugrep platform -- see configure.ac
#if !defined(PLATFORM)
# if defined(OS_WIN)
#  define PLATFORM "WIN"
# else
#  define PLATFORM ""
# endif
#endif

// ugrep exit codes
#define EXIT_OK    0 // One or more lines were selected
#define EXIT_FAIL  1 // No lines were selected
#define EXIT_ERROR 2 // An error occurred

// undefined size_t value
#define UNDEFINED (size_t)(-1)

// max --jobs
#define MAX_JOBS 1000

// max mmap() file size to allocate, must be less or equal to 4294967295LL, 0 disables mmap()
#define MAX_MMAP_SIZE 4294967295LL

// statistics
struct Stats {
  Stats() : files(), dirs(), fileno() { }
  size_t files;
  size_t dirs;
  size_t fileno;
};

// ANSI SGR substrings extracted from GREP_COLORS
#define COLORLEN 16
char color_sl[COLORLEN];
char color_cx[COLORLEN];
char color_mt[COLORLEN];
char color_ms[COLORLEN];
char color_mc[COLORLEN];
char color_fn[COLORLEN];
char color_ln[COLORLEN];
char color_cn[COLORLEN];
char color_bn[COLORLEN];
char color_se[COLORLEN];
const char *color_off = "";

// hex dump state data and colors
#define HEX_MATCH         0
#define HEX_LINE          1
#define HEX_CONTEXT_MATCH 2
#define HEX_CONTEXT_LINE  3
short last_hex_line[0x10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
size_t last_hex_offset = 0;
const char *color_hex[4] = { color_ms, color_sl, color_mc, color_cx };

// output destination is standard output by default or a pipe to --pager
FILE *out = stdout;

// ugrep command-line options
bool flag_with_filename            = false;
bool flag_no_filename              = false;
bool flag_no_group                 = false;
bool flag_no_messages              = false;
bool flag_no_hidden                = false;
bool flag_count                    = false;
bool flag_fixed_strings            = false;
bool flag_free_space               = false;
bool flag_ignore_case              = false;
bool flag_smart_case               = false;
bool flag_invert_match             = false;
bool flag_only_line_number         = false;
bool flag_line_number              = false;
bool flag_column_number            = false;
bool flag_byte_offset              = false;
bool flag_line_buffered            = false;
bool flag_only_matching            = false;
bool flag_quiet                    = false;
bool flag_files_with_match         = false;
bool flag_files_without_match      = false;
bool flag_null                     = false;
bool flag_basic_regexp             = false;
bool flag_perl_regexp              = false;
bool flag_word_regexp              = false;
bool flag_line_regexp              = false;
bool flag_dereference              = false;
bool flag_no_dereference           = false;
bool flag_binary                   = false;
bool flag_binary_without_matches   = false;
bool flag_text                     = false;
bool flag_hex                      = false;
bool flag_with_hex                 = false;
bool flag_empty                    = false;
bool flag_initial_tab              = false;
bool flag_decompress               = false;
bool flag_any_line                 = false;
bool flag_break                    = false;
bool flag_stats                    = false;
size_t flag_after_context          = 0;
size_t flag_before_context         = 0;
size_t flag_max_count              = 0;
size_t flag_max_depth              = 0;
size_t flag_max_files              = 0;
size_t flag_jobs                   = 0;
size_t flag_tabs                   = 8;
const char *flag_pager             = NULL;
const char *flag_color             = NULL;
const char *flag_encoding          = NULL;
const char *flag_devices           = "read";
const char *flag_directories       = "read";
const char *flag_label             = "(standard input)";
const char *flag_separator         = ":";
const char *flag_group_separator   = "--";
const char *flag_binary_files      = "binary";
std::vector<std::string> flag_regexp;;
std::vector<std::string> flag_file;
std::vector<std::string> flag_file_type;
std::vector<std::string> flag_file_extensions;
std::vector<std::string> flag_file_magic;
std::vector<std::string> flag_include;
std::vector<std::string> flag_include_dir;
std::vector<std::string> flag_include_from;
std::vector<std::string> flag_include_override;
std::vector<std::string> flag_include_override_dir;
std::vector<std::string> flag_exclude;
std::vector<std::string> flag_exclude_dir;
std::vector<std::string> flag_exclude_from;
std::vector<std::string> flag_exclude_override;
std::vector<std::string> flag_exclude_override_dir;

// external functions
extern bool globmat(const char *pathname, const char *basename, const char *glob);

// function protos
bool findinfiles(reflex::Matcher& magic, reflex::AbstractMatcher& matcher, std::vector<const char*>& infiles, reflex::Input::file_encoding_type encoding);
void find(Stats& stats, size_t level, reflex::Matcher& magic, reflex::AbstractMatcher& matcher, reflex::Input::file_encoding_type encoding, const char *pathname, const char *basename, bool is_argument = false);
void recurse(Stats& stats, size_t level, reflex::Matcher& magic, reflex::AbstractMatcher& matcher, reflex::Input::file_encoding_type encoding, const char *pathname);
bool ugrep(reflex::AbstractMatcher& matcher, reflex::Input& input, const char *pathname);
void display(const char *name, size_t lineno, size_t columno, size_t byte_offset, const char *sep, bool newline);
void hex_dump(short mode, const char *name, size_t lineno, size_t columno, size_t byte_offset, const char *data, size_t size, const char *separator);
void hex_done(const char *separator);
void hex_line(const char *separator);
void set_color(const char *grep_colors, const char *parameter, char color[COLORLEN]);
void trim(std::string& line);
bool same_file(FILE *file1, FILE *file2);
bool is_file(const reflex::Input& input);
void read_file(reflex::AbstractMatcher& matcher, reflex::Input& input, const char*& base, size_t& size);
bool mmap_file(reflex::Input& input, const char*& base, size_t& size);
void munmap_file(const char *base, size_t size);
void warning(const char *message, const char *arg);
void error(const char *message, const char *arg);
void help(const char *message = NULL, const char *arg = NULL);
void version();

// read a line from buffered input
inline bool getline(reflex::BufferedInput& input, std::string& line)
{
  int ch;

  line.erase();
  while ((ch = input.get()) != EOF)
  {
    line.push_back(ch);
    if (ch == '\n')
      break;
  }
  return ch == EOF && line.empty();
}

// read a line from mmap memory or from buffered input or from unbuffered input
inline bool getline(const char*& here, size_t& left, reflex::BufferedInput& buffered_input, reflex::Input& input, std::string& line)
{
  if (here != NULL)
  {
    // read line from mmap memory
    if (left == 0)
      return true;
    const char *s = here;
    const char *e = here + left;
    while (s < e)
      if (*s++ == '\n')
        break;
    line.assign(here, s - here);
    left -= s - here;
    here = s;
    return false;
  }

  int ch;

  line.erase();

  if (buffered_input.assigned())
  {
    // read line from buffered input
    while ((ch = buffered_input.get()) != EOF)
    {
      line.push_back(ch);
      if (ch == '\n')
        break;
    }
    return ch == EOF && line.empty();
  }

  // read line from unbuffered input
  while ((ch = input.get()) != EOF)
  {
    line.push_back(ch);
    if (ch == '\n')
      break;
  }
  return ch == EOF && line.empty();
}

// return true if text[0..size=1] is displayable text
inline bool is_binary(const char *text, size_t size)
{
  // check if text[0..size-1] contains a NUL or invalid UTF-8
  const char *end = text + size;
  while (text < end)
  {
    if (*text == '\0' || (*text & 0xc0) == 0x80)
      return true;
    if ((*text & 0xc0) == 0xc0)
    {
      if (++text >= end || (*text & 0xc0) != 0x80)
        return true;
      while (text < end && (*text & 0xc0) == 0x80)
        ++text;
    }
    else
    {
      ++text;
    }
  }
  return false;
}

#ifndef OS_WIN
// Windows compatible fopen_s()
inline int fopen_s(FILE **file, const char *filename, const char *mode)
{
  return (*file = fopen(filename, mode)) == NULL ? errno : 0;
}
#endif

// specify a line of input for the matcher to read, matcher must not use text() or rest() to keep the line contents unmodified
inline void read_line(reflex::AbstractMatcher& matcher, const std::string& line)
{
  // safe cast: buffer() is read-only if no matcher.text() and matcher.rest() are used, size + 1 to include final \0
  matcher.buffer(const_cast<char*>(line.c_str()), line.size() + 1);
}

// copy color buffers
inline void copy_color(char to[COLORLEN], char from[COLORLEN])
{
  memcpy(to, from, COLORLEN);
}

// table of RE/flex file encodings for ugrep option --encoding
const struct { const char *format; reflex::Input::file_encoding_type encoding; } format_table[] = {
  { "binary",     reflex::Input::file_encoding::plain   },
  { "ISO-8859-1", reflex::Input::file_encoding::latin   },
  { "ASCII",      reflex::Input::file_encoding::utf8    },
  { "EBCDIC",     reflex::Input::file_encoding::ebcdic  },
  { "UTF-8",      reflex::Input::file_encoding::utf8    },
  { "UTF-16",     reflex::Input::file_encoding::utf16be },
  { "UTF-16BE",   reflex::Input::file_encoding::utf16be },
  { "UTF-16LE",   reflex::Input::file_encoding::utf16le },
  { "UTF-32",     reflex::Input::file_encoding::utf32be },
  { "UTF-32BE",   reflex::Input::file_encoding::utf32be },
  { "UTF-32LE",   reflex::Input::file_encoding::utf32le },
  { "CP437",      reflex::Input::file_encoding::cp437   },
  { "CP850",      reflex::Input::file_encoding::cp850   },
  { "CP858",      reflex::Input::file_encoding::cp858   },
  { "CP1250",     reflex::Input::file_encoding::cp1250  },
  { "CP1251",     reflex::Input::file_encoding::cp1251  },
  { "CP1252",     reflex::Input::file_encoding::cp1252  },
  { "CP1253",     reflex::Input::file_encoding::cp1253  },
  { "CP1254",     reflex::Input::file_encoding::cp1254  },
  { "CP1255",     reflex::Input::file_encoding::cp1255  },
  { "CP1256",     reflex::Input::file_encoding::cp1256  },
  { "CP1257",     reflex::Input::file_encoding::cp1257  },
  { "CP1258",     reflex::Input::file_encoding::cp1258  },
  { NULL, 0 }
};

// table of file types for ugrep option -t, --file-type
const struct { const char *type; const char *extensions; const char *magic; } type_table[] = {
  { "actionscript", "as,mxml",                                                  NULL },
  { "ada",          "ada,adb,ads",                                              NULL },
  { "asm",          "asm,s,S",                                                  NULL },
  { "asp",          "asp",                                                      NULL },
  { "aspx",         "master,ascx,asmx,aspx,svc",                                NULL },
  { "autoconf",     "ac,in",                                                    NULL },
  { "automake",     "am,in",                                                    NULL },
  { "awk",          "awk",                                                      NULL },
  { "Awk",          "awk",                                                      "#!/.*\\Wg?awk(\\W.*)?\\n" },
  { "basic",        "bas,BAS,cls,frm,ctl,vb,resx",                              NULL },
  { "batch",        "bat,BAT,cmd,CMD",                                          NULL },
  { "bison",        "y,yy,yxx",                                                 NULL },
  { "c",            "c,h,H,hdl,xs",                                             NULL },
  { "c++",          "cpp,CPP,cc,cxx,CXX,h,hh,H,hpp,hxx,Hxx,HXX",                NULL },
  { "clojure",      "clj",                                                      NULL },
  { "csharp",       "cs",                                                       NULL },
  { "css",          "css",                                                      NULL },
  { "csv",          "csv",                                                      NULL },
  { "dart",         "dart",                                                     NULL },
  { "Dart",         "dart",                                                     "#!/.*\\Wdart(\\W.*)?\\n" },
  { "delphi",       "pas,int,dfm,nfm,dof,dpk,dproj,groupproj,bdsgroup,bdsproj", NULL },
  { "elisp",        "el",                                                       NULL },
  { "elixir",       "ex,exs",                                                   NULL },
  { "erlang",       "erl,hrl",                                                  NULL },
  { "fortran",      "for,ftn,fpp,f,F,f77,F77,f90,F90,f95,F95,f03,F03",          NULL },
  { "gif",          "gif",                                                      NULL },
  { "Gif",          "gif",                                                      "GIF87a|GIF89a" },
  { "go",           "go",                                                       NULL },
  { "groovy",       "groovy,gtmpl,gpp,grunit,gradle",                           NULL },
  { "gsp",          "gsp",                                                      NULL },
  { "haskell",      "hs,lhs",                                                   NULL },
  { "html",         "htm,html,xhtml",                                           NULL },
  { "jade",         "jade",                                                     NULL },
  { "java",         "java,properties",                                          NULL },
  { "jpeg",         "jpg,jpeg",                                                 NULL },
  { "Jpeg",         "jpg,jpeg",                                                 "\\xff\\xd8\\xff[\\xdb\\xe0\\xe1\\xee]" },
  { "js",           "js",                                                       NULL },
  { "json",         "json",                                                     NULL },
  { "jsp",          "jsp,jspx,jthm,jhtml",                                      NULL },
  { "julia",        "jl",                                                       NULL },
  { "kotlin",       "kt,kts",                                                   NULL },
  { "less",         "less",                                                     NULL },
  { "lex",          "l,ll,lxx",                                                 NULL },
  { "lisp",         "lisp,lsp",                                                 NULL },
  { "lua",          "lua",                                                      NULL },
  { "m4",           "m4",                                                       NULL },
  { "make",         "mk,mak,makefile,Makefile,Makefile.Debug,Makefile.Release", NULL },
  { "markdown",     "md",                                                       NULL },
  { "matlab",       "m",                                                        NULL },
  { "node",         "js",                                                       NULL },
  { "Node",         "js",                                                       "#!/.*\\Wnode(\\W.*)?\\n" },
  { "objc",         "m,h",                                                      NULL },
  { "objc++",       "mm,h",                                                     NULL },
  { "ocaml",        "ml,mli,mll,mly",                                           NULL },
  { "parrot",       "pir,pasm,pmc,ops,pod,pg,tg",                               NULL },
  { "pascal",       "pas,pp",                                                   NULL },
  { "pdf",          "pdf",                                                      NULL },
  { "Pdf",          "pdf",                                                      "\\x25\\x50\\x44\\x46\\x2d" },
  { "perl",         "pl,PL,pm,pod,t,psgi",                                      NULL },
  { "Perl",         "pl,PL,pm,pod,t,psgi",                                      "#!/.*\\Wperl(\\W.*)?\\n" },
  { "php",          "php,php3,php4,phtml",                                      NULL },
  { "Php",          "php,php3,php4,phtml",                                      "#!/.*\\Wphp(\\W.*)?\\n" },
  { "png",          "png",                                                      NULL },
  { "Png",          "png",                                                      "\\x89png\\x0d\\x0a\\x1a\\x0a" },
  { "prolog",       "pl,pro",                                                   NULL },
  { "python",       "py",                                                       NULL },
  { "Python",       "py",                                                       "#!/.*\\Wpython(\\W.*)?\\n" },
  { "r",            "R",                                                        NULL },
  { "rpm",          "rpm",                                                      NULL },
  { "Rpm",          "rpm",                                                      "\\xed\\xab\\xee\\xdb" },
  { "rst",          "rst",                                                      NULL },
  { "rtf",          "rtf",                                                      NULL },
  { "Rtf",          "rtf",                                                      "\\{\\rtf1" },
  { "ruby",         "rb,rhtml,rjs,rxml,erb,rake,spec,Rakefile",                 NULL },
  { "Ruby",         "rb,rhtml,rjs,rxml,erb,rake,spec,Rakefile",                 "#!/.*\\Wruby(\\W.*)?\\n" },
  { "rust",         "rs",                                                       NULL },
  { "scala",        "scala",                                                    NULL },
  { "scheme",       "scm,ss",                                                   NULL },
  { "shell",        "sh,bash,dash,csh,tcsh,ksh,zsh,fish",                       NULL },
  { "Shell",        "sh,bash,dash,csh,tcsh,ksh,zsh,fish",                       "#!/.*\\W(ba|da|t?c|k|z|fi)?sh(\\W.*)?\\n" },
  { "smalltalk",    "st",                                                       NULL },
  { "sql",          "sql,ctl",                                                  NULL },
  { "svg",          "svg",                                                      NULL },
  { "swift",        "swift",                                                    NULL },
  { "tcl",          "tcl,itcl,itk",                                             NULL },
  { "tex",          "tex,cls,sty,bib",                                          NULL },
  { "text",         "text,txt,TXT,md",                                          NULL },
  { "tiff",         "tif,tiff",                                                 NULL },
  { "Tiff",         "tif,tiff",                                                 "\\x49\\x49\\x2a\\x00|\\x4d\\x4d\\x00\\x2a" },
  { "tt",           "tt,tt2,ttml",                                              NULL },
  { "typescript",   "ts,tsx",                                                   NULL },
  { "verilog",      "v,vh,sv",                                                  NULL },
  { "vhdl",         "vhd,vhdl",                                                 NULL },
  { "vim",          "vim",                                                      NULL },
  { "xml",          "xml,xsd,xsl,xslt,wsdl,rss,svg,ent,plist",                  NULL },
  { "Xml",          "xml,xsd,xsl,xslt,wsdl,rss,svg,ent,plist",                  "<\\?xml " },
  { "yacc",         "y",                                                        NULL },
  { "yaml",         "yaml,yml",                                                 NULL },
  { NULL,           NULL,                                                       NULL }
};

// ugrep main()
int main(int argc, char **argv)
{
  std::string regex;
  const char *pattern = NULL;
  std::vector<const char*> infiles;
  bool options = true;

  // parse ugrep command-line options and arguments
  for (int i = 1; i < argc; ++i)
  {
    const char *arg = argv[i];

    if ((*arg == '-'
#ifdef OS_WIN
         || *arg == '/'
#endif
        ) && arg[1] && options)
    {
      bool is_grouped = true;

      // parse a ugrep command-line option
      while (is_grouped && *++arg)
      {
        switch (*arg)
        {
          case '-':
            ++arg;
            if (!*arg)
              options = false;
            else if (strncmp(arg, "after-context=", 14) == 0)
              flag_after_context = (size_t)strtoull(arg + 14, NULL, 10);
            else if (strcmp(arg, "any-line") == 0)
              flag_any_line = true;
            else if (strcmp(arg, "basic-regexp") == 0)
              flag_basic_regexp = true;
            else if (strncmp(arg, "before-context=", 15) == 0)
              flag_before_context = (size_t)strtoull(arg + 15, NULL, 10);
            else if (strcmp(arg, "binary") == 0)
              flag_binary = true;
            else if (strncmp(arg, "binary-files=", 13) == 0)
              flag_binary_files = arg + 13;
            else if (strcmp(arg, "break") == 0)
              flag_break = true;
            else if (strcmp(arg, "byte-offset") == 0)
              flag_byte_offset = true;
            else if (strcmp(arg, "color") == 0 || strcmp(arg, "colour") == 0)
              flag_color = "auto";
            else if (strncmp(arg, "color=", 6) == 0)
              flag_color = arg + 6;
            else if (strncmp(arg, "colour=", 7) == 0)
              flag_color = arg + 7;
            else if (strcmp(arg, "column-number") == 0)
              flag_column_number = true;
            else if (strcmp(arg, "context") == 0)
              flag_after_context = flag_before_context = 2;
            else if (strncmp(arg, "context=", 8) == 0)
              flag_after_context = flag_before_context = (size_t)strtoull(arg + 8, NULL, 10);
            else if (strcmp(arg, "count") == 0)
              flag_count = true;
            else if (strcmp(arg, "decompress") == 0)
              flag_decompress = true;
            else if (strcmp(arg, "dereference") == 0)
              flag_dereference = true;
            else if (strcmp(arg, "dereference-recursive") == 0)
              flag_directories = "dereference-recurse";
            else if (strncmp(arg, "devices=", 8) == 0)
              flag_devices = arg + 8;
            else if (strncmp(arg, "directories=", 12) == 0)
              flag_directories = arg + 12;
            else if (strcmp(arg, "empty") == 0)
              flag_empty = true;
            else if (strncmp(arg, "encoding=", 9) == 0)
              flag_encoding = arg + 9;
            else if (strncmp(arg, "exclude=", 8) == 0)
              flag_exclude.emplace_back(arg + 8);
            else if (strncmp(arg, "exclude-dir=", 12) == 0)
              flag_exclude_dir.emplace_back(arg + 12);
            else if (strncmp(arg, "exclude-from=", 13) == 0)
              flag_exclude_from.emplace_back(arg + 13);
            else if (strcmp(arg, "extended-regexp") == 0)
              flag_basic_regexp = false;
            else if (strncmp(arg, "file=", 5) == 0)
              flag_file.emplace_back(arg + 5);
            else if (strncmp(arg, "file-extensions=", 16) == 0)
              flag_file_extensions.emplace_back(arg + 16);
            else if (strncmp(arg, "file-magic=", 11) == 0)
              flag_file_magic.emplace_back(arg + 11);
            else if (strncmp(arg, "file-type=", 10) == 0)
              flag_file_type.emplace_back(arg + 10);
            else if (strcmp(arg, "files-with-match") == 0)
              flag_files_with_match = true;
            else if (strcmp(arg, "files-without-match") == 0)
              flag_files_without_match = true;
            else if (strcmp(arg, "fixed-strings") == 0)
              flag_fixed_strings = true;
            else if (strcmp(arg, "free-space") == 0)
              flag_free_space = true;
            else if (strncmp(arg, "group-separator=", 16) == 0)
              flag_group_separator = arg + 16;
            else if (strcmp(arg, "help") == 0)
              help();
            else if (strcmp(arg, "hex") == 0)
              flag_binary_files = "hex";
            else if (strcmp(arg, "ignore-case") == 0)
              flag_ignore_case = true;
            else if (strncmp(arg, "include=", 8) == 0)
              flag_include.emplace_back(arg + 8);
            else if (strncmp(arg, "include-dir=", 12) == 0)
              flag_include_dir.emplace_back(arg + 12);
            else if (strncmp(arg, "include-from=", 13) == 0)
              flag_include_from.emplace_back(arg + 13);
            else if (strcmp(arg, "initial-tab") == 0)
              flag_initial_tab = true;
            else if (strcmp(arg, "invert-match") == 0)
              flag_invert_match = true;
            else if (strcmp(arg, "jobs") == 0)
              flag_jobs = MAX_JOBS;
            else if (strncmp(arg, "jobs=", 5) == 0)
              flag_jobs = (size_t)strtoull(arg + 5, NULL, 10);
            else if (strcmp(arg, "label") == 0)
              flag_label = "";
            else if (strncmp(arg, "label=", 6) == 0)
              flag_label = arg + 6;
            else if (strcmp(arg, "line-buffered") == 0)
              flag_line_buffered = true;
            else if (strcmp(arg, "line-number") == 0)
              flag_line_number = true;
            else if (strcmp(arg, "line-regexp") == 0)
              flag_line_regexp = true;
            else if (strncmp(arg, "max-count=", 10) == 0)
              flag_max_count = (size_t)strtoull(arg + 10, NULL, 10);
            else if (strncmp(arg, "max-depth=", 10) == 0)
              flag_max_depth = (size_t)strtoull(arg + 10, NULL, 10);
            else if (strncmp(arg, "max-files=", 10) == 0)
              flag_max_files = (size_t)strtoull(arg + 10, NULL, 10);
            else if (strcmp(arg, "no-dereference") == 0)
              flag_no_dereference = true;
            else if (strcmp(arg, "no-filename") == 0)
              flag_no_filename = true;
            else if (strcmp(arg, "no-group") == 0)
              flag_no_group = true;
            else if (strcmp(arg, "no-group-separator") == 0)
              flag_group_separator = NULL;
            else if (strcmp(arg, "no-hidden") == 0)
              flag_no_hidden = true;
            else if (strcmp(arg, "no-messages") == 0)
              flag_no_messages = true;
            else if (strcmp(arg, "null") == 0)
              flag_null = true;
            else if (strcmp(arg, "only-line-number") == 0)
              flag_only_line_number = true;
            else if (strcmp(arg, "only-matching") == 0)
              flag_only_matching = true;
            else if (strncmp(arg, "pager", 5) == 0)
              flag_pager = "less -R";
            else if (strncmp(arg, "pager=", 6) == 0)
              flag_pager = arg + 6;
            else if (strcmp(arg, "perl-regexp") == 0)
              flag_basic_regexp = !(flag_perl_regexp = true);
            else if (strcmp(arg, "quiet") == 0 || strcmp(arg, "silent") == 0)
              flag_quiet = flag_no_messages = true;
            else if (strcmp(arg, "recursive") == 0)
              flag_directories = "recurse";
            else if (strncmp(arg, "regexp=", 7) == 0)
              flag_regexp.emplace_back(arg + 7);
            else if (strncmp(arg, "separator=", 10) == 0)
              flag_separator = arg + 10;
            else if (strcmp(arg, "smart-case") == 0)
              flag_smart_case = true;
            else if (strcmp(arg, "stats") == 0)
              flag_stats = true;
            else if (strncmp(arg, "tabs=", 5) == 0)
              flag_tabs = (size_t)strtoull(arg + 5, NULL, 10);
            else if (strcmp(arg, "text") == 0)
              flag_binary_files = "text";
            else if (strcmp(arg, "version") == 0)
              version();
            else if (strcmp(arg, "with-filename") == 0)
              flag_with_filename = true;
            else if (strcmp(arg, "with-hex") == 0)
              flag_binary_files = "with-hex";
            else if (strcmp(arg, "word-regexp") == 0)
              flag_word_regexp = true;
            else
              help("unrecognized option --", arg);
            is_grouped = false;
            break;

          case 'A':
            ++arg;
            if (*arg)
              flag_after_context = (size_t)strtoull(&arg[*arg == '='], NULL, 10);
            else if (++i < argc)
              flag_after_context = (size_t)strtoull(argv[i], NULL, 10);
            else
              help("missing NUM for option -A");
            is_grouped = false;
            break;

          case 'a':
            flag_binary_files = "text";
            break;

          case 'B':
            ++arg;
            if (*arg)
              flag_before_context = (size_t)strtoull(&arg[*arg == '='], NULL, 10);
            else if (++i < argc)
              flag_before_context = (size_t)strtoull(argv[i], NULL, 10);
            else
              help("missing NUM for option -B");
            is_grouped = false;
            break;

          case 'b':
            flag_byte_offset = true;
            break;

          case 'C':
            ++arg;
            if (*arg == '=' || isdigit(*arg))
            {
              flag_after_context = flag_before_context = (size_t)strtoull(&arg[*arg == '='], NULL, 10);
              is_grouped = false;
            }
            else
            {
              flag_after_context = flag_before_context = 2;
              --arg;
            }
            break;

          case 'c':
            flag_count = true;
            break;

          case 'D':
            ++arg;
            if (*arg)
              flag_devices = &arg[*arg == '='];
            else if (++i < argc)
              flag_devices = argv[i];
            else
              help("missing ACTION for option -D");
            is_grouped = false;
            break;

          case 'd':
            ++arg;
            if (*arg)
              flag_directories = &arg[*arg == '='];
            else if (++i < argc)
              flag_directories = argv[i];
            else
              help("missing ACTION for option -d");
            is_grouped = false;
            break;

          case 'E':
            flag_basic_regexp = false;
            break;

          case 'e':
            ++arg;
            if (*arg)
              flag_regexp.emplace_back(&arg[*arg == '=']);
            else if (++i < argc)
              flag_regexp.emplace_back(argv[i]);
            else
              help("missing PATTERN for option -e");
            is_grouped = false;
            break;

          case 'F':
            flag_fixed_strings = true;
            break;

          case 'f':
            ++arg;
            if (*arg)
              flag_file.emplace_back(&arg[*arg == '=']);
            else if (++i < argc)
              flag_file.emplace_back(argv[i]);
            else
              help("missing FILE for option -f");
            is_grouped = false;
            break;

          case 'G':
            flag_basic_regexp = true;
            break;

          case 'g':
            flag_no_group = true;
            break;

          case 'H':
            flag_with_filename = true;
            break;

          case 'h':
            flag_no_filename = true;
            break;

          case 'I':
            flag_binary_files = "without-matches";
            break;

          case 'i':
            flag_ignore_case = true;
            break;

          case 'J':
            ++arg;
            if (*arg == '=' || isdigit(*arg))
            {
              flag_jobs = (size_t)strtoull(&arg[*arg == '='], NULL, 10);
              is_grouped = false;
            }
            else
            {
              flag_jobs = MAX_JOBS;
              --arg;
            }
            break;

          case 'j':
            flag_smart_case = true;
            break;

          case 'k':
            flag_column_number = true;
            break;

          case 'L':
            flag_files_without_match = true;
            break;

          case 'l':
            flag_files_with_match = true;
            break;

          case 'm':
            ++arg;
            if (*arg)
              flag_max_count = (size_t)strtoull(&arg[*arg == '='], NULL, 10);
            else if (++i < argc)
              flag_max_count = (size_t)strtoull(argv[i], NULL, 10);
            else
              help("missing NUM for option -m");
            is_grouped = false;
            break;

          case 'M':
            ++arg;
            if (*arg)
              flag_file_magic.emplace_back(&arg[*arg == '=']);
            else if (++i < argc)
              flag_file_magic.emplace_back(argv[i]);
            else
              help("missing MAGIC for option -M");
            is_grouped = false;
            break;

          case 'N':
            flag_only_line_number = true;
            break;

          case 'n':
            flag_line_number = true;
            break;

          case 'O':
            ++arg;
            if (*arg)
              flag_file_extensions.emplace_back(&arg[*arg == '=']);
            else if (++i < argc)
              flag_file_extensions.emplace_back(argv[i]);
            else
              help("missing EXTENSIONS for option -O");
            is_grouped = false;
            break;

          case 'o':
            flag_only_matching = true;
            break;

          case 'P':
            flag_perl_regexp = true;
            flag_basic_regexp = false;
            break;

          case 'p':
            flag_no_dereference = true;
            break;

          case 'Q':
            ++arg;
            if (*arg)
              flag_encoding = &arg[*arg == '='];
            else if (++i < argc)
              flag_encoding = argv[i];
            else
              help("missing ENCODING for option -:");
            is_grouped = false;
            break;

          case 'q':
            flag_quiet = true;
            break;

          case 'R':
            flag_directories = "dereference-recurse";
            break;

          case 'r':
            flag_directories = "recurse";
            break;

          case 'S':
            flag_dereference = true;
            break;

          case 's':
            flag_no_messages = true;
            break;

          case 'T':
            flag_initial_tab = true;
            break;

          case 't':
            ++arg;
            if (*arg)
              flag_file_type.emplace_back(&arg[*arg == '=']);
            else if (++i < argc)
              flag_file_type.emplace_back(argv[i]);
            else
              help("missing TYPES for option -t");
            is_grouped = false;
            break;

          case 'U':
            flag_binary = true;
            break;

          case 'V':
            version();
            break;

          case 'v':
            flag_invert_match = true;
            break;

          case 'W':
            flag_binary_files = "with-hex";
            break;

          case 'w':
            flag_word_regexp = true;
            break;

          case 'X':
            flag_binary_files = "hex";
            break;

          case 'x':
            flag_line_regexp = true;
            break;

          case 'Y':
            flag_empty = true;
            break;

          case 'y':
            flag_any_line = true;
            break;

          case 'Z':
            flag_null = true;
            break;

          case 'z':
            flag_decompress = true;
            break;

          default:
            help("unrecognized option -", arg);
        }
      }
    }
    else if (options && pattern == NULL && flag_file.empty() && strcmp(arg, "-") != 0)
    {
      // no regex pattern specified yet, so assume it is PATTERN
      pattern = arg;
    }
    else
    {
      // otherwise add the file argument to the list of FILE files
      infiles.emplace_back(arg);
    }
  }

#ifndef HAVE_LIBZ
  // -z: but we don't have libz
  if (flag_decompress)
    help("option -z is not available in this version of ugrep");
#endif

  // -t list: list table of types
  if (flag_file_type.size() == 1 && flag_file_type[0] == "list")
  {
    int i;

    std::cerr << std::setw(12) << "FILE TYPE" << "   FILE NAME EXTENSIONS (-O) AND FILE SIGNATURE 'MAGIC' BYTES (-M)" << std::endl;

    for (i = 0; type_table[i].type != NULL; ++i)
    {
      std::cerr << std::setw(12) << type_table[i].type << " = -O " << type_table[i].extensions << std::endl;
      if (type_table[i].magic)
        std::cerr << std::setw(19) << "-M '" << type_table[i].magic << "'" << std::endl;
    }

    exit(EXIT_ERROR);
  }

  // regex PATTERN specified
  if (pattern != NULL)
  {
    // if one or more -e PATTERN given, add pattern to the front else add to the front of FILE args
    if (flag_regexp.empty())
      flag_regexp.insert(flag_regexp.begin(), pattern);
    else
      infiles.insert(infiles.begin(), pattern);
  }

  // if no regex pattern is specified and no -f file then exit with usage message
  if (flag_regexp.empty() && flag_file.empty())
    help("");

  // -F: make newline-separated lines in regex literal with \Q and \E
  const char *Q = flag_fixed_strings ? "\\Q" : "";
  const char *E = flag_fixed_strings ? "\\E|" : "|";

  // combine all -e PATTERN into a single regex string for matching
  for (auto pattern : flag_regexp)
  {
    // empty PATTERN matches everything
    if (pattern.empty())
    {
      regex.append(".*\\n?|");
    }
    else
    {
      // -F: make newline-separated lines in regex literal with \Q and \E

      size_t from = 0;
      size_t to;

      // split regex at newlines, for -F add \Q \E to each string, separate by |
      while ((to = pattern.find('\n', from)) != std::string::npos)
      {
        if (from < to)
          regex.append(Q).append(pattern.substr(from, to - from - (pattern[to - 1] == '\r'))).append(E);
        from = to + 1;
      }

      if (from < pattern.size())
        regex.append(Q).append(pattern.substr(from)).append(E);

      if (pattern == "^$")
        flag_empty = true; // we're matching empty lines, so enable -Y
    }
  }

  // remove the ending '|' from the |-concatenated regexes in the regex string
  if (!regex.empty())
    regex.pop_back();

  // -x or -w
  if (flag_line_regexp)
    regex.insert(0, "^(").append(")$"); // make the regex line-anchored
  else if (flag_word_regexp)
    regex.insert(0, "\\<(").append(")\\>"); // make the regex word-anchored

  // -j: case insensitive search if regex does not contain a capital letter
  if (flag_smart_case)
  {
    flag_ignore_case = true;

    for (size_t i = 0; i < regex.size(); ++i)
    {
      if (regex[i] == '\\')
      {
        ++i;
      }
      else if (isupper(regex[i]))
      {
        flag_ignore_case = false;
        break;
      }
    }
  }

  if (!flag_file.empty())
  {
    // add an ending '|' to the regex to concatenate sub-expressions
    if (!regex.empty())
      regex.push_back('|');

    // -f: read patterns from the specified file or files
    for (auto filename : flag_file)
    {
      FILE *file = NULL;

      if (filename == "-")
        file = stdin;
      else if (fopen_s(&file, filename.c_str(), "r") != 0)
        file = NULL;

#ifndef OS_WIN
      if (file == NULL)
      {
        // could not open, try GREP_PATH environment variable
        const char *grep_path = getenv("GREP_PATH");

        if (grep_path != NULL)
        {
          std::string path_file(grep_path);
          path_file.append(PATHSEPSTR).append(filename);

          if (fopen_s(&file, path_file.c_str(), "r") != 0)
            file = NULL;
        }
      }
#endif

#ifdef GREP_PATH
      if (file == NULL)
      {
        std::string path_file(GREP_PATH);
        path_file.append(PATHSEPSTR).append(filename);

        if (fopen_s(&file, path_file.c_str(), "r") != 0)
          file = NULL;
      }
#endif

      if (file == NULL)
        error("cannot read", filename.c_str());

      reflex::BufferedInput input(file);
      std::string line;
      size_t lineno = 0;

      while (true)
      {
        // read the next line
        if (getline(input, line))
          break;

        ++lineno;

        trim(line);

        // add line to the regex if not empty
        if (!line.empty())
        {
          // enable -o when the first line is ###-o
          if (lineno == 1 && line == "###-o")
            flag_only_matching = true;
          else
            regex.append(line).push_back('|');
        }
      }

      if (file != stdin)
        fclose(file);
    }

    // remove the ending '|' from the |-concatenated regexes in the regex string
    regex.pop_back();
  }

  // -y: disable -A, -B, and -C
  if (flag_any_line)
    flag_after_context = flag_before_context = 0;

  // -y, -A, -B, or -C: disable -o
  if (flag_any_line || flag_after_context > 0 || flag_before_context > 0)
    flag_only_matching = false;

  // -v: disable -g and -o
  if (flag_invert_match)
  {
    flag_no_group = false;
    flag_only_matching = false;
  }

  // normalize -R (--dereference-recurse) option
  if (strcmp(flag_directories, "dereference-recurse") == 0)
  {
    flag_directories = "recurse";
    flag_dereference = true;
  }

  // normalize -p (--no-dereference) and -S (--dereference) options, -p taking priority over -S
  if (flag_no_dereference)
    flag_dereference = false;

  // display file name if more than one input file is specified or options -R, -r, and option -h --no-filename is not specified
  if (!flag_no_filename && (infiles.size() > 1 || strcmp(flag_directories, "recurse") == 0))
    flag_with_filename = true;

  // (re)set flag_color depending on color_term and isatty()
  if (flag_color)
  {
    if (strcmp(flag_color, "never") == 0)
    {
      flag_color = NULL;
    }
    else if (strcmp(flag_color, "auto") == 0)
    {
      bool color_term = false;

#ifndef OS_WIN
      // check whether we have a color terminal
      const char *term = getenv("TERM");
      color_term = term &&
        (strstr(term, "ansi") != NULL ||
          strstr(term, "xterm") != NULL ||
          strstr(term, "color") != NULL);
#endif

      if (!color_term || !isatty(1))
        flag_color = NULL;
    }
    else if (strcmp(flag_color, "always") != 0)
    {
      help("invalid --color=WHEN, valid arguments are 'never', 'always', and 'auto'");
    }

    if (flag_color)
    {
      const char *grep_color = NULL;
      const char *grep_colors = NULL;

#ifndef OS_WIN
      // get GREP_COLOR and GREP_COLORS environment variables
      grep_color = getenv("GREP_COLOR");
      grep_colors = getenv("GREP_COLORS");
#endif

      if (grep_color != NULL)
        set_color(std::string("mt=").append(grep_color).c_str(), "mt", color_mt);
      else if (grep_colors == NULL)
        grep_colors = "mt=1;31:cx=2:fn=35:ln=32:cn=32:bn=32:se=36";

      if (grep_colors != NULL)
      {
        // parse GREP_COLORS
        set_color(grep_colors, "sl", color_sl); // selected line
        set_color(grep_colors, "cx", color_cx); // context line
        set_color(grep_colors, "mt", color_mt); // matching text in any line
        set_color(grep_colors, "ms", color_ms); // matching text in selected line
        set_color(grep_colors, "mc", color_mc); // matching text in a context line
        set_color(grep_colors, "fn", color_fn); // file name
        set_color(grep_colors, "ln", color_ln); // line number
        set_color(grep_colors, "cn", color_cn); // column number
        set_color(grep_colors, "bn", color_bn); // byte offset
        set_color(grep_colors, "se", color_se); // separators

        if (flag_invert_match && strstr(grep_colors, "rv") != NULL)
        {
          char color_tmp[COLORLEN];
          copy_color(color_tmp, color_sl);
          copy_color(color_sl, color_cx);
          copy_color(color_cx, color_tmp);
        }
      }

      // if ms= or mc= are not specified, use the mt= value
      if (*color_ms == '\0')
        copy_color(color_ms, color_mt);
      if (*color_mc == '\0')
        copy_color(color_mc, color_mt);

      color_off = "\033[0m";
    }
  }

  // -D: check ACTION value
  if (strcmp(flag_devices, "read") != 0 &&
      strcmp(flag_devices, "skip") != 0)
    help("invalid --devices=ACTION, valid arguments are 'read' and 'skip'");

  // -d: check ACTION value
  if (strcmp(flag_directories, "read") != 0 &&
      strcmp(flag_directories, "recurse") != 0 &&
      strcmp(flag_directories, "skip") != 0)
    help("invalid --directories=ACTION, valid arguments are 'read', 'recurse', 'dereference-recurse', and 'skip'");

  // --binary-files: normalize by assigning flags
  if (strcmp(flag_binary_files, "without-matches") == 0)
    flag_binary_without_matches = true;
  else if (strcmp(flag_binary_files, "text") == 0)
    flag_text = true;
  else if (strcmp(flag_binary_files, "hex") == 0)
    flag_hex = true;
  else if (strcmp(flag_binary_files, "with-hex") == 0)
    flag_with_hex = true;
  else if (strcmp(flag_binary_files, "binary") != 0)
    help("invalid --binary-files=TYPE, valid arguments are 'binary', 'without-match', 'text', 'hex', and 'with-hex'");

  // default file encoding is plain (no conversion)
  reflex::Input::file_encoding_type encoding = reflex::Input::file_encoding::plain;

  // -Q: parse ENCODING value
  if (flag_encoding != NULL)
  {
    int i;

    // scan the format_table[] for a matching encoding
    for (i = 0; format_table[i].format != NULL; ++i)
      if (strcmp(flag_encoding, format_table[i].format) == 0)
        break;

    if (format_table[i].format == NULL)
      help("invalid --encoding=ENCODING");

    // encoding is the file encoding used by all input files, if no BOM is present
    encoding = format_table[i].encoding;
  }

  // -t: parse TYPES and access type table to add -O (--file-extensions) and -M (--file-magic) values
  for (auto type : flag_file_type)
  {
    int i;

    // scan the type_table[] for a matching type
    for (i = 0; type_table[i].type != NULL; ++i)
      if (type == type_table[i].type)
        break;

    if (type_table[i].type == NULL)
      help("invalid --file-type=TYPE, to list the valid values use -tlist");

    flag_file_extensions.emplace_back(type_table[i].extensions);

    if (type_table[i].magic != NULL)
      flag_file_magic.emplace_back(type_table[i].magic);
  }

  // -O: add extensions as globs to the --include list
  for (auto extensions : flag_file_extensions)
  {
    size_t from = 0;
    size_t to;
    std::string glob;

    while ((to = extensions.find(',', from)) != std::string::npos)
    {
      flag_include.emplace_back(glob.assign("*.").append(extensions.substr(from, to - from)));
      from = to + 1;
    }

    flag_include.emplace_back(glob.assign("*.").append(extensions.substr(from)));
  }

  // -M: file signature magic bytes MAGIC regex
  std::string signature;

  // -M: combine to create a signature regex from MAGIC
  for (auto magic : flag_file_magic)
  {
    if (!signature.empty())
      signature.push_back('|');
    signature.append(magic);
  }

  // --exclude-from: add globs to the --exclude and --exclude-dir lists
  for (auto i : flag_exclude_from)
  {
    if (!i.empty())
    {
      FILE *file = NULL;

      if (i == "-")
        file = stdin;
      else if (fopen_s(&file, i.c_str(), "r") != 0)
        error("cannot read", i.c_str());

      // read globs from the specified file or files

      reflex::BufferedInput input(file);
      std::string line;

      while (true)
      {
        // read the next line
        if (getline(input, line))
          break;

        trim(line);

        // add glob to --exclude and --exclude-dir using gitignore rules
        if (!line.empty() && line.front() != '#')
        {
          // gitignore-style ! negate pattern (overrides --exclude and --exclude-dir)
          if (line.front() == '!' && !line.empty())
          {
            line.erase(0, 1);

            // globs ending in / should only match directories
            if (line.back() == '/')
              line.pop_back();
            else
              flag_exclude_override.emplace_back(line);

            flag_exclude_override_dir.emplace_back(line);
          }
          else
          {
            // remove leading \ if present
            if (line.front() == '\\' && !line.empty())
              line.erase(0, 1);

            // globs ending in / should only match directories
            if (line.back() == '/')
              line.pop_back();
            else
              flag_exclude.emplace_back(line);

            flag_exclude_dir.emplace_back(line);
          }
        }
      }

      if (file != stdin)
        fclose(file);
    }
  }

  // --include-from: add globs to the --include and --include-dir lists
  for (auto i : flag_include_from)
  {
    if (!i.empty())
    {
      FILE *file = NULL;

      if (i == "-")
        file = stdin;
      else if (fopen_s(&file, i.c_str(), "r") != 0)
        error("cannot read", i.c_str());

      // read globs from the specified file or files

      reflex::BufferedInput input(file);
      std::string line;

      while (true)
      {
        // read the next line
        if (getline(input, line))
          break;

        trim(line);

        // add glob to --include and --include-dir using gitignore rules
        if (!line.empty() && line.front() != '#')
        {
          // gitignore-style ! negate pattern (overrides --include and --include-dir)
          if (line.front() == '!' && !line.empty())
          {
            line.erase(0, 1);

            // globs ending in / should only match directories
            if (line.back() == '/')
              line.pop_back();
            else
              flag_include_override.emplace_back(line);

            flag_include_override_dir.emplace_back(line);
          }
          else
          {
            // remove leading \ if present
            if (line.front() == '\\' && !line.empty())
              line.erase(0, 1);

            // globs ending in / should only match directories
            if (line.back() == '/')
              line.pop_back();
            else
              flag_include.emplace_back(line);

            flag_include_dir.emplace_back(line);
          }
        }
      }

      if (file != stdin)
        fclose(file);
    }
  }

  // -q: we only need to find one matching file and we're done
  if (flag_quiet)
    flag_max_files = 1;

#ifndef OS_WIN
  // --pager: if output is to a TTY then page through the results
  if (isatty(1) && flag_pager != NULL)
  {
    out = popen(flag_pager, "w");
    if (out == NULL)
      error("cannot open pipe to pager", flag_pager);

    // enable --break
    flag_break = true;

    // enable --line-buffered to flush output to the pager immediately
    flag_line_buffered = true;
  }
#endif

  // if no files were specified then read standard input, unless recursive searches are specified
  if (infiles.empty() && strcmp(flag_directories, "recurse") != 0)
    infiles.emplace_back("-");

  // if any match was found in any of the input files later, then found = true
  bool found = false;

  // -M: create a magic matcher for the MAGIC regex signature to match file signatures with magic.scan()
  reflex::Pattern magic_pattern;
  reflex::Matcher magic;

  try
  {
    magic_pattern.assign(signature, "r");
    magic.pattern(magic_pattern);
  }

  catch (reflex::regex_error& error)
  {
    if (!flag_no_messages)
      std::cerr << "option -M MAGIC:\n" << error.what();

    exit(EXIT_ERROR);
  }

  try
  {
    // -U: set flags to convert regex to Unicode
    reflex::convert_flag_type convert_flags = flag_binary ? reflex::convert_flag::none : reflex::convert_flag::unicode;

    // -G: convert basic regex (BRE) to extended regex (ERE)
    if (flag_basic_regexp)
      convert_flags |= reflex::convert_flag::basic;

    // set reflex::Pattern options to raise exceptions and to enable multiline mode
    std::string pattern_options("(?m");

    if (flag_ignore_case)
    {
      // -i: case-insensitive reflex::Pattern option, applies to ASCII only
      pattern_options.append("i");
    }

    if (flag_free_space)
    {
      // --free-space: this is needed to check free-space conformance by the converter
      convert_flags |= reflex::convert_flag::freespace;
      // free-space reflex::Pattern option
      pattern_options.append("x");
    }

    // prepend the pattern options (?m...) to the regex
    pattern_options.append(")");
    regex = pattern_options + regex;

    // reflex::Matcher options
    std::string matcher_options;

    // -Y: permit empty pattern matches
    if (flag_empty)
      matcher_options.append("N");

    // --tabs: set reflex::Matcher option T to NUM tab size
    if (flag_tabs)
    {
      if (flag_tabs == 1 || flag_tabs == 2 || flag_tabs == 4 || flag_tabs == 8)
        matcher_options.append("T=").push_back((char)flag_tabs + '0');
      else
        help("invalid --tabs=NUM value");
    }

    if (flag_perl_regexp)
    {
#ifdef HAVE_BOOST_REGEX
      // construct the NFA pattern matcher
      std::string pattern(reflex::BoostPerlMatcher::convert(regex, convert_flags));
      reflex::BoostPerlMatcher matcher(pattern, matcher_options.c_str());
      found = findinfiles(magic, matcher, infiles, encoding);
#else
      help("Option -P is not available in this version of ugrep");
#endif
    }
    else
    {
      // construct the DFA pattern matcher
      reflex::Pattern pattern(reflex::Matcher::convert(regex, convert_flags), "r");
      reflex::Matcher matcher(pattern, matcher_options.c_str());
      found = findinfiles(magic, matcher, infiles, encoding);
    }
  }

  catch (reflex::regex_error& error)
  {
    if (!flag_no_messages)
      std::cerr << error.what();

    exit(EXIT_ERROR);
  }

#ifdef HAVE_BOOST_REGEX
  catch (boost::regex_error& error)
  {
    if (!flag_no_messages)
    {
      std::cerr << "Boost regex error in " << regex << std::endl;
      switch (error.code())
      {
        case boost::regex_constants::error_collate:
          std::cerr << "an invalid collating element was specified in a [[.name.]] block" << std::endl;
          break;
        case boost::regex_constants::error_ctype:
          std::cerr << "an invalid character class name was specified in a [[:name:]] block" << std::endl;
          break;
        case boost::regex_constants::error_escape:
          std::cerr << "an invalid or trailing escape was encountered" << std::endl;
          break;
        case boost::regex_constants::error_backref:
          std::cerr << "a back-reference to a non-existant marked sub-expression was encountered" << std::endl;
          break;
        case boost::regex_constants::error_brack:
          std::cerr << "an invalid character set [...] was encountered" << std::endl;
          break;
        case boost::regex_constants::error_paren:
          std::cerr << "mismatched ( and )" << std::endl;
          break;
        case boost::regex_constants::error_brace:
          std::cerr << "mismatched { and }" << std::endl;
          break;
        case boost::regex_constants::error_badbrace:
          std::cerr << "invalid contents of a {...} block" << std::endl;
          break;
        case boost::regex_constants::error_range:
          std::cerr << "a character range was invalid, for example [d-a]" << std::endl;
          break;
        case boost::regex_constants::error_space:
          std::cerr << "out of memory" << std::endl;
          break;
        case boost::regex_constants::error_badrepeat:
          std::cerr << "an attempt to repeat something that can not be repeated - for example a*+" << std::endl;
          break;
        case boost::regex_constants::error_complexity:
          std::cerr << "the expression became too complex to handle" << std::endl;
          break;
        case boost::regex_constants::error_stack:
          std::cerr << "out of program stack space" << std::endl;
          break;
        default:
          std::cerr << "bad pattern" << std::endl;
      }
    }

    exit(EXIT_ERROR);
  }
#endif

#ifndef OS_WIN
  if (out != stdout)
    pclose(out);
#endif

  exit(found ? EXIT_OK : EXIT_FAIL);
}

// search infiles for pattern matches
bool findinfiles(reflex::Matcher& magic, reflex::AbstractMatcher& matcher, std::vector<const char*>& infiles, reflex::Input::file_encoding_type encoding)
{
  Stats stats;

  if (infiles.empty())
  {
    recurse(stats, 1, magic, matcher, encoding, NULL);
  }
  else
  {
    // read each input file to find pattern matches
    for (auto infile : infiles)
    {
      if (strcmp(infile, "-") == 0)
      {
        // search standard input, does not count towards fileno
        reflex::Input input(stdin, encoding);

        ++stats.files;

        if (ugrep(matcher, input, flag_label))
          ++stats.fileno;
      }
      else
      {
        // search file or directory, get the basename from the infile argument first
        const char *basename = strrchr(infile, PATHSEPCHR);

        if (basename != NULL)
          ++basename;
        else
          basename = infile;

        find(stats, 1, magic, matcher, encoding, infile, basename, true);
      }

      // stop after finding max-files matching files
      if (flag_max_files > 0 && stats.fileno >= flag_max_files)
        break;
    }
  }

  if (flag_stats)
  {
    fprintf(out, "Searched %zu file%s", stats.files, stats.files == 1 ? "" : "s");
    if (stats.dirs > 0)
      fprintf(out, " in %zu director%s", stats.dirs, stats.dirs == 1 ? "y" : "ies");
    if (stats.fileno > 0)
      fprintf(out, ": found %zu file%s with matches\n", stats.fileno, stats.fileno == 1 ? "" : "s");
    else
      fprintf(out, ": found no matches\n");
  }

  return stats.fileno > 0;
}

// search file or directory for pattern matches
void find(Stats& stats, size_t level, reflex::Matcher& magic, reflex::AbstractMatcher& matcher, reflex::Input::file_encoding_type encoding, const char *pathname, const char *basename, bool is_argument)
{
  if (flag_no_hidden && *basename == '.')
    return;

#ifdef OS_WIN

  DWORD attr = GetFileAttributesA(pathname);

  if (flag_no_hidden && (attr & FILE_ATTRIBUTE_HIDDEN))
    return;

  if ((attr & FILE_ATTRIBUTE_DIRECTORY))
  {
    if (strcmp(flag_directories, "read") == 0)
    {
      // directories cannot be read actually, so grep produces a warning message (errno is not set)
      if (!flag_no_messages)
        fprintf(stderr, "ugrep: %s is a directory\n", pathname);

      return;
    }

    if (strcmp(flag_directories, "recurse") == 0)
    {
      // check for --exclude-dir and --include-dir constraints if pathname != "."
      if (strcmp(pathname, ".") != 0)
      {
        // do not exclude directories that are overridden by ! negation
        bool negate = false;
        for (auto& glob : flag_exclude_override_dir)
          if ((negate = globmat(pathname, basename, glob.c_str())))
            break;

        if (!negate)
        {
          // exclude directories whose basename matches any one of the --exclude-dir globs
          for (auto& glob : flag_exclude_dir)
            if (globmat(pathname, basename, glob.c_str()))
              return;
        }

        if (!flag_include_dir.empty())
        {
          // do not include directories that are overridden by ! negation
          for (auto& glob : flag_include_override_dir)
            if (globmat(pathname, basename, glob.c_str()))
              return;

          // include directories whose basename matches any one of the --include-dir globs
          bool ok = false;
          for (auto& glob : flag_include_dir)
            if ((ok = globmat(pathname, basename, glob.c_str())))
              break;
          if (!ok)
            return;
        }
      }

      recurse(stats, level, magic, matcher, encoding, pathname);
    }
  }
  else if ((attr & FILE_ATTRIBUTE_DEVICE) == 0 || strcmp(flag_devices, "read") == 0)
  {
    // do not exclude files that are overridden by ! negation
    bool negate = false;
    for (auto& glob : flag_exclude_override)
      if ((negate = globmat(pathname, basename, glob.c_str())))
        break;

    if (!negate)
    {
      // exclude files whose basename matches any one of the --exclude globs
      for (auto& glob : flag_exclude)
        if (globmat(pathname, basename, glob.c_str()))
          return;
    }

    // check magic pattern against the file signature, when --file-magic=MAGIC is specified
    if (!flag_file_magic.empty())
    {
      FILE *file;

      if (fopen_s(&file, pathname, "r") == 0)
      {
        if (same_file(out, file))
        {
          fclose(file);
          return;
        }

        // read the file to check its file signature
        reflex::Input input(file, encoding);

        // file has the magic bytes we're looking for: search the file
        if (magic.input(input).scan() != 0)
        {
          ++stats.files;

          rewind(file);

          if (ugrep(matcher, input, pathname))
            ++stats.fileno;

          fclose(file);

          return;
        }

        fclose(file);

        if (flag_include.empty())
          return;
      }
    }

    if (!flag_include.empty())
    {
      // do not include files that are overridden by ! negation
      for (auto& glob : flag_include_override)
        if (globmat(pathname, basename, glob.c_str()))
          return;

      // include files whose basename matches any one of the --include globs
      bool ok = false;
      for (auto& glob : flag_include)
        if ((ok = globmat(pathname, basename, glob.c_str())))
          break;
      if (!ok)
        return;
    }

    FILE *file;

    if (fopen_s(&file, pathname, "r") != 0)
    {
      if (!flag_no_messages)
        warning("cannot read", pathname);

      return;
    }

    if (!same_file(out, file))
    {
      reflex::Input input(file, encoding);

      ++stats.files;

      if (ugrep(matcher, input, pathname))
        ++stats.fileno;
    }

    fclose(file);
  }

#else

  struct stat buf;

  // use lstat() to check if pathname is a symlink
  if (lstat(pathname, &buf) == 0)
  {
    // symlinks are followed when specified on the command line (unless option -p) or with options -R, -S, --dereference
    if (!S_ISLNK(buf.st_mode) || (is_argument && !flag_no_dereference) || flag_dereference)
    {
      // if we got a symlink, use stat() to check if pathname is a directory or a regular file
      if (!S_ISLNK(buf.st_mode) || stat(pathname, &buf) == 0)
      {
        if (S_ISDIR(buf.st_mode))
        {
          if (strcmp(flag_directories, "read") == 0)
          {
            // directories cannot be read actually, so grep produces a warning message (errno is not set)
            if (!flag_no_messages)
              fprintf(stderr, "ugrep: %s is a directory\n", pathname);

            return;
          }

          if (strcmp(flag_directories, "recurse") == 0)
          {
            // check for --exclude-dir and --include-dir constraints if pathname != "."
            if (strcmp(pathname, ".") != 0)
            {
              // do not exclude directories that are overridden by ! negation
              bool negate = false;
              for (auto& glob : flag_exclude_override_dir)
                if ((negate = globmat(pathname, basename, glob.c_str())))
                  break;

              if (!negate)
              {
                // exclude directories whose pathname matches any one of the --exclude-dir globs
                for (auto& glob : flag_exclude_dir)
                  if (globmat(pathname, basename, glob.c_str()))
                    return;
              }

              if (!flag_include_dir.empty())
              {
                // do not include directories that are overridden by ! negation
                for (auto& glob : flag_include_override_dir)
                  if (globmat(pathname, basename, glob.c_str()))
                    return;

                // include directories whose pathname matches any one of the --include-dir globs
                bool ok = false;
                for (auto& glob : flag_include_dir)
                  if ((ok = globmat(pathname, basename, glob.c_str())))
                    break;
                if (!ok)
                  return;
              }
            }

            recurse(stats, level, magic, matcher, encoding, pathname);
          }
        }
        else if (S_ISREG(buf.st_mode) || strcmp(flag_devices, "read") == 0)
        {
          // do not exclude files that are overridden by ! negation
          bool negate = false;
          for (auto& glob : flag_exclude_override)
            if ((negate = globmat(pathname, basename, glob.c_str())))
              break;

          if (!negate)
          {
            // exclude files whose pathname matches any one of the --exclude globs
            for (auto& glob : flag_exclude)
              if (globmat(pathname, basename, glob.c_str()))
                return;
          }

          // check magic pattern against the file signature, when --file-magic=MAGIC is specified
          if (!flag_file_magic.empty())
          {
            FILE *file;

            if (fopen_s(&file, pathname, "r") == 0)
            {
              if (same_file(out, file))
              {
                fclose(file);
                return;
              }

#ifdef HAVE_LIBZ
              if (flag_decompress)
              {
                zstreambuf streambuf(file);
                std::istream stream(&streambuf);
                reflex::Input input(&stream);

                // file has the magic bytes we're looking for: search the file
                if (magic.input(input).scan() != 0)
                {
                  ++stats.files;

                  rewind(file);

                  if (ugrep(matcher, input, pathname))
                  {
                    ++stats.fileno;

                    fclose(file);

                    return;
                  }
                }
              }
              else
#endif
              {
                reflex::Input input(file, encoding);

                // file has the magic bytes we're looking for: search the file
                if (magic.input(input).scan() != 0)
                {
                  ++stats.files;

                  rewind(file);

                  if (ugrep(matcher, input, pathname))
                  {
                    ++stats.fileno;

                    fclose(file);

                    return;
                  }
                }
              }

              fclose(file);

              if (flag_include.empty())
                return;
            }
          }

          if (!flag_include.empty())
          {
            // do not include files that are overridden by ! negation
            for (auto& glob : flag_include_override)
              if (globmat(pathname, basename, glob.c_str()))
                return;

            // include files whose pathname matches any one of the --include globs
            bool ok = false;
            for (auto& glob : flag_include)
              if ((ok = globmat(pathname, basename, glob.c_str())))
                break;
            if (!ok)
              return;
          }

          FILE *file;

          if (fopen_s(&file, pathname, "r") != 0)
          {
            if (!flag_no_messages)
              warning("cannot read", pathname);

            return;
          }

          if (!same_file(out, file))
          {
#ifdef HAVE_LIBZ
            if (flag_decompress)
            {
              zstreambuf streambuf(file);
              std::istream stream(&streambuf);
              reflex::Input input(&stream);

              ++stats.files;

              if (ugrep(matcher, input, pathname))
                ++stats.fileno;
            }
            else
#endif
            {
              reflex::Input input(file, encoding);

              ++stats.files;

              if (ugrep(matcher, input, pathname))
                ++stats.fileno;
            }
          }

          fclose(file);
        }
      }
    }
  }
  else if (!flag_no_messages)
  {
    warning("cannot stat", pathname);
  }

#endif
}

// recurse over directory, searching for pattern matches in files and sub-directories
void recurse(Stats& stats, size_t level, reflex::Matcher& magic, reflex::AbstractMatcher& matcher, reflex::Input::file_encoding_type encoding, const char *pathname)
{
  // --max-depth: recursion level exceeds max depth?
  if (flag_max_depth > 0 && level > flag_max_depth)
    return;

  ++stats.dirs;

#ifdef OS_WIN

  WIN32_FIND_DATAA ffd;

  std::string glob;

  if (pathname && strcmp(pathname, ".") != 0)
    glob.assign(pathname).append(PATHSEPSTR).append("*");
  else
    glob.assign("*");

  HANDLE hFind = FindFirstFileA(glob.c_str(), &ffd);

  if (hFind == INVALID_HANDLE_VALUE) 
  {
    if (!flag_no_messages)
      warning("cannot open directory", pathname);

    return;
  } 

  std::string dirpathname;

  do
  {
    if (strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0)
    {
      // pathname is NULL when searching the working directory recursively
      if (pathname)
        dirpathname.assign(pathname).append(PATHSEPSTR).append(ffd.cFileName);
      else
        dirpathname.assign(ffd.cFileName);

      find(stats, level + 1, magic, matcher, encoding, dirpathname.c_str(), ffd.cFileName);

      // stop after finding max-files matching files
      if (flag_max_files > 0 && stats.fileno >= flag_max_files)
        break;
    }
  }
  while (FindNextFileA(hFind, &ffd) != 0);

  FindClose(hFind);

#else

  DIR *dir = opendir(pathname ? pathname : ".");

  if (dir == NULL)
  {
    if (!flag_no_messages)
      warning("cannot open directory", pathname);

    return;
  }

  struct dirent *dirent;
  std::string dirpathname;

  while ((dirent = readdir(dir)) != NULL)
  {
    // search directory entries that aren't . or ..
    if (strcmp(dirent->d_name, ".") != 0 && strcmp(dirent->d_name, "..") != 0)
    {
      // pathname is NULL when searching the working directory recursively
      if (pathname)
        dirpathname.assign(pathname).append(PATHSEPSTR).append(dirent->d_name);
      else
        dirpathname.assign(dirent->d_name);

      find(stats, level + 1, magic, matcher, encoding, dirpathname.c_str(), dirent->d_name);

      // stop after finding max-files matching files
      if (flag_max_files > 0 && stats.fileno >= flag_max_files)
        break;
    }
  }

  closedir(dir);

#endif
}

// search input, display pattern matches, return true when pattern matched anywhere
bool ugrep(reflex::AbstractMatcher& matcher, reflex::Input& input, const char *pathname)
{
  // mmap base and size, set with read_file() and mmap_file()
  const char *base = NULL;
  size_t size = 0;

  size_t matches = 0;

  if (flag_quiet || flag_files_with_match || flag_files_without_match)
  {
    // -q, -l, or -L: report if a single pattern match was found in the input

    read_file(matcher, input, base, size);

    matches = matcher.find() != 0;

    if (flag_invert_match)
      matches = !matches;

    // -l or -L without -q

    if (!flag_quiet && ((matches && flag_files_with_match) || (!matches && flag_files_without_match)))
    {
      fputs(color_fn, out);
      fputs(pathname, out);
      fputs(color_off, out);
      fputc(flag_null ? '\0' : '\n', out);

      if (flag_line_buffered)
        fflush(out);
    }
  }
  else if (flag_count)
  {
    // -c: count the number of lines/patterns matched

    if (flag_invert_match)
    {
      reflex::BufferedInput buffered_input;

      if (!mmap_file(input, base, size))
        buffered_input = input;

      const char *here = base;
      size_t left = size;

      std::string line;

      // -c with -v: count the number of non-matching lines
      while (true)
      {
        // read the next line from mmap, buffered input, or unbuffered input
        if (getline(here, left, buffered_input, input, line))
          break;

        read_line(matcher, line);

        // count this line if not matched
        if (matcher.find() == 0)
        {
          ++matches;

          // -m: max number of matches reached?
          if (flag_max_count > 0 && matches >= flag_max_count)
            break;
        }
      }
    }
    else if (flag_no_group)
    {
      // -c with -g: count the number of patterns matched in the file

      read_file(matcher, input, base, size);

      while (matcher.find() != 0)
      {
        ++matches;

        // -m: max number of matches reached?
        if (flag_max_count > 0 && matches >= flag_max_count)
          break;
      }
    }
    else
    {
      // -c without -g: count the number of matching lines

      size_t lineno = 0;

      read_file(matcher, input, base, size);

      for (auto& match : matcher.find)
      {
        if (lineno != match.lineno())
        {
          lineno = match.lineno();

          ++matches;

          // -m: max number of matches reached?
          if (flag_max_count > 0 && matches >= flag_max_count)
            break;
        }
      }
    }

    if (flag_with_filename)
    {
      fputs(color_fn, out);
      fputs(pathname, out);
      fputs(color_off, out);

      if (flag_null)
      {
        fputc('\0', out);
      }
      else
      {
        fputs(color_se, out);
        fputs(flag_separator, out);
        fputs(color_off, out);
      }

    }

    fprintf(out, "%zu\n", matches);

    if (flag_line_buffered)
      fflush(out);
  }
  else if (flag_only_matching || flag_only_line_number)
  {
    // -o or -N

    bool hex = false;
    size_t lineno = 0;
    const char *separator = flag_separator;

    read_file(matcher, input, base, size);

    for (auto& match : matcher.find)
    {
      separator = lineno != match.lineno() ? flag_separator : "+";

      if (flag_no_group || lineno != match.lineno())
      {
        // -m: max number of matches reached?
        if (flag_max_count > 0 && matches >= flag_max_count)
          break;

        lineno = match.lineno();

        ++matches;

        if (flag_only_line_number)
          display(pathname, lineno, match.columno() + 1, match.first(), separator, true);
      }

      if (!flag_only_line_number)
      {
        if (flag_hex)
        {
          hex_dump(HEX_MATCH, pathname, lineno, match.columno() + 1, match.first(), match.begin(), match.size(), separator);
          hex = true;
        }
        else if (!flag_text && is_binary(match.begin(), match.size()))
        {
          if (flag_with_hex)
          {
            if (hex)
            {
              hex_dump(HEX_MATCH, pathname, lineno, match.columno() + 1, match.first(), match.begin(), match.size(), separator);
            }
            else
            {
              display(pathname, lineno, match.columno() + 1, match.first(), separator, true);
              hex_dump(HEX_MATCH, NULL, 0, 0, match.first(), match.begin(), match.size(), separator);
              hex = true;
            }
          }
          else if (!flag_binary_without_matches)
          {
            display(pathname, lineno, match.columno() + 1, match.first(), separator, false);
            fprintf(out, "Binary file %s matches %zu bytes\n", pathname, match.size());
          }
        }
        else
        {
          if (hex)
            hex_done(separator);
          hex = false;

          display(pathname, lineno, match.columno() + 1, match.first(), separator, false);

          const char *begin = match.begin();
          size_t size = match.size();

          if (flag_line_number)
          {
            // -o with -n: echo multi-line matches line-by-line

            const char *from = begin;
            const char *to;

            while ((to = static_cast<const char*>(memchr(from, '\n', size - (from - begin)))) != NULL)
            {
              fputs(color_ms, out);
              fwrite(from, 1, to - from + 1, out);
              fputs(color_off, out);

              if (to + 1 < begin + size)
                display(pathname, ++lineno, 1, match.first() + (to - begin) + 1, "|", false);

              from = to + 1;
            }

            fputs(color_ms, out);
            fwrite(from, 1, size - (from - begin), out);
            fputs(color_off, out);
            if (size == 0 || begin[size - 1] != '\n')
              fputc('\n', out);
          }
          else
          {
            fputs(color_ms, out);
            fwrite(begin, 1, size, out);
            fputs(color_off, out);
            if (size == 0 || begin[size - 1] != '\n')
              fputc('\n', out);
          }

          if (flag_line_buffered)
            fflush(out);
        }
      }
    }

    if (hex)
      hex_done(separator);
  }
  else if (flag_before_context == 0 && flag_after_context == 0)
  {
    // read input line-by-line and display lines that match the pattern
    // this branch is the same as the next branch but optimized, with before and after context logic removed
    // TODO: further optimize to use a regular char* line pointing to mmap memory, when mmap is used

    reflex::BufferedInput buffered_input;

    if (!mmap_file(input, base, size) && is_file(input))
      buffered_input = input;

    const char *here = base;
    size_t left = size;

    size_t byte_offset = 0;
    size_t lineno = 1;

    std::string line;

    while (true)
    {
      // read the next line from mmap, buffered input, or unbuffered input
      if (getline(here, left, buffered_input, input, line))
        break;

      bool binary = flag_hex;

      if (!flag_text && !flag_hex && is_binary(line.c_str(), line.size()))
      {
        if (flag_binary_without_matches)
        {
          matches = 0;
          break;
        }
        binary = true;
      }

      size_t last = UNDEFINED;

      // the current input line to match
      read_line(matcher, line);

      if (flag_invert_match)
      {
        // -v: select non-matching line

        bool found = false;

        for (auto& match : matcher.find)
        {
          if (flag_any_line)
          {
            if (last == UNDEFINED)
            {
              display(pathname, lineno, match.columno() + 1, byte_offset, "-", binary);

              last = 0;
            }

            if (binary)
            {
              hex_dump(HEX_CONTEXT_LINE, NULL, 0, 0, byte_offset + last, line.c_str() + last, match.first() - last, "-");
            }
            else
            {
              fputs(color_cx, out);
              fwrite(line.c_str() + last, 1, match.first() - last, out);
              fputs(color_off, out);
            }

            last = match.last();

            // skip any further empty pattern matches
            if (last == 0)
              break;

            if (binary)
            {
              hex_dump(HEX_CONTEXT_MATCH, NULL, 0, 0, byte_offset + match.first(), match.begin(), match.size(), "-");
            }
            else
            {
              fputs(color_mc, out);
              fwrite(match.begin(), 1, match.size(), out);
              fputs(color_off, out);
            }
          }
          else
          {
            found = true;

            break;
          }
        }

        if (last != UNDEFINED)
        {
          if (binary)
          {
            hex_dump(HEX_CONTEXT_LINE, NULL, 0, 0, byte_offset + last, line.c_str() + last, line.size() - last, "-");
            hex_done("-");
          }
          else
          {
            fputs(color_cx, out);
            fwrite(line.c_str() + last, 1, line.size() - last, out);
            fputs(color_off, out);
          }
        }
        else if (!found)
        {
          if (binary && !flag_hex && !flag_with_hex)
          {
            fprintf(out, "Binary file %s matches\n", pathname);
            matches = 1;
            break;
          }

          fputs(color_sl, out);
          fwrite(line.c_str(), 1, line.size(), out);
          fputs(color_off, out);

          if (flag_line_buffered)
            fflush(out);

          ++matches;

          // -m: max number of matches reached?
          if (flag_max_count > 0 && matches >= flag_max_count)
            break;
        }
      }
      else
      {
        // search the line for pattern matches

        for (auto& match : matcher.find)
        {
          if (last == UNDEFINED && binary && !flag_hex && !flag_with_hex)
          {
            fprintf(out, "Binary file %s matches\n", pathname);
            matches = 1;
            goto exit_ugrep;
          }

          if (flag_no_group)
          {
            // -g: do not group matches on a single line but on multiple lines, counting each match separately

            display(pathname, lineno, match.columno() + 1, byte_offset + match.first(), last == UNDEFINED ? flag_separator : "+", binary);

            if (binary)
            {
              hex_dump(HEX_LINE, NULL, 0, 0, byte_offset, line.c_str(), match.first(), "+");
              hex_dump(HEX_MATCH, NULL, 0, 0, byte_offset + match.first(), match.begin(), match.size(), "+");
              hex_dump(HEX_LINE, NULL, 0, 0, byte_offset + match.last(), line.c_str() + match.last(), match.last() - match.first(), "+");
              hex_done("+");
            }
            else
            {
              fputs(color_sl, out);
              fwrite(line.c_str(), 1, match.first(), out);
              fputs(color_off, out);
              fputs(color_ms, out);
              fwrite(match.begin(), 1, match.size(), out);
              fputs(color_off, out);
              fputs(color_sl, out);
              fwrite(line.c_str() + match.last(), 1, line.size() - match.last(), out);
              fputs(color_off, out);
            }

            ++matches;

            // -m: max number of matches reached?
            if (flag_max_count > 0 && matches >= flag_max_count)
              goto exit_ugrep;
          }
          else
          {
            if (last == UNDEFINED)
            {
              display(pathname, lineno, match.columno() + 1, byte_offset, flag_separator, binary);

              ++matches;

              last = 0;
            }

            if (binary)
            {
              hex_dump(HEX_LINE, NULL, 0, 0, byte_offset + last, line.c_str() + last, match.first() - last, flag_separator);
              hex_dump(HEX_MATCH, NULL, 0, 0, byte_offset + match.first(), match.begin(), match.size(), flag_separator);
            }
            else
            {
              fputs(color_sl, out);
              fwrite(line.c_str() + last, 1, match.first() - last, out);
              fputs(color_off, out);
              fputs(color_ms, out);
              fwrite(match.begin(), 1, match.size(), out);
              fputs(color_off, out);
            }
          }

          last = match.last();

          // skip any further empty pattern matches
          if (last == 0)
            break;
        }

        if (last != UNDEFINED)
        {
          if (!flag_no_group)
          {
            if (binary)
            {
              hex_dump(HEX_LINE, NULL, 0, 0, byte_offset + last, line.c_str() + last, line.size() - last, flag_separator);
              hex_done(flag_separator);
            }
            else
            {
              fputs(color_sl, out);
              fwrite(line.c_str() + last, 1, line.size() - last, out);
              fputs(color_off, out);
            }
          }

          if (flag_line_buffered)
            fflush(out);
        }
        else if (flag_any_line)
        {
          display(pathname, lineno, 1, byte_offset, "-", binary);

          if (binary)
          {
            hex_dump(HEX_CONTEXT_LINE, NULL, 0, 0, byte_offset, line.c_str(), line.size(), "-");
            hex_done("-");
          }
          else
          {
            fputs(color_cx, out);
            fwrite(line.c_str(), 1, line.size(), out);
            fputs(color_off, out);
          }
        }

        // -m: max number of matches reached?
        if (flag_max_count > 0 && matches >= flag_max_count)
          break;
      }

      // update byte offset and line number
      byte_offset += line.size();
      ++lineno;
    }
  }
  else
  {
    // read input line-by-line and display lines that match the pattern with context lines

    reflex::BufferedInput buffered_input;

    if (!mmap_file(input, base, size) && is_file(input))
      buffered_input = input;

    const char *here = base;
    size_t left = size;

    size_t byte_offset = 0;
    size_t lineno = 1;
    size_t before = 0;
    size_t after = 0;

    std::vector<bool> binary;
    std::vector<size_t> byte_offsets;
    std::vector<std::string> lines;

    binary.reserve(flag_before_context + 1);
    byte_offsets.reserve(flag_before_context + 1);
    lines.reserve(flag_before_context + 1);

    for (size_t i = 0; i <= flag_before_context; ++i)
    {
      binary[i] = false;
      byte_offsets.emplace_back(0);
      lines.emplace_back("");
    }

    while (true)
    {
      size_t current = lineno % (flag_before_context + 1);

      binary[current] = flag_hex;
      byte_offsets[current] = byte_offset;

      // read the next line from mmap, buffered input, or unbuffered input
      if (getline(here, left, buffered_input, input, lines[current]))
        break;

      if (!flag_text && !flag_hex && is_binary(lines[current].c_str(), lines[current].size()))
      {
        if (flag_binary_without_matches)
        {
          matches = 0;
          break;
        }
        binary[current] = true;
      }

      bool before_context = flag_before_context > 0;
      bool after_context = flag_after_context > 0;

      size_t last = UNDEFINED;

      // the current input line to match
      read_line(matcher, lines[current]);

      if (flag_invert_match)
      {
        // -v: select non-matching line

        bool found = false;

        for (auto& match : matcher.find)
        {
          if (flag_any_line || (after > 0 && after + flag_after_context >= lineno))
          {
            // -A NUM: show context after matched lines, simulates BSD grep -A

            if (last == UNDEFINED)
            {
              display(pathname, lineno, match.columno() + 1, byte_offset, "-", binary[current]);

              last = 0;
            }

            if (binary[current])
            {
              hex_dump(HEX_CONTEXT_LINE, NULL, 0, 0, byte_offsets[current] + last, lines[current].c_str() + last, match.first() - last, "-");
            }
            else
            {
              fputs(color_cx, out);
              fwrite(lines[current].c_str() + last, 1, match.first() - last, out);
              fputs(color_off, out);
            }

            last = match.last();

            // skip any further empty pattern matches
            if (last == 0)
              break;

            if (binary[current])
            {
              hex_dump(HEX_CONTEXT_MATCH, NULL, 0, 0, byte_offsets[current] + match.first(), match.begin(), match.size(), "-");
            }
            else
            {
              fputs(color_mc, out);
              fwrite(match.begin(), 1, match.size(), out);
              fputs(color_off, out);
            }
          }
          else
          {
            found = true;

            break;
          }
        }

        if (last != UNDEFINED)
        {
          if (binary[current])
          {
            hex_dump(HEX_CONTEXT_LINE, NULL, 0, 0, byte_offsets[current] + last, lines[current].c_str() + last, lines[current].size() - last, "-");
            hex_done("-");
          }
          else
          {
            fputs(color_cx, out);
            fwrite(lines[current].c_str() + last, 1, lines[current].size() - last, out);
            fputs(color_off, out);
          }
        }
        else if (!found)
        {
          if (binary[current] && !flag_hex && !flag_with_hex)
          {
            fprintf(out, "Binary file %s matches\n", pathname);
            matches = 1;
            break;
          }

          if (after_context)
          {
            // -A NUM: show context after matched lines, simulates BSD grep -A

            // indicate the end of the group of after lines of the previous matched line
            if (after + flag_after_context < lineno && matches > 0 && flag_group_separator != NULL)
            {
              fputs(color_se, out);
              fputs(flag_group_separator, out);
              fputs(color_off, out);
              fputc('\n', out);
            }

            // remember the matched line
            after = lineno;
          }

          if (before_context)
          {
            // -B NUM: show context before matched lines, simulates BSD grep -B

            size_t begin = before + 1;

            if (lineno > flag_before_context && begin < lineno - flag_before_context)
              begin = lineno - flag_before_context;

            // indicate the begin of the group of before lines
            if (begin < lineno && matches > 0 && flag_group_separator != NULL)
            {
              fputs(color_se, out);
              fputs(flag_group_separator, out);
              fputs(color_off, out);
              fputc('\n', out);
            }

            // display lines before the matched line
            while (begin < lineno)
            {
              size_t begin_context = begin % (flag_before_context + 1);

              last = UNDEFINED;

              read_line(matcher, lines[begin_context]);

              for (auto& match : matcher.find)
              {
                if (last == UNDEFINED)
                {
                  display(pathname, begin, match.columno() + 1, byte_offsets[begin_context], "-", binary[begin_context]);

                  last = 0;
                }

                if (binary[begin_context])
                {
                  hex_dump(HEX_CONTEXT_LINE, NULL, 0, 0, byte_offsets[begin_context] + last, lines[begin_context].c_str() + last, match.first() - last, "-");
                }
                else
                {
                  fputs(color_cx, out);
                  fwrite(lines[begin_context].c_str() + last, 1, match.first() - last, out);
                  fputs(color_off, out);
                }

                last = match.last();

                // skip any further empty pattern matches
                if (last == 0)
                  break;

                if (binary[begin_context])
                {
                  hex_dump(HEX_CONTEXT_MATCH, NULL, 0, 0, byte_offsets[begin_context] + match.first(), match.begin(), match.size(), "-");
                }
                else
                {
                  fputs(color_mc, out);
                  fwrite(match.begin(), 1, match.size(), out);
                  fputs(color_off, out);
                }
              }

              if (last != UNDEFINED)
              {
                if (binary[begin % (flag_before_context + 1)])
                {
                  hex_dump(HEX_CONTEXT_LINE, NULL, 0, 0, byte_offsets[begin_context] + last, lines[begin_context].c_str() + last, lines[begin_context].size() - last, "-");
                  hex_done("-");
                }
                else
                {
                  fputs(color_cx, out);
                  fwrite(lines[begin_context].c_str() + last, 1, lines[begin_context].size() - last, out);
                  fputs(color_off, out);
                }
              }

              ++begin;
            }

            // remember the matched line
            before = lineno;
          }

          display(pathname, lineno, 1, byte_offsets[current], flag_separator, binary[current]);

          if (binary[current])
          {
            hex_dump(HEX_LINE, NULL, 0, 0, byte_offsets[current], lines[current].c_str(), lines[current].size(), flag_separator);
            hex_done(flag_separator);
          }
          else
          {
            fputs(color_sl, out);
            fwrite(lines[current].c_str(), 1, lines[current].size(), out);
            fputs(color_off, out);
          }

          if (flag_line_buffered)
            fflush(out);

          ++matches;

          // -m: max number of matches reached?
          if (flag_max_count > 0 && matches >= flag_max_count)
            break;
        }
      }
      else
      {
        // search the line for pattern matches

        for (auto& match : matcher.find)
        {
          if (last == UNDEFINED && binary[current] && !flag_hex && !flag_with_hex)
          {
            fprintf(out, "Binary file %s matches\n", pathname);
            matches = 1;
            goto exit_ugrep;
          }

          if (after_context)
          {
            // -A NUM: show context after matched lines, simulates BSD grep -A

            // indicate the end of the group of after lines of the previous matched line
            if (after + flag_after_context < lineno && matches > 0 && flag_group_separator != NULL)
            {
              fputs(color_se, out);
              fputs(flag_group_separator, out);
              fputs(color_off, out);
              fputc('\n', out);
            }

            // remember the matched line and we're done with the after context
            after = lineno;
            after_context = false;
          }

          if (before_context)
          {
            // -B NUM: show context before matched lines, simulates BSD grep -B

            size_t begin = before + 1;

            if (lineno > flag_before_context && begin < lineno - flag_before_context)
              begin = lineno - flag_before_context;

            // indicate the begin of the group of before lines
            if (begin < lineno && matches > 0 && flag_group_separator != NULL)
            {
              fputs(color_se, out);
              fputs(flag_group_separator, out);
              fputs(color_off, out);
              fputc('\n', out);
            }

            // display lines before the matched line
            while (begin < lineno)
            {
              size_t begin_context = begin % (flag_before_context + 1);

              display(pathname, begin, 1, byte_offsets[begin_context], "-", binary[begin_context]);

              if (binary[begin_context])
              {
                hex_dump(HEX_CONTEXT_LINE, NULL, 0, 0, byte_offsets[begin_context], lines[begin_context].c_str(), lines[begin_context].size(), "-");
                hex_done("-");
              }
              else
              {
                fputs(color_cx, out);
                fwrite(lines[begin_context].c_str(), 1, lines[begin_context].size(), out);
                fputs(color_off, out);
              }

              ++begin;
            }

            // remember the matched line and we're done with the before context
            before = lineno;
            before_context = false;
          }

          if (flag_no_group)
          {
            // -g: do not group matches on a single line but on multiple lines, counting each match separately

            display(pathname, lineno, match.columno() + 1, byte_offset + match.first(), last == UNDEFINED ? flag_separator : "+", binary[current]);

            if (binary[current])
            {
              hex_dump(HEX_LINE, NULL, 0, 0, byte_offsets[current], lines[current].c_str(), match.first(), "+");
              hex_dump(HEX_MATCH, NULL, 0, 0, byte_offsets[current] + match.first(), match.begin(), match.size(), "+");
              hex_dump(HEX_LINE, NULL, 0, 0, byte_offsets[current] + match.last(), lines[current].c_str() + match.last(), match.last() - match.first(), "+");
              hex_done("+");
            }
            else
            {
              fputs(color_sl, out);
              fwrite(lines[current].c_str(), 1, match.first(), out);
              fputs(color_off, out);
              fputs(color_ms, out);
              fwrite(match.begin(), 1, match.size(), out);
              fputs(color_off, out);
              fputs(color_sl, out);
              fwrite(lines[current].c_str() + match.last(), 1, lines[current].size() - match.last(), out);
              fputs(color_off, out);
            }

            ++matches;

            // -m: max number of matches reached?
            if (flag_max_count > 0 && matches >= flag_max_count)
              goto exit_ugrep;
          }
          else
          {
            if (last == UNDEFINED)
            {
              display(pathname, lineno, match.columno() + 1, byte_offset, flag_separator, binary[current]);

              ++matches;

              last = 0;
            }

            if (binary[current])
            {
              hex_dump(HEX_LINE, NULL, 0, 0, byte_offsets[current] + last, lines[current].c_str() + last, match.first() - last, flag_separator);
              hex_dump(HEX_MATCH, NULL, 0, 0, byte_offsets[current] + match.first(), match.begin(), match.size(), flag_separator);
            }
            else
            {
              fputs(color_sl, out);
              fwrite(lines[current].c_str() + last, 1, match.first() - last, out);
              fputs(color_off, out);
              fputs(color_ms, out);
              fwrite(match.begin(), 1, match.size(), out);
              fputs(color_off, out);
            }
          }

          last = match.last();

          // skip any further empty pattern matches
          if (last == 0)
            break;
        }

        if (last != UNDEFINED)
        {
          if (!flag_no_group)
          {
            if (binary[current])
            {
              hex_dump(HEX_LINE, NULL, 0, 0, byte_offsets[current] + last, lines[current].c_str() + last, lines[current].size() - last, flag_separator);
              hex_done(flag_separator);
            }
            else
            {
              fputs(color_sl, out);
              fwrite(lines[current].c_str() + last, 1, lines[current].size() - last, out);
              fputs(color_off, out);
            }
          }

          if (flag_line_buffered)
            fflush(out);
        }
        else if (flag_any_line || (after > 0 && after + flag_after_context >= lineno))
        {
          // -A NUM: show context after matched lines, simulates BSD grep -A

          // display line as part of the after context of the matched line
          display(pathname, lineno, 1, byte_offsets[current], "-", binary[current]);

          if (binary[current])
          {
            hex_dump(HEX_CONTEXT_LINE, NULL, 0, 0, byte_offsets[current], lines[current].c_str(), lines[current].size(), "-");
            hex_done("-");
          }
          else
          {
            fputs(color_cx, out);
            fwrite(lines[current].c_str(), 1, lines[current].size(), out);
            fputs(color_off, out);
          }
        }

        // -m: max number of matches reached?
        if (flag_max_count > 0 && matches >= flag_max_count)
          break;
      }

      // update byte offset and line number
      byte_offset += lines[current].size();
      ++lineno;
    }
  }

exit_ugrep:

  // if mmap was used, then deallocate
  munmap_file(base, size);

  // --break: add a line break and flush
  if ((matches > 0 || flag_any_line) && flag_break)
  {
    fputc('\n', out);
    fflush(out);
  }

  return matches > 0;
}

// display the header part of the match, preceeding the matched line
void display(const char *name, size_t lineno, size_t columno, size_t byte_offset, const char *separator, bool newline)
{
  if (name != NULL)
  {
    bool sep = false;

    if (flag_with_filename)
    {
      fputs(color_fn, out);
      fputs(name, out);
      fputs(color_off, out);

      if (flag_null)
        fputc('\0', out);
      else
        sep = true;
    }

    if (flag_line_number || flag_only_line_number)
    {
      if (sep)
      {
        fputs(color_se, out);
        fputs(separator, out);
        fputs(color_off, out);
      }

      fputs(color_ln, out);
      fprintf(out, flag_initial_tab ? "%6zu" : "%zu", lineno);
      fputs(color_off, out);

      sep = true;
    }

    if (flag_column_number)
    {
      if (sep)
      {
        fputs(color_se, out);
        fputs(separator, out);
        fputs(color_off, out);
      }

      fputs(color_ln, out);
      fprintf(out, flag_initial_tab ? "%3zu" : "%zu", columno);
      fputs(color_off, out);

      sep = true;
    }

    if (flag_byte_offset)
    {
      if (sep)
      {
        fputs(color_se, out);
        fputs(separator, out);
        fputs(color_off, out);
      }

      fputs(color_ln, out);
      fprintf(out, flag_hex ? flag_initial_tab ? "%7zx" : "%zx" : flag_initial_tab ? "%7zu" : "%zu", byte_offset);
      fputs(color_off, out);

      sep = true;
    }

    if (sep)
    {
      fputs(color_se, out);
      fputs(separator, out);
      fputs(color_off, out);

      if (flag_initial_tab)
        fputc('\t', out);

      if (newline)
        fputc('\n', out);
    }
  }
}

// dump data in hex
void hex_dump(short mode, const char *pathname, size_t lineno, size_t columno, size_t byte_offset, const char *data, size_t size, const char *separator)
{
  if (pathname == NULL)
    last_hex_offset = byte_offset;

  if (size > 0)
  {
    if (last_hex_offset == 0 || last_hex_offset < byte_offset)
    {
      if ((last_hex_offset & (size_t)0x0f) > 0)
        hex_line(separator);
      if (pathname != NULL)
        display(pathname, lineno, columno, byte_offset, separator, true);
    }

    last_hex_offset = byte_offset;

    while (last_hex_offset < byte_offset + size)
    {
      last_hex_line[last_hex_offset++ & 0x0f] = (mode << 8) | *(unsigned char*)data++;
      if ((last_hex_offset & 0x0f) == 0)
        hex_line(separator);
    }
  }
}

// done dumping hex
void hex_done(const char *separator)
{
  if ((last_hex_offset & 0x0f) != 0)
    hex_line(separator);
}

// dump one line of hex data
void hex_line(const char *separator)
{
  fputs(color_bn, out);
  fprintf(out, "%.8zx", (last_hex_offset - 1) & ~(size_t)0x0f);
  fputs(color_off, out);
  fputs(color_se, out);
  fputs(separator, out);
  fputs(color_off, out);
  fputc(' ', out);

  for (size_t i = 0; i < 0x10; ++i)
  {
    if (last_hex_line[i] < 0)
    {
      fputs(color_cx, out);
      fputs(" --", out);
      fputs(color_off, out);
    }
    else
    {
      short byte = last_hex_line[i];

      fputs(color_hex[byte >> 8], out);
      fprintf(out, " %.2x", byte & 0xff);

      fputs(color_off, out);
    }
  }

  fputs("  ", out);

  for (size_t i = 0; i < 0x10; ++i)
  {
    if (last_hex_line[i] < 0)
    {
      fputs(color_cx, out);
      fputc('-', out);
      fputs(color_off, out);
    }
    else
    {
      short byte = last_hex_line[i];

      fputs(color_hex[byte >> 8], out);

      byte &= 0xff;

      if (byte < 0x20 && flag_color)
        fprintf(out, "\033[7m%c", '@' + byte);
      else if (byte == 0x7f && flag_color)
        fputs("\033[7m~", out);
      else if (byte < 0x20 || byte >= 0x7f)
        fputc(' ', out);
      else
        fprintf(out, "%c", byte);

      fputs(color_off, out);
    }
  }

  fputc('\n', out);

  if (flag_line_buffered)
    fflush(out);

  for (size_t i = 0; i < 0x10; ++i)
    last_hex_line[i] = -1;
}

// trim line to remove leading and trailing white space
void trim(std::string& line)
{
  size_t len = line.length();
  size_t pos;

  for (pos = 0; pos < len && isspace(line.at(pos)); ++pos)
    continue;

  if (pos > 0)
    line.erase(0, pos);

  for (pos = len - pos; pos > 0 && isspace(line.at(pos - 1)); --pos)
    continue;

  line.erase(pos);
}

// convert GREP_COLORS and set the color substring to the ANSI SGR sequence
void set_color(const char *grep_colors, const char *parameter, char color[COLORLEN])
{
  const char *substr = strstr(grep_colors, parameter);

  // check if substring parameter is present in GREP_COLORS
  if (substr != NULL && substr[2] == '=')
  {
    substr += 3;
    const char *colon = substr;

    while (*colon && (isdigit(*colon) || *colon == ';'))
      ++colon;

    size_t sublen = colon - substr;

    if (sublen > 0 && sublen < COLORLEN - 4)
    {
      color[0] = '\033';
      color[1] = '[';
      memcpy(color + 2, substr, sublen);
      color[sublen + 2] = 'm';
      color[sublen + 3] = '\0';
    }
  }
}

// check if two FILE* refer to the same file
bool same_file(FILE *file1, FILE *file2)
{
#ifdef OS_WIN
  return false; // TODO check that two FILE* on Windows are the same, is this possible?
#else
  int fd1 = fileno(file1);
  int fd2 = fileno(file2);
  struct stat stat1, stat2;
  if (fstat(fd1, &stat1) < 0 || fstat(fd2, &stat2) < 0)
    return false;
  return stat1.st_dev == stat2.st_dev && stat1.st_ino == stat2.st_ino;
#endif
}

// return true if input is a regular file
bool is_file(const reflex::Input& input)
{
  if (input.file() == NULL)
    return false;
#ifdef OS_WIN
  int fd = _fileno(input.file());
  struct _stat st;
  return _fstat(fd, &st) == 0 && ((st.st_mode & S_IFMT) == S_IFREG);
#else
  int fd = ::fileno(input.file());
  struct stat st;
  return ::fstat(fd, &st) == 0 && S_ISREG(st.st_mode);
#endif
}

// specify input to read for matcher, when inout is a regular file then try mmap for zero copy overhead
void read_file(reflex::AbstractMatcher& matcher, reflex::Input& input, const char*& base, size_t& size)
{
  // attempt to mmap the input file
  if (mmap_file(input, base, size))
  {
    matcher.buffer(const_cast<char*>(base), size + 1); // cast is safe: base is not modified
  }
  else
  {
    matcher.input(input);
#ifdef HAVE_BOOST_REGEX
    // buffer all input to work around Boost.Regex bug
    if (flag_perl_regexp)
      matcher.buffer();
#endif
  }
}

// attempt to mmap the given file-based input, return true if successful with base and size
bool mmap_file(reflex::Input& input, const char*& base, size_t& size)
{
  base = NULL;
  size = 0;
#if !defined(OS_WIN) && MAX_MMAP_SIZE > 0
  FILE *file = input.file();
  if (file == NULL || input.file_encoding() != reflex::Input::file_encoding::plain)
    return false;
  int fd = ::fileno(file);
  struct stat st;
  if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size > MAX_MMAP_SIZE)
    return false;
  size = static_cast<size_t>(st.st_size);
  base = static_cast<const char*>(::mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0));
  if (base != MAP_FAILED)
    return true;
  base = NULL;
#else
  (void)input;
#endif
  return false;
}

// munmap the file at mmap base
void munmap_file(const char *base, size_t size)
{
#if MAX_MMAP_SIZE > 0 && !defined(OS_WIN)
  if (base != NULL)
    ::munmap(const_cast<void*>(static_cast<const void*>(base)), size);
#else
  (void)base, (void)size;
#endif
}

// display warning message assuming errno is set, like perror()
void warning(const char *message, const char *arg)
{
  // use safe strerror_s() instead of strerror() when available
#if defined(__STDC_LIB_EXT1__) || defined(OS_WIN)
  char errmsg[256]; 
  strerror_s(errmsg, sizeof(errmsg), errno);
#else
  const char *errmsg = strerror(errno);
#endif
  fprintf(stderr, "ugrep: %s %s: %s\n", message, arg, errmsg);
}

// display error message assuming errno is set, like perror(), then exit
void error(const char *message, const char *arg)
{
  warning(message, arg);
  exit(EXIT_ERROR);
}

// display usage/help information with an optional diagnostic message and exit
void help(const char *message, const char *arg)
{
  if (message && *message)
    std::cout << "ugrep: " << message << (arg != NULL ? arg : "") << std::endl;
  std::cout << "Usage: ugrep [OPTIONS] [PATTERN] [-e PATTERN] [-f FILE] [FILE ...]\n";
  if (!message)
  {
    std::cout << "\n\
    -A NUM, --after-context=NUM\n\
            Print NUM lines of trailing context after matching lines.  Places\n\
            a --group-separator between contiguous groups of matches.  See also\n\
            the -B, -C, and -y options.\n\
    -a, --text\n\
            Process a binary file as if it were text.  This is equivalent to\n\
            the --binary-files=text option.  This option might output binary\n\
            garbage to the terminal, which can have problematic consequences if\n\
            the terminal driver interprets some of it as commands.\n\
    -B NUM, --before-context=NUM\n\
            Print NUM lines of leading context before matching lines.  Places\n\
            a --group-separator between contiguous groups of matches.  See also\n\
            the -A, -C, and -y options.\n\
    -b, --byte-offset\n\
            The offset in bytes of a matched line is displayed in front of the\n\
            respective matched line.  When used with option -g, displays the\n\
            offset in bytes of each pattern matched.  Byte offsets are exact\n\
            for binary, ASCII, and UTF-8 input.  Otherwise, the byte offset in\n\
            the UTF-8-converted input is displayed.\n\
    --binary-files=TYPE\n\
            Controls searching and reporting pattern matches in binary files.\n\
            Options are `binary', `without-match`, `text`, `hex`, and\n\
            `with-hex'.  The default is `binary' to search binary files and to\n\
            report a match without displaying the match.  `without-match'\n\
            ignores binary matches.  `text' treats all binary files as text,\n\
            which might output binary garbage to the terminal, which can have\n\
            problematic consequences if the terminal driver interprets some of\n\
            it as commands.  `hex' reports all matches in hexadecimal.\n\
            `with-hex` only reports binary matches in hexadecimal, leaving text\n\
            matches alone.  A match is considered binary if a match contains a\n\
            zero byte or invalid UTF encoding.  See also the -a, -I, -U, -W,\n\
            and -X options.\n\
    --break\n\
            Adds a line break between results from different files.\n\
    -C[NUM], --context[=NUM]\n\
            Print NUM lines of leading and trailing context surrounding each\n\
            match.  The default is 2 and is equivalent to -A 2 -B 2.  Places\n\
            a --group-separator between contiguous groups of matches.\n\
            No whitespace may be given between -C and its argument NUM.\n\
    -c, --count\n\
            Only a count of selected lines is written to standard output.\n\
            When used with option -g, counts the number of patterns matched.\n\
            With option -v, counts the number of non-matching lines.\n\
    --color[=WHEN], --colour[=WHEN]\n\
            Mark up the matching text with the expression stored in the\n\
            GREP_COLOR or GREP_COLORS environment variable.  The possible\n\
            values of WHEN can be `never', `always', or `auto', where `auto'\n\
            marks up matches only when output on a terminal.\n\
    -D ACTION, --devices=ACTION\n\
            If an input file is a device, FIFO or socket, use ACTION to process\n\
            it.  By default, ACTION is `read', which means that devices are\n\
            read just as if they were ordinary files.  If ACTION is `skip',\n\
            devices are silently skipped.\n\
    -d ACTION, --directories=ACTION\n\
            If an input file is a directory, use ACTION to process it.  By\n\
            default, ACTION is `read', i.e., read directories just as if they\n\
            were ordinary files.  If ACTION is `skip', silently skip\n\
            directories.  If ACTION is `recurse', read all files under each\n\
            directory, recursively, following symbolic links only if they are\n\
            on the command line.  This is equivalent to the -r option.  If\n\
            ACTION is `dereference-recurse', read all files under each\n\
            directory, recursively, following symbolic links.  This is\n\
            equivalent to the -R option.\n\
    -E, --extended-regexp\n\
            Interpret patterns as extended regular expressions (EREs). This is\n\
            the default.\n\
    -e PATTERN, --regexp=PATTERN\n\
            Specify a PATTERN used during the search of the input: an input\n\
            line is selected if it matches any of the specified patterns.\n\
            This option is most useful when multiple -e options are used to\n\
            specify multiple patterns, when a pattern begins with a dash (`-'),\n\
            to specify a pattern after option -f or after the FILE arguments.\n\
    --exclude=GLOB\n\
            Skip files whose name matches GLOB (using wildcard matching).  A\n\
            glob can use *, ?, and [...] as wildcards, and \\ to quote a\n\
            wildcard or backslash character literally.  If GLOB contains /,\n\
            full pathnames are matched.  Otherwise basenames are matched.  Note\n\
            that --exclude patterns take priority over --include patterns.\n\
            This option may be repeated.\n\
    --exclude-dir=GLOB\n\
            Exclude directories whose name matches GLOB from recursive\n\
            searches.  If GLOB contains /, full pathnames are matched.\n\
            Otherwise basenames are matched.  Note that --exclude-dir patterns\n\
            take priority over --include-dir patterns.  This option may be\n\
            repeated.\n\
    --exclude-from=FILE\n\
            Read the globs from FILE and skip files and directories whose name\n\
            matches one or more globs (as if specified by --exclude and\n\
            --exclude-dir).  Lines starting with a `#' and empty lines in FILE\n\
            ignored.  When FILE is a a `-', standard input is read.  This\n\
            option may be repeated.\n\
    -F, --fixed-strings\n\
            Interpret pattern as a set of fixed strings, separated by newlines,\n\
            any of which is to be matched.  This makes ugrep behave as fgrep.\n\
            This option does not apply to -f FILE patterns.  To apply -F to\n\
            patterns in FILE use -Fe `cat FILE`.\n\
    -f FILE, --file=FILE\n\
            Read one or more newline-separated patterns from FILE.  Empty\n\
            pattern lines in the file are not processed.  Options -F, -w, and\n\
            -x do not apply to FILE patterns.  If FILE does not exist, the\n\
            GREP_PATH environment variable is used as the path to read FILE.\n"
#ifdef GREP_PATH
"\
            If that fails, looks for FILE in " GREP_PATH ".\n"
#endif
"\
            When FILE is a `-', standard input is read.  This option may be\n\
            repeated.\n\
    --free-space\n\
            Spacing (blanks and tabs) in regular expressions are ignored.\n\
    -G, --basic-regexp\n\
            Interpret pattern as a basic regular expression, i.e. make ugrep\n\
            behave as traditional grep.\n\
    -g, --no-group\n\
            Do not group multiple pattern matches on the same matched line.\n\
            Output the matched line again for each additional pattern match,\n\
            using `+' as the field separator for each additional match.\n\
    --group-separator=SEP\n\
            Use SEP as a group separator for context options -A, -B, and -C. By\n\
            default SEP is a double hyphen (`--').\n\
    -H, --with-filename\n\
            Always print the filename with output lines.  This is the default\n\
            when there is more than one file to search.\n\
    -h, --no-filename\n\
            Never print filenames with output lines.\n\
    --help\n\
            Print a help message.\n\
    -I\n\
            Ignore matches in binary files.  This option is equivalent to the\n\
            --binary-files=without-match option.\n\
    -i, --ignore-case\n\
            Perform case insensitive matching.  By default, ugrep is case\n\
            sensitive.  This option applies to ASCII letters only.\n\
    --include=GLOB\n\
            Search only files whose name matches GLOB (using wildcard\n\
            matching).  A glob can use *, ?, and [...] as wildcards, and \\ to\n\
            quote a wildcard or backslash character literally.  If GLOB\n\
            contains /, file pathnames are matched.  Otherwise file basenames\n\
            are matched.  Note that --exclude patterns take priority over\n\
            --include patterns.  This option may be repeated.\n\
    --include-dir=GLOB\n\
            Only directories whose name matches GLOB are included in recursive\n\
            searches.  If GLOB contains /, full pathnames are matched.\n\
            Otherwise basenames are matched.  Note that --exclude-dir patterns\n\
            take priority over --include-dir patterns.  This option may be\n\
            repeated.\n\
    --include-from=FILE\n\
            Read the globs from FILE and search only files and directories\n\
            whose name matches one or more globs (as if specified by --include\n\
            and --include-dir).  Lines starting with a `#' and empty lines in\n\
            FILE are ignored.  When FILE is a `-', standard input is read.\n\
            This option may be repeated.\n\
    -J[NUM], --jobs[=NUM]\n\
            Specifies the number of jobs to run simultaneously to search files.\n\
            Without argument NUM, the number of jobs spawned is optimized.\n\
            No whitespace may be given between -J and its argument NUM.\n\
            This feature is not available in this version of ugrep.\n\
    -j, --smart-case\n\
            Perform case insensitive matching unless PATTERN contains a capital\n\
            letter.  Case insensitive matching applies to ASCII letters only.\n\
    -k, --column-number\n\
            The column number of a matched pattern is displayed in front of the\n\
            respective matched line, starting at column 1.  Tabs are expanded\n\
            when columns are counted, see option --tabs.\n\
    -L, --files-without-match\n\
            Only the names of files not containing selected lines are written\n\
            to standard output.  Pathnames are listed once per file searched.\n\
            If the standard input is searched, the string ``(standard input)''\n\
            is written.\n\
    -l, --files-with-matches\n\
            Only the names of files containing selected lines are written to\n\
            standard output.  ugrep will only search a file until a match has\n\
            been found, making searches potentially less expensive.  Pathnames\n\
            are listed once per file searched.  If the standard input is\n\
            searched, the string ``(standard input)'' is written.\n\
    --label[=LABEL]\n\
            Displays the LABEL value when input is read from standard input\n\
            where a file name would normally be printed in the output.  This\n\
            option applies to options -H, -L, and -l.\n\
    --line-buffered\n\
            Force output to be line buffered.  By default, output is line\n\
            buffered when standard output is a terminal and block buffered\n\
            otherwise.\n\
    -M MAGIC, --file-magic=MAGIC\n\
            Only files matching the signature pattern `MAGIC' are searched.\n\
            The signature magic bytes at the start of a file are compared to\n\
            the `MAGIC' regex pattern.  When matching, the file will be\n\
            searched.  This option may be repeated and may be combined with\n\
            options -O and -t to expand the search.  This option is relatively\n\
            slow as every file on the search path is read.\n\
    -m NUM, --max-count=NUM\n\
            Stop reading the input after NUM matches for each file processed.\n\
    --max-depth=NUM\n\
            Restrict recursive search to NUM (NUM > 0) directories deep, where\n\
            --max-depth=1 searches the specified path without visiting\n\
            sub-directories.  By comparison, -dskip skips all directories even\n\
            when they are on the command line.\n\
    --max-files=NUM\n\
            Restrict the number of files matched to NUM (NUM > 0).\n\
    -N, --only-line-number\n\
            The line number of the matching line in the file is output without\n\
            displaying the match.  The line number counter is reset for each\n\
            file processed.\n\
    -n, --line-number\n\
            Each output line is preceded by its relative line number in the\n\
            file, starting at line 1.  The line number counter is reset for\n\
            each file processed.\n\
    --no-group-separator\n\
            Removes the group separator line from the output for context\n\
            options -A, -B, and -C.\n\
    --no-hidden\n\
            Do not search hidden files and hidden directories.\n\
    -O EXTENSIONS, --file-extensions=EXTENSIONS\n\
            Search only files whose file name extensions match the specified\n\
            comma-separated list of file name EXTENSIONS.  This option is the\n\
            same as specifying --include='*.ext' for each extension name `ext'\n\
            in the EXTENSIONS list.  This option may be repeated and may be\n\
            combined with options -M and -t to expand the search.\n\
    -o, --only-matching\n\
            Prints only the matching part of lines and allows pattern matches\n\
            across newlines to span multiple lines.  Line numbers for\n\
            multi-line matches are displayed with option -n, using `|' as the\n\
            field separator for each additional line matched by the pattern.\n\
            This option cannot be combined with options -A, -B, -C, -v, and -y.\n\
    -P, --perl-regexp\n\
            Interpret PATTERN as a Perl regular expression.\n";
#ifndef HAVE_BOOST_REGEX
  std::cout << "\
            This feature is not available in this version of ugrep.\n";
#endif
  std::cout << "\
    -p, --no-dereference\n\
            If -R or -r is specified, no symbolic links are followed, even when\n\
            they are on the command line.\n\
    --pager[=COMMAND]\n\
            When output is sent to the terminal, uses `COMMAND' to page through\n\
            the output.  The default COMMAND is `less -R'.  This option makes\n\
            --color=auto behave as --color=always.  Enables --break.\n\
    -Q ENCODING, --encoding=ENCODING\n\
            The input file encoding.  The possible values of ENCODING can be:";
  for (int i = 0; format_table[i].format != NULL; ++i)
    std::cout << (i == 0 ? "" : ",") << (i % 6 ? " " : "\n            ") << "`" << format_table[i].format << "'";
  std::cout << "\n\
    -q, --quiet, --silent\n\
            Quiet mode: suppress normal output.  ugrep will only search until a\n\
            match has been found, making searches potentially less expensive.\n\
            Allows a pattern match to span multiple lines.\n\
    -R, --dereference-recursive\n\
            Recursively read all files under each directory.  Follow all\n\
            symbolic links, unlike -r.\n\
    -r, --recursive\n\
            Recursively read all files under each directory, following symbolic\n\
            links only if they are on the command line.\n\
    -S, --dereference\n\
            If -r is specified, all symbolic links are followed, like -R.  The\n\
            default is not to follow symbolic links.\n\
    -s, --no-messages\n\
            Silent mode.  Nonexistent and unreadable files are ignored (i.e.\n\
            their error messages are suppressed).\n\
    --separator=SEP\n\
            Use SEP as field separator between file name, line number, column\n\
            number, byte offset, and the matched line.  The default is a colon\n\
            (`:').\n\
    --stats\n\
            Display statistics on the number of files and directories searched.\n\
    -T, --initial-tab\n\
            Add a tab space to separate the file name, line number, column\n\
            number, and byte offset with the matched line.\n\
    -t TYPES, --file-type=TYPES\n\
            Search only files associated with TYPES, a comma-separated list of\n\
            file types.  Each file type corresponds to a set of file name\n\
            extensions passed to option -O.  For capitalized file types, the\n\
            search is expanded to include files found on the search path with\n\
            matching file signature magic bytes passed to option -M.  This\n\
            option may be repeated.  The possible values of TYPES can be\n\
            (use option -tlist to display a detailed list):";
  for (int i = 0; type_table[i].type != NULL; ++i)
    std::cout << (i == 0 ? "" : ",") << (i % 7 ? " " : "\n            ") << "`" << type_table[i].type << "'";
  std::cout << "\n\
    --tabs=NUM\n\
            Set the tab size to NUM to expand tabs for option -k.  The value of\n\
            NUM may be 1, 2, 4, or 8.\n\
    -U, --binary\n\
            Disables Unicode matching for binary file matching, forcing PATTERN\n\
            to match bytes, not Unicode characters.  For example, -U '\\xa3'\n\
            matches byte A3 (hex) instead of the Unicode code point U+00A3\n\
            represented by the two-byte UTF-8 sequence C2 A3.\n\
    -V, --version\n\
            Display version information and exit.\n\
    -v, --invert-match\n\
            Selected lines are those not matching any of the specified\n\
            patterns.\n\
    -W, --with-hex\n\
            Only output binary matches in hexadecimal, leaving text matches\n\
            alone.  This option is equivalent to the --binary-files=with-hex\n\
            option.\n\
    -w, --word-regexp\n\
            The PATTERN or -e PATTERN are searched for as a word (as if\n\
            surrounded by \\< and \\>).  This option does not apply to -f FILE\n\
            patterns.  To apply -w to patterns in FILE use -we `cat FILE`.\n\
    -X, --hex\n\
            Output matches in hexadecimal.  This option is equivalent to the\n\
            --binary-files=hex option.\n\
    -x, --line-regexp\n\
            Only input lines selected against the entire PATTERN or -e PATTERN\n\
            are considered to be matching lines (as if surrounded by ^ and $).\n\
            This option does not apply to -f FILE patterns.  To apply -x to\n\
            patterns in FILE use -xe `cat FILE`.\n\
    -Y, --empty\n\
            Permits empty matches, such as `^\\h*$' to match blank lines.  Empty\n\
            matches are disabled by default.  Note that empty-matching patterns\n\
            such as `x?' and `x*' match all input, not only lines with `x'.\n\
    -y, --any-line\n\
            Any matching or non-matching line is output.  Non-matching lines\n\
            are output with the `-' separator as context of the matching lines.\n\
            See also the -A, -B, and -C options.\n\
    -Z, --null\n\
            Prints a zero-byte after the file name.\n\
    -z, --decompress\n\
            Search zlib-compressed (.gz) files.  Option -Q is disabled.\n";
#ifndef HAVE_LIBZ
  std::cout << "\
            This feature is not available in this version of ugrep.\n";
#endif
  std::cout << "\
\n\
    The ugrep utility exits with one of the following values:\n\
\n\
    0       One or more lines were selected.\n\
    1       No lines were selected.\n\
    >1      An error occurred.\n\
\n\
    If -q or --quiet or --silent is used and a line is selected, the exit\n\
    status is 0 even if an error occurred.\n\
" << std::endl;
  }
  exit(EXIT_ERROR);
}

// display version info
void version()
{
  std::cout << "ugrep " UGREP_VERSION " " PLATFORM << std::endl;
  exit(EXIT_OK);
}
