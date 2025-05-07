Polyfill for regex
------------------

Github:  https://github.com/strlcat/libregex

This version is going to function as a portable regex library. The 816-opt project had too many
regex.h references to deal with std::regex!  

So, this will do until code gets updated. Most of these libs could be moved to managed code with
just a little work; but the details of the ASM tools might take tedious hours to fix!

Thanks to strlcat for your work on libregex!

NOTE*** I had to make some minor changes to get things working:

1) Removed reference to "features.h". I tried looking this up to include from GNU libraries; but it
   is too lengthly of a list of dependencies to import. I didn't see any compiler issues with it - so
   it must've been unused. There were, however, type substitutes to stdlib C types. They seem mostly
   innocuous.

2) Removed "restricted" keyword from function calls. This might not be an issue. Tests will tel very
   shortly.