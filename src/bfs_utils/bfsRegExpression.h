#ifndef BFS_REGEXP_INCLUDED
#define BFS_REGEXP_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsRegExpression.h
//  Description   : This is the class describing the regular expression matcher
//                  class used for processing of textual data.
//
//  Author  : Patrick McDaniel
//  Created : Wed May 19 07:06:16 EDT 2021
//

// Includes
#include <stdint.h>
#include <bfs_log.h>
#include <bfsUtilLayer.h>

// STL-isms
#include <string>
#include <vector>
using namespace std;

// Project Includes

//
// Class definitions
// Unite test defines
#define REGEXP_UTEST_ITERATIONS 1000
#define REGEXP_UTEST_RE_SIZE 20
#define UTEST_STRING_MAXREPS 5

// Class types

// Atom types
typedef enum {
	RE_LITERAL = 0, // Literal atom
	RE_GROUP   = 1, /// Group of atoms
	RE_ORLIST  = 2, // List of OR atoms
} re_atomtype_t;

// Repetition modifier types
typedef enum {
	RE_NOMODIFER  = 0,  // None
    RE_ZEROORMORE = 1,  // '*'
    RE_ONEORMORE  = 2,  // '+'
    RE_ZEROORONE  = 3,  // '?'
} re_atommodifier_t;

// Literal types
typedef enum {
	RE_NORMAL_LITERAL = 0, // A normal literal, e.g., "a"
	RE_ANY_LITERAL    = 1, // A literal can be anything "."
	RE_WORD_LITERAL   = 2, // A word literal "\w", [a-zA-Z0-9_]
	RE_SPACE_LITERAL  = 3, // A space literal "\s", [ \t\n\r\f\v]
	RE_DIGIT_LITERAL  = 4, // A digit literal "\d", [0-9]
	RE_NIL_LITERAL    = 5, // A NIL transitions (no consume char)
	RE_OR_LITERAL     = 6, // An OR literal [...]
	RE_MAX_LITERAL    = 7, // Guard value
} re_littype_t; 

// Literals, lists of literals
typedef struct {
	re_littype_t        ltype;    // The type of literal
    char                ch;       // The literal itself
} re_literal_t;
typedef vector<re_literal_t> re_literals_t;

// Parsed atoms, lists of atoms
typedef struct atom {
	re_atomtype_t       atype;    // The atom type
    bool                invert;   // Literal inversion
	re_literal_t        literal;  // The literal to use
	re_literals_t       orlist;   // The list of chars in the OR
    re_atommodifier_t   modifier; // Any repetition modifier
    vector<struct atom> group;    // Sub-grouping of atoms "(...)"
} atom_t;
typedef vector<atom_t> atoms_t;

// States and transitions, lists of transitions
typedef size_t state_t;  // The state (number)
typedef vector<state_t> states_t; // A list of states
typedef struct {
	state_t       fromstate;  // The state exiting from
	state_t       tostate;    // The state transitioning to
	re_literal_t  literal;    // The literal to use
	re_literals_t orlist;     // The list of chars in the OR
	bool          invert;     // Invert the OR list 
} re_transition_t;
typedef vector<re_transition_t> state_machine_t;

//
// Class Definition

class bfsRegExpression {

public:

	//
	// Static methods

	bfsRegExpression( void );
	  // Default constructor

    bfsRegExpression( string exp );
      // Attribute constructor

    virtual ~bfsRegExpression( void );
      // Default destructor

    //
    // Access Methods

	// The regular expression 
	void setExpression( string re ) {
		regexp = re;
		smachine.clear();
		states = 1;
		createRegexpStateMachine();
		last_state = generateStateMachine( machine.group, 0 );
		logMessage( UTIL_VRBLOG_LEVEL, "Last state : s%d", last_state );
	}

	//
	// Class Methods

	// Match a string against the regular expression
	bool match( string matchstr ) {
	    states_t newnils;
		return( match(0, matchstr, 0, 0, newnils) );
	}

    //
    // Static Class Methods

	static bool unitTest( void );
	  // Do the Unit test for this class

	static string generateRegExpression( uint8_t atms );
	  // generate a random regular expression (for testing)

	static string generateRegexpString( atom_t & atm );
	  // generate a random regular expression (for testing)

	static char generateLiteral( re_literal_t & lit );
	  // Generate a random literal from definition

private:

	//
    // Private class methods

	bool createRegexpStateMachine( void );
	  // Generate the state machine associated with the reg exp

	size_t parseExpression( atoms_t & atomlist, size_t pos );
	  // Parse the string and create the atom structure

	state_t generateStateMachine( atoms_t & atms, state_t from );
	  // Recursive state machine construction

	bool match( state_t state, string matchstr, size_t pos, size_t depth, states_t & nils );
	  // Recursive match function over the string (NFA traversal)

	// Private static methods

	static bool nextLiteral( char ch, bool escaped, re_literal_t & lit );
	  // Process the next literal

	static bool matchLiteral( re_literal_t & lit, char ch );
	  // Check a specific character for a match

	// 
	// Class Data

	string regexp;
	  // The regular expression to match

	struct atom machine;
	  // The state machine associated with this regular expression

    size_t states, last_state;
	  // State variables when matching

    state_machine_t smachine;
	  // The set of transitions defining the NFA

	//
	// Static Member Data

	static const string re_special_chars;
	  // The regular expression special characters

	static const string literalStrings[RE_MAX_LITERAL];
	  // Descriptive strgins for the literal types

	static const string re_Printables;
	static const string re_WordChars;
	static const string re_SpaceChars;
	static const string re_DigitChars;
	  // Strings containting the characters of different matching classes

};

#endif
