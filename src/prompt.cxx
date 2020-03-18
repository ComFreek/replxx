#ifdef _WIN32

#include <conio.h>
#include <windows.h>
#include <io.h>
#if _MSC_VER < 1900
//#define snprintf _snprintf	// Microsoft headers use underscores in some names
// Commented out, otherwise it would produce the following errors with GCC
/*

[ 52%] Building CXX object CMakeFiles/replxx.dir/extern/replxx/src/prompt.cxx.obj
In file included from C:/msys64/mingw64/x86_64-w64-mingw32/include/locale.h:12,
                 from C:/msys64/mingw64/include/c++/9.2.0/clocale:42,
                 from C:/msys64/mingw64/include/c++/9.2.0/x86_64-w64-mingw32/bits/c++locale.h:41,
                 from C:/msys64/mingw64/include/c++/9.2.0/bits/localefwd.h:40,
                 from C:/msys64/mingw64/include/c++/9.2.0/string:43,
                 from P:/curv/extern/replxx/src/ConvertUTF.h:97,
                 from P:/curv/extern/replxx/src/conversion.hxx:4,
                 from P:/curv/extern/replxx/src/utfstring.hxx:6,
                 from P:/curv/extern/replxx/src/prompt.hxx:6,
                 from P:/curv/extern/replxx/src/prompt.cxx:20:
C:/msys64/mingw64/x86_64-w64-mingw32/include/stdio.h:780:23: error: conflicting declaration of 'int _snprintf(char*, size_t, const char*, ...)' with 'C' linkage
  780 |   _CRTIMP int __cdecl _snprintf(char * __restrict__ _Dest,size_t _Count,const char * __restrict__ _Format,...) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
      |                       ^~~~~~~~~
P:/curv/extern/replxx/src/prompt.cxx:7:18: note: previous declaration with 'C++' linkage
    7 | #define snprintf _snprintf // Microsoft headers use underscores in some names
      |                  ^~~~~~~~~
In file included from C:/msys64/mingw64/include/c++/9.2.0/ext/string_conversions.h:43,
                 from C:/msys64/mingw64/include/c++/9.2.0/bits/basic_string.h:6493,
                 from C:/msys64/mingw64/include/c++/9.2.0/string:55,
                 from P:/curv/extern/replxx/src/ConvertUTF.h:97,
                 from P:/curv/extern/replxx/src/conversion.hxx:4,
                 from P:/curv/extern/replxx/src/utfstring.hxx:6,
                 from P:/curv/extern/replxx/src/prompt.hxx:6,
                 from P:/curv/extern/replxx/src/prompt.cxx:20:
C:/msys64/mingw64/include/c++/9.2.0/cstdio:175:11: error: '::snprintf' has not been declared
  175 |   using ::snprintf;
      |           ^~~~~~~~
C:/msys64/mingw64/include/c++/9.2.0/cstdio:185:22: error: '__gnu_cxx::snprintf' has not been declared
  185 |   using ::__gnu_cxx::snprintf;
      |                      ^~~~~~~~
make[3]: *** [CMakeFiles/replxx.dir/build.make:141: CMakeFiles/replxx.dir/extern/replxx/src/prompt.cxx.obj] Fehler 1
make[3]: Verzeichnis „/p/curv/release“ wird verlassen
make[2]: *** [CMakeFiles/Makefile2:401: CMakeFiles/replxx.dir/all] Fehler 2
make[2]: Verzeichnis „/p/curv/release“ wird verlassen
make[1]: *** [Makefile:130: all] Fehler 2
make[1]: Verzeichnis „/p/curv/release“ wird verlassen
make: *** [Makefile:5: release] Fehler 2

*/
#endif
#define strcasecmp _stricmp
#define strdup _strdup
#define write _write
#define STDIN_FILENO 0

#else /* _WIN32 */

#include <unistd.h>

#endif /* _WIN32 */

#include "prompt.hxx"
#include "util.hxx"
#include "io.hxx"

namespace replxx {

bool PromptBase::write() {
	if (write32(1, promptText.get(), promptBytes) == -1) return false;

	return true;
}

PromptInfo::PromptInfo(std::string const& text_, int columns) {
	promptExtraLines = 0;
	promptLastLinePosition = 0;
	promptPreviousLen = 0;
	promptScreenColumns = columns;
	Utf32String tempUnicode(text_.c_str());

	// strip control characters from the prompt -- we do allow newline
	char32_t* pIn = tempUnicode.get();
	char32_t* pOut = pIn;

	int len = 0;
	int x = 0;

	bool const strip = !tty::out;

	while (*pIn) {
		char32_t c = *pIn;
		if ('\n' == c || !isControlChar(c)) {
			*pOut = c;
			++pOut;
			++pIn;
			++len;
			if ('\n' == c || ++x >= promptScreenColumns) {
				x = 0;
				++promptExtraLines;
				promptLastLinePosition = len;
			}
		} else if (c == '\x1b') {
			if (strip) {
				// jump over control chars
				++pIn;
				if (*pIn == '[') {
					++pIn;
					while (*pIn && ((*pIn == ';') || ((*pIn >= '0' && *pIn <= '9')))) {
						++pIn;
					}
					if (*pIn == 'm') {
						++pIn;
					}
				}
			} else {
				// copy control chars
				*pOut = *pIn;
				++pOut;
				++pIn;
				if (*pIn == '[') {
					*pOut = *pIn;
					++pOut;
					++pIn;
					while (*pIn && ((*pIn == ';') || ((*pIn >= '0' && *pIn <= '9')))) {
						*pOut = *pIn;
						++pOut;
						++pIn;
					}
					if (*pIn == 'm') {
						*pOut = *pIn;
						++pOut;
						++pIn;
					}
				}
			}
		} else {
			++pIn;
		}
	}
	*pOut = 0;
	promptChars = len;
	promptBytes = static_cast<int>(pOut - tempUnicode.get());
	promptText = tempUnicode;

	promptIndentation = len - promptLastLinePosition;
	promptCursorRowOffset = promptExtraLines;
}

// Used with DynamicPrompt (history search)
//
const Utf32String forwardSearchBasePrompt("(i-search)`");
const Utf32String reverseSearchBasePrompt("(reverse-i-search)`");
const Utf32String endSearchBasePrompt("': ");
Utf32String previousSearchText;	// remembered across invocations of replxx_input()

DynamicPrompt::DynamicPrompt(PromptBase& pi, int initialDirection)
		: searchTextLen(0), direction(initialDirection) {
	promptScreenColumns = pi.promptScreenColumns;
	promptCursorRowOffset = 0;
	Utf32String emptyString(1);
	searchText = emptyString;
	const Utf32String* basePrompt =
			(direction > 0) ? &forwardSearchBasePrompt : &reverseSearchBasePrompt;
	size_t promptStartLength = basePrompt->length();
	promptChars =
			static_cast<int>(promptStartLength + endSearchBasePrompt.length());
	promptBytes = promptChars;
	promptLastLinePosition = promptChars;	// TODO fix this, we are asssuming
																				 // that the history prompt won't wrap
																				 // (!)
	promptPreviousLen = promptChars;
	Utf32String tempUnicode(promptChars + 1);
	memcpy(tempUnicode.get(), basePrompt->get(),
				 sizeof(char32_t) * promptStartLength);
	memcpy(&tempUnicode[promptStartLength], endSearchBasePrompt.get(),
				 sizeof(char32_t) * (endSearchBasePrompt.length() + 1));
	tempUnicode.initFromBuffer();
	promptText = tempUnicode;
	calculateScreenPosition(0, 0, pi.promptScreenColumns, promptChars,
													promptIndentation, promptExtraLines);
}

void DynamicPrompt::updateSearchPrompt(void) {
	const Utf32String* basePrompt =
			(direction > 0) ? &forwardSearchBasePrompt : &reverseSearchBasePrompt;
	size_t promptStartLength = basePrompt->length();
	promptChars = static_cast<int>(promptStartLength + searchTextLen +
																 endSearchBasePrompt.length());
	promptBytes = promptChars;
	Utf32String tempUnicode(promptChars + 1);
	memcpy(tempUnicode.get(), basePrompt->get(),
				 sizeof(char32_t) * promptStartLength);
	memcpy(&tempUnicode[promptStartLength], searchText.get(),
				 sizeof(char32_t) * searchTextLen);
	size_t endIndex = promptStartLength + searchTextLen;
	memcpy(&tempUnicode[endIndex], endSearchBasePrompt.get(),
				 sizeof(char32_t) * (endSearchBasePrompt.length() + 1));
	tempUnicode.initFromBuffer();
	promptText = tempUnicode;
}

void DynamicPrompt::updateSearchText(const char32_t* text_) {
	Utf32String tempUnicode(text_);
	searchTextLen = static_cast<int>(tempUnicode.chars());
	searchText = tempUnicode;
	updateSearchPrompt();
}

}

