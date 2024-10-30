# Autogenerated from ugrep --help
# Copy this file to ~/.config/fish/completions
complete -c ug -s A -l after-context -d 'Output NUM lines of trailing context after matching lines'
complete -c ug -s a -l text -d 'Process a binary file as if it were text'
complete -c ug -l all -s '@' -d 'Search all files except hidden: cancel previous file and directory search restrictions and cancel --ignore-binary and --ignore-files when specified'
complete -c ug -l and -d 'Specify additional PATTERN that must match'
complete -c ug -l andnot -d 'Combines --and --not'
complete -c ug -s B -l before-context -d 'Output NUM lines of leading context before matching lines'
complete -c ug -s b -l byte-offset -d 'The offset in bytes of a pattern match is displayed in front of the respective matched line'
complete -c ug -l binary-files -d 'Controls searching and reporting pattern matches in binary files'
complete -c ug -l bool -s '%' -o '%%' -d 'Specifies Boolean query patterns'
complete -c ug -l break -d 'Adds a line break between results from different files'
complete -c ug -s C -l context -d 'Output NUM lines of leading and trailing context surrounding each matching line'
complete -c ug -s c -l count -d 'Only a count of selected lines is written to standard output'
complete -c ug -l color -l colour -d 'Mark up the matching text with the colors specified with option --colors or the GREP_COLOR or GREP_COLORS environment variable'
complete -c ug -l colors -l colours -d 'Use COLORS to mark up text'
complete -c ug -l config -l - -d 'Use configuration FILE'
complete -c ug -l no-config -d 'Do not automatically load the default .ugrep configuration file.'
complete -c ug -l no-confirm -d 'Do not confirm actions in -Q query TUI'
complete -c ug -l cpp -d 'Output file matches in C++'
complete -c ug -l csv -d 'Output file matches in CSV'
complete -c ug -s D -l devices -xa 'read skip' -d 'If an input file is a device, FIFO or socket, use ACTION to process it'
complete -c ug -s d -l directories -xa 'read recurse skip' -d 'If an input file is a directory, use ACTION to process it'
complete -c ug -l delay -d 'Set the default -Q key response delay'
complete -c ug -l depth -s 1 -s 2 -s 3 -s 9 -o '10' -o '11' -d 'Restrict recursive searches from MIN to MAX directory levels deep, where -1 (--depth=1) searches the specified path without recursing into subdirectories'
complete -c ug -l dotall -d 'Dot . in regular expressions matches anything, including newline.'
complete -c ug -s E -l extended-regexp -d 'Interpret patterns as extended regular expressions (EREs)'
complete -c ug -s e -r -l regexp -d 'Specify a PATTERN to search the input'
complete -c ug -l encoding -xa 'binary ASCII UTF-8 UTF-16 UTF-16BE UTF-16LE UTF-32 UTF-32BE UTF-32LE LATIN1 ISO-8859-1 ISO-8859-2 ISO-8859-3 ISO-8859-4 ISO-8859-5 ISO-8859-6 ISO-8859-7 ISO-8859-8 ISO-8859-9 ISO-8859-10 ISO-8859-11 ISO-8859-13 ISO-8859-14 ISO-8859-15 ISO-8859-16 MAC MACROMAN EBCDIC CP437 CP850 CP858 CP1250 CP1251 CP1252 CP1253 CP1254 CP1255 CP1256 CP1257 CP1258 KOI8-R KOI8-U KOI8-RU ' -d 'The encoding format of the input'
complete -c ug -l exclude -d 'Exclude files whose name matches GLOB, same as -g ^GLOB'
complete -c ug -l exclude-dir -d 'Exclude directories whose name matches GLOB from recursive searches, same as -g ^GLOB/'
complete -c ug -l exclude-from -d 'Read the globs from FILE and skip files and directories whose name matches one or more globs'
complete -c ug -l exclude-fs -d 'Exclude file systems specified by MOUNTS from recursive searches'
complete -c ug -s F -l fixed-strings -d 'Interpret pattern as a set of fixed strings, separated by newlines, any of which is to be matched'
complete -c ug -s f -r -l file -d 'Read newline-separated patterns from FILE'
complete -c ug -l filter -d 'Filter files through the specified COMMANDS first before searching'
complete -c ug -l filter-magic-label -d 'Associate LABEL with files whose signature "magic bytes" match the MAGIC regex pattern'
complete -c ug -l format -d 'Output FORMAT-formatted matches'
complete -c ug -l free-space -d 'Spacing (blanks and tabs) in regular expressions are ignored'
complete -c ug -s G -l basic-regexp -d 'Interpret patterns as basic regular expressions (BREs)'
complete -c ug -s g -r -l glob -l iglob -d 'Only search files whose name matches the specified comma-separated list of GLOBS, same as --include=glob for each glob in GLOBS'
complete -c ug -l glob-ignore-case -d 'Perform case-insensitive glob matching in general'
complete -c ug -l group-separator -d 'Use SEP as a group separator for context options -A, -B and -C'
complete -c ug -l no-group-separator -d 'Removes the group separator line from the output for context options -A, -B and -C'
complete -c ug -s H -l with-filename -d 'Always print the filename with output lines'
complete -c ug -s h -l no-filename -d 'Never print filenames with output lines'
complete -c ug -l heading -s '+' -d 'Group matches per file'
complete -c ug -l help -s '?' -d 'Display a help message on options related to WHAT when specified'
complete -c ug -l hexdump -xa '1a 2a 4ah 6ah 8ah 1aC1 2aC1 4ahC1 6ahC1 8ahC1' -d 'Output matches in 1 to 8 columns of 8 hexadecimal octets'
complete -c ug -l hidden -s '.' -d 'Search hidden files and directories'
complete -c ug -l hyperlink -d 'Hyperlinks are enabled for file names when colors are enabled'
complete -c ug -s I -l ignore-binary -d 'Ignore matches in binary files'
complete -c ug -s i -l ignore-case -d 'Perform case insensitive matching'
complete -c ug -l ignore-files -d 'Ignore files and directories matching the globs in each FILE that is encountered in recursive searches'
complete -c ug -l no-ignore-files -d 'Do not ignore files, i.e. cancel --ignore-files when specified.'
complete -c ug -l include -d 'Only search files whose name matches GLOB, same as -g GLOB'
complete -c ug -l include-dir -d 'Only directories whose name matches GLOB are included in recursive searches, same as -g GLOB/'
complete -c ug -l include-from -d 'Read the globs from FILE and search only files and directories whose name matches one or more globs'
complete -c ug -l include-fs -d 'Only file systems specified by MOUNTS are included in recursive searches'
complete -c ug -l index -d 'Perform fast index-based recursive search'
complete -c ug -s J -r -l jobs -d 'Specifies the number of threads spawned to search files'
complete -c ug -s j -l smart-case -d 'Perform case insensitive matching, unless a pattern is specified with a literal upper case ASCII letter'
complete -c ug -l json -d 'Output file matches in JSON'
complete -c ug -s K -r -l range -l min-line -l max-line -d 'Start searching at line MIN, stop at line MAX when specified'
complete -c ug -s k -l column-number -d 'The column number of a pattern match is displayed in front of the respective matched line, starting at column 1'
complete -c ug -s L -l files-without-match -d 'Only the names of files not containing selected lines are written to standard output'
complete -c ug -s l -l files-with-matches -d 'Only the names of files containing selected lines are written to standard output'
complete -c ug -l label -d 'Displays the LABEL value when input is read from standard input where a file name would normally be printed in the output'
complete -c ug -l line-buffered -d 'Force output to be line buffered instead of block buffered'
complete -c ug -l lines -d 'Boolean line matching mode for option --bool, the default mode'
complete -c ug -s M -r -l file-magic -d 'Only search files matching the magic signature pattern MAGIC'
complete -c ug -s m -r -l min-count -l max-count -d 'Require MIN matches, stop after MAX matches when specified'
complete -c ug -l match -d 'Match all input'
complete -c ug -l max-files -d 'Restrict the number of files matched to NUM'
complete -c ug -l mmap -d 'Use memory maps to search files'
complete -c ug -s N -r -l neg-regexp -d 'Specify a negative PATTERN to reject specific -e PATTERN matches with a counter pattern'
complete -c ug -s n -l line-number -d 'Each output line is preceded by its relative line number in the file, starting at line 1'
complete -c ug -l not -d 'Specifies that PATTERN should not match'
complete -c ug -s O -r -l file-extension -d 'Only search files whose filename extensions match the specified comma-separated list of EXTENSIONS, same as -g *.ext for each'
complete -c ug -s o -l only-matching -d 'Only the matching part of a pattern match is output'
complete -c ug -l only-line-number -d 'Only the line number of a matching line is output'
complete -c ug -l files -o '%%' -d 'Boolean file matching mode, the opposite of --lines'
complete -c ug -s P -l perl-regexp -d 'Interpret PATTERN as a Perl regular expression using PCRE2'
complete -c ug -s p -l no-dereference -d 'If -R or -r is specified, do not follow symbolic links, even when symbolic links are specified on the command line'
complete -c ug -l pager -d 'When output is sent to the terminal, uses COMMAND to page through the output'
complete -c ug -l pretty -d 'When output is sent to a terminal, enables --color, --heading, -n, --sort, --tree and -T when not explicitly disabled'
complete -c ug -s Q -l query -d 'Query mode: start a TUI to perform interactive searches'
complete -c ug -s q -l quiet -l silent -d 'Quiet mode: suppress all output'
complete -c ug -s R -l dereference-recursive -d 'Recursively read all files under each directory, following symbolic links to files and directories, unlike -r'
complete -c ug -s r -l recursive -d 'Recursively read all files under each directory, following symbolic links only if they are on the command line'
complete -c ug -l replace -d 'Replace matching patterns in the output by FORMAT with % fields'
complete -c ug -s S -l dereference-files -d 'When -r is specified, follow symbolic links to files, but not to directories'
complete -c ug -s s -l no-messages -d 'Silent mode: nonexistent and unreadable files are ignored and their error messages and warnings are suppressed'
complete -c ug -l save-config -d 'Save configuration FILE to include OPTIONS'
complete -c ug -l separator -l context-separator -d 'Use SEP as field separator between file name, line number, column number, byte offset and the matched line'
complete -c ug -l split -d 'Split the -Q query TUI screen on startup'
complete -c ug -l sort -xa 'name best size changed created used rname rbest rsize rchanged rcreated rused' -d 'Displays matching files in the order specified by KEY in recursive searches'
complete -c ug -l stats -d 'Output statistics on the number of files and directories searched and the inclusion and exclusion constraints applied'
complete -c ug -s T -l initial-tab -d 'Add a tab space to separate the file name, line number, column number and byte offset with the matched line'
complete -c ug -s t -l file-type -xa 'actionscript ada asm asp aspx autoconf automake awk Awk basic batch bison c c++ clojure cpp csharp css csv dart Dart delphi elisp elixir erlang fortran gif Gif go groovy gsp haskell html jade java jpeg Jpeg js json jsp julia kotlin less lex lisp lua m4 make markdown matlab node Node objc objc++ ocaml parrot pascal pdf Pdf perl Perl php Php png Png prolog python Python r rpm Rpm rst rtf Rtf ruby Ruby rust scala scheme shell Shell smalltalk sql svg swift tcl tex text tiff Tiff tt typescript verilog vhdl vim xml Xml yacc yaml zig ' -d 'Search only files associated with TYPES, a comma-separated list of file types'
complete -c ug -l tabs -d 'Set the tab size to NUM to expand tabs for option -k'
complete -c ug -l tag -d 'Disables colors to mark up matches with TAG'
complete -c ug -l tree -s '^' -d 'Output directories with matching files in a tree-like format for option -c or --count, -l or --files-with-matches, -L or --files-without-match'
complete -c ug -s U -l ascii -l binary -d 'Disables Unicode matching for ASCII and binary matching'
complete -c ug -s u -l ungroup -d 'Do not group multiple pattern matches on the same matched line'
complete -c ug -s V -l version -d 'Display version with linked libraries and exit'
complete -c ug -s v -l invert-match -d 'Selected lines are those not matching any of the specified patterns'
complete -c ug -l view -d 'Use COMMAND to view/edit a file in -Q query TUI by pressing CTRL-Y'
complete -c ug -s W -l with-hex -d 'Output binary matches in hexadecimal, leaving text matches alone'
complete -c ug -s w -l word-regexp -d 'The PATTERN is searched for as a word, such that the matching text is preceded by a non-word character and is followed by a non-word character'
complete -c ug -l width -d 'Truncate the output to NUM visible characters per line'
complete -c ug -s X -l hex -d 'Output matches and matching lines in hexadecimal'
complete -c ug -s x -l line-regexp -d 'Select only those matches that exactly match the whole line, as if the patterns are surrounded by ^ and $'
complete -c ug -l xml -d 'Output file matches in XML'
complete -c ug -s Y -l empty -d 'Permits empty matches'
complete -c ug -s y -l any-line -l passthru -d 'Any line is output (passthru)'
complete -c ug -s Z -l fuzzy -xa '1 +1 -1 ~1 +-1 +~1 +-~1 -~1 best1 best+1 best-1 best~1 best+-1 best+~1 best+-~1 best-~1' -d 'Fuzzy mode: report approximate pattern matches within MAX errors'
complete -c ug -s z -l decompress -d 'Search compressed files and archives'
complete -c ug -l zmax -d 'When used with option -z (--decompress), searches the contents of compressed files and archives stored within archives by up to NUM expansion stages'
complete -c ug -s 0 -l null -d 'Output a zero-byte (NUL) after the file name'
