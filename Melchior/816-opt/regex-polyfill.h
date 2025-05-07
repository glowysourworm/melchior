#pragma once

#include "helpers.h"

/*

    The POSIX version of regex (regex.h) is not setup to be portable. These functions will
    substitute in the std::regex version; but the rest of the code in "helpers.c" didn't like
    the std version in the namespace. So, this will remove the namespace clash from "helpers.c".

*/

//#include <regex>        // The culprit library!
//#include <string>       // .. and
//#include <malloc.h>     // .. several
//#include <cstring>      // .. more

// The base libraries were giving further errors when C++ compiler was used.

#include "../libregex/regex.h"
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>



