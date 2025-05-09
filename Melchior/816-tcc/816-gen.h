#pragma once

/*

	[glowysourworm] The purpose of this project is to encapsulate only the 816-tcc functionality.
					There are a tremendous number of compiler related issues trying to preserve
					all of the tinycc functionality. 

					This code file is an attempt to gather any global variables and provide more
					structure to this solution to try and eliminate all other extraneous code
					so that a simple assembler compiler may be produced.

					Suggestion:  Lets try and utilize an "empty" C compiler: What does a compiler
								 do?

					Stages:  Lexical Analysis (Lexor)
							 Syntax (Parsing)
							 Semantics 
							 Code Generation
							 Code Optimization
							 Final Code Generation

					The goal of a C compiler project would be to inject assembler rules for semantics
					and code generation. The basic syntax should be settled 1000 years ago! 

					Possibility:  We could try LLVM to produce the compiler tool chain; but it may 
								  not be entirely required.

								  Also, there's no reason to use unmanaged code either. Java, or portable
								  C# would be more ideal for fast development. 

								  The entire project should be SMALL. Less than 20,000 lines probably!

*/


// ***** TCC 816 (mostly moved from the top of 816-gen.c)
//
// These are the "header" of that file, which will be encapsulated here 
// probably as a struct (for these global variables). This will point the
// rest of the dependent code base at this header. So, this code base is
// no longer going to properly target other platforms.
//

#include <cstddef>

#define LDOUBLE_SIZE 12 // not actually supported
#define LDOUBLE_ALIGN 4
#define MAX_ALIGN 8

#define NB_REGS 15

#define RC_INT 0x0001
#define RC_FLOAT 0x0002
#define RC_R0 0x0004
#define RC_R1 0x0008
#define RC_R2 0x0010
#define RC_R3 0x0020
#define RC_R4 0x0040
#define RC_R5 0x0080
#define RC_R9 0x0100
#define RC_R10 0x0200
#define RC_F0 0x0400
#define RC_F1 0x0800
#define RC_F2 0x1000
#define RC_F3 0x2000
#define RC_NONE 0x8000

#define RC_IRET RC_R0
#define RC_LRET RC_R1
#define RC_FRET RC_F0

enum {
    TREG_R0 = 0,
    TREG_R1,
    TREG_R2,
    TREG_R3,
    TREG_R4,
    TREG_R5,
    TREG_R9 = 9,
    TREG_R10,
    TREG_F0,
    TREG_F1,
    TREG_F2,
    TREG_F3,
};

int reg_classes[NB_REGS] = {
    RC_INT | RC_R0,
    RC_INT | RC_R1,
    RC_INT | RC_R2,
    RC_INT | RC_R3,
    RC_INT | RC_R4,
    RC_INT | RC_R5,
    RC_NONE,
    RC_NONE,
    RC_NONE,
    RC_R9,
    RC_R10,
    RC_FLOAT | RC_F0,
    RC_FLOAT | RC_F1,
    RC_FLOAT | RC_F2,
    RC_FLOAT | RC_F3,
};

#define REG_IRET TREG_R0
#define REG_LRET TREG_R1
#define REG_FRET TREG_F0

#define R_DATA_32 1  // whatever
#define R_DATA_PTR 1 // whatever
#define R_JMP_SLOT 2 // whatever
#define R_COPY 3     // whatever

#define ELF_PAGE_SIZE 0x1000 // whatever
#define ELF_START_ADDR 0x400 // made up

#define PTR_SIZE 4

#define EM_TCC_TARGET EM_W65

#define LOCAL_LABEL "__local_%d"

#define MAXLEN 512

#define MAX_LABELS 1000

char unique_token[] = "{WLA_FILENAME}";

/**
 * @note WLA does not have file-local symbols, only section-local and global.
 * Thus, everything that is file-local must be made global and given a
 * unique name. Not knowing how to choose one deterministically, we use a
 * random string, saved to STATIC_PREFIX.
 * With WLA 9.X, if you have a label that begins with a _ and it is inside a section,
 * then the name doesn't show outside of that section.
 * If it is not inside a section it doesn't show outside of the object file...
 */

#define STATIC_PREFIX "tccs_"
char current_fn[MAXLEN] = "";

// Variable relocate a given section
char **relocptrs = NULL;

char *label_workaround = NULL;

/**
 * @struct labels_816
 *
 * @brief Structure representing labels in the code.
 *
 * This structure represents a label in the code with a name and a position.
 */
struct labels_816
{
    char *name; /**< @brief The name of the label. */
    int pos;    /**< @brief The position of the label in the code. */
};

struct labels_816 label[MAX_LABELS]; /**< @brief Array to store multiple label structures. */

int labels = 0;