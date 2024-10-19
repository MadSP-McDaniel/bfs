
////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsRegExpression.cpp
//  Description   : This file contains the implementation of the regular
//                  expression matcher for the BFS utilities.
//
//  Author        : Patrick McDaniel
//  Created       : Wed May 19 07:06:16 EDT 2021
//

// Include files
#include <stack>
#include <algorithm>

// Project include files
#include <bfs_log.h>
#include <bfs_util.h>
#include <bfsUtilLayer.h>
#include <bfsRegExpression.h>
#include <bfsRegExpressionError.h>

// Helper defines
#define ADD_LITERAL_TRANSITION(mach,from,to,atm) \
	transition.fromstate = from; \
	transition.tostate = to; \
	transition.literal = atm->literal; \
	transition.orlist = atm->orlist; \
	transition.invert = atm->invert; \
	if ( atm->atype == RE_ORLIST ) { \
		logMessage( UTIL_VRBLOG_LEVEL, "Adding transition : s%d -> s%d OR invert=%s [%d elements]]  ", \
			from, to, (atm->invert == true) ? "true" : "false", atm->orlist.size() ); \
	} else {  \
		logMessage( UTIL_VRBLOG_LEVEL, "Adding transition : s%d -> s%d literal : %c", from, to, atm->literal.ch ); \
	} \
	mach.push_back( transition );
#define ADD_NIL_TRANSITION(mach,from,to) \
	transition.fromstate = from; \
	transition.tostate = to; \
    transition.literal.ltype = RE_NIL_LITERAL; \
	logMessage( UTIL_VRBLOG_LEVEL, "Adding NIL transition : s%d -> s%d", from, to ); \
	mach.push_back( transition );

// Static data
const string bfsRegExpression::re_special_chars = ".*+?^[]()";
const string bfsRegExpression::literalStrings[RE_MAX_LITERAL] = {
    "RE_NORMAL_LITERAL", "RE_ANY_LITERAL", "RE_WORD_LITERAL", "RE_SPACE_LITERAL", 
    "RE_DIGIT_LITERAL", "RE_NIL_LITERAL", "RE_OR_LITERAL"
};
const string bfsRegExpression::re_Printables = "<!\\“#$%&\\‘()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[|]^_\\`abcdefghijklmnopqrstuvwxyz{}~>";
const string bfsRegExpression::re_WordChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
const string bfsRegExpression::re_SpaceChars = " \t\n\r\f\v";
const string bfsRegExpression::re_DigitChars = "0123456789";

//
// Class methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::bfsRegExpression
// Description  : Base constructor
//
// Inputs       : none
// Outputs      : none

bfsRegExpression::bfsRegExpression( void ) {
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::bfsRegExpression
// Description  : Attribute constructor
//
// Inputs       : exp - the regular expression to parse/use
// Outputs      : none

bfsRegExpression::bfsRegExpression( string exp ) {
    setExpression( exp );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::~bfsRegExpression
// Description  : Base deconstructor
//
// Inputs       : none
// Outputs      : none

bfsRegExpression::~bfsRegExpression( void ) {
    // TODO : clean up the storage
}

//
// Private methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::createRegexpStateMachine
// Description  : Generate the state machine associated with the reg exp
//
// Inputs       : none
// Outputs      : true if successful, throws exception on error

bool bfsRegExpression::createRegexpStateMachine( void ) {

    // Setup/reset state machine
    machine.atype = RE_GROUP;
    machine.invert = false;
    machine.literal.ltype = RE_NORMAL_LITERAL;
    machine.literal.ch = 0x0;
    machine.modifier = RE_NOMODIFER;
    machine.group.clear();
    machine.orlist.clear();

    // Parse out the state machine
    parseExpression( machine.group, 0 );
 
    // Return succesfuly
    return( true );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::parseExpression
// Description  : Parse the string and create the atom structure
//
// Inputs       : atomlist - place to put the atoms created
//                pos - the current position in the string
// Outputs      : true if successful, throws exception on error

size_t bfsRegExpression::parseExpression( atoms_t & atomlist, size_t pos ) {

    // Local variables
    atom_t newatom;
    re_literal_t newlit;
    string message;
    bool done, escaped;

    // Walk the expression 
    while ( pos < regexp.length() ) {

        // Create the new atom
        newatom.atype = RE_LITERAL;
        newatom.invert = false;
        newatom.literal.ltype = RE_NORMAL_LITERAL;
        newatom.literal.ch = 0x0;
        newatom.modifier = RE_NOMODIFER;
        newatom.group.clear();
        newatom.orlist.clear();

        // Check to see if this is a compound (group or or list)
        if ( regexp[pos] == '(' ) {
            newatom.atype = RE_GROUP;
            pos = parseExpression( newatom.group, pos+1 );
            if ( regexp[pos] != ')' ) {
                message = "Syntax error in regular expression at position " + to_string(pos);
                throw new bfsRegExpressionError( message );
            }
            pos ++;
        } else if ( regexp[pos] == ')' ) {
            return( pos );
        } else if ( regexp[pos] == '[' ) {
            // check to see if the list is inverted
            pos ++;
            if ( regexp[pos] == '^' ) {
                newatom.invert = true;
                pos ++;
            }
            newatom.atype = RE_ORLIST;
            newatom.literal.ltype = RE_OR_LITERAL;
            done = false;
            while ( (pos < regexp.length()) && (!done) ) {
                if ( regexp[pos] == ']' ) {
                    done = true;
                    pos ++;
                } else {
                    if ( regexp[pos] == '\\' ) {
                        escaped = true;
                        pos++;
                    } else {
                        escaped = false;
                    }
                    if ( pos >= regexp.length() ) {
                        message = "Syntax error, missing literal at position " + to_string(pos);
                        throw new bfsRegExpressionError( message );
                    }
                    nextLiteral( regexp[pos++], escaped, newlit );
                    newatom.orlist.push_back( newlit );
                }
            }
            if ( !done ) {
                message = "Syntax error in regular expression at position " + to_string(pos);
                throw new bfsRegExpressionError( message );
            }
        } else if ( regexp[pos] == ']' ) {
            message = "Syntax error in regular expression at position, unexpected or close : " + to_string(pos);
            throw new bfsRegExpressionError( message );
        } else {
            // Check for escaped literal
            if ( regexp[pos] == '\\' ) {
                escaped = true;
                pos++;
            } else {
                escaped = false;
            }
            if ( pos >= regexp.length() ) {
                message = "Syntax error, missing literal at position " + to_string(pos);
                throw new bfsRegExpressionError( message );
            }
            nextLiteral( regexp[pos++], escaped, newatom.literal );
        }

        // Look for modifier
        if ( regexp[pos] == '*' ) {
            newatom.modifier = RE_ZEROORMORE;
            pos++;
        } else if ( regexp[pos] == '+' ) {
            newatom.modifier = RE_ONEORMORE;
            pos++;
        } else if ( regexp[pos] == '?' ) {
            newatom.modifier = RE_ZEROORONE;
            pos++;
        } 

        // Push the atom onto the list of atoms
        logMessage( UTIL_VRBLOG_LEVEL, "Atom: %d,%s,%c,%s,%d (%d)[%d]", newatom.atype, literalStrings[newatom.literal.ltype].c_str(),
            (newatom.literal.ch == 0x0) ? '0' : newatom.literal.ch, (newatom.invert) ?  "true" : "false", 
            newatom.modifier, newatom.group.size(), newatom.orlist.size() );
        atomlist.push_back( newatom );
    }

    // Return the end of the position    
    return( pos );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::generateStateMachine
// Description  : Recursive state machine construction 
//
// Inputs       : atoms - list of atoms in state machine
//                from - the state that is entering the group
// Outputs      : last state in group, throws exception on error

bool bfsRegExpression::nextLiteral( char ch, bool escaped, re_literal_t & lit ) {

    // Local variables
    string message;

    // Process an escaped character
    if ( escaped ) {
        
        // What kind of escaped character is it?
        switch (ch) {
        case 'w': // A word literal "\w", [a-zA-Z0-9_]
            lit.ltype = RE_WORD_LITERAL;
            break;
        case 's': // A space literal "\s", [ \t\n\r\f\v]
            lit.ltype = RE_SPACE_LITERAL;
            break;
        case 'd': // A digit literal "\d", [0-9]
            lit.ltype = RE_DIGIT_LITERAL;
            break;
        default: // Just a normal escaped character
            lit.ltype = RE_NORMAL_LITERAL;
            lit.ch = ch;
        }

    } else { 

        // Look for the "Any" character
        if ( ch == '.' ) {
            lit.ltype = RE_ANY_LITERAL;
        } else {
            // Look for a syntax error,
            if ( re_special_chars.find_first_of(ch) != string::npos ) {
                message = "Syntax error in regular expression at : " + ch;
                throw new bfsRegExpressionError( message );
            }
            lit.ltype = RE_NORMAL_LITERAL;
            lit.ch = ch;
        }

    }

    // Return successfull
    return( true );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::generateStateMachine
// Description  : Recursive state machine construction 
//
// Inputs       : atoms - list of atoms in state machine
//                from - the state that is entering the group
// Outputs      : last state in group, throws exception on error

state_t bfsRegExpression::generateStateMachine( atoms_t & atms, state_t from ) {

    // Globals
    string message;
    state_t next, last;
    atoms_t::iterator it;
    re_transition_t transition;

    // Walk the list of items in the list of atoms
    for ( it=atms.begin(); it!=atms.end(); it++ ) {

        // Pick out the next state
        next = states++;

        // Do the terminal state transitions
        if ( (it->atype == RE_LITERAL) || (it->atype == RE_ORLIST) ) {
            
            switch (it->modifier) {
                case RE_NOMODIFER: // None
                ADD_LITERAL_TRANSITION( smachine, from, next, it );
                break;
            
            case RE_ZEROORMORE: // '*'
                ADD_LITERAL_TRANSITION( smachine, from, next, it );
                ADD_LITERAL_TRANSITION( smachine, from, from, it );
                ADD_NIL_TRANSITION( smachine, from, next );
                break;
            
            case RE_ONEORMORE: // '+'
                ADD_LITERAL_TRANSITION( smachine, from, next, it );
                ADD_LITERAL_TRANSITION( smachine, from, from, it );
                break;
            
            case RE_ZEROORONE: // '?'
                ADD_LITERAL_TRANSITION( smachine, from, next, it );
                ADD_NIL_TRANSITION( smachine, from, next );
                break;
            }
            
        } else if ( it->atype == RE_GROUP ) {

            last = generateStateMachine( it->group, from );
            switch (it->modifier) {
                case RE_NOMODIFER: // None
                ADD_NIL_TRANSITION( smachine, last, next );
                break;
            
            case RE_ZEROORMORE: // '*'
                ADD_NIL_TRANSITION( smachine, last, next );
                ADD_NIL_TRANSITION( smachine, last, from );
                ADD_NIL_TRANSITION( smachine, from, next );
                break;
            
            case RE_ONEORMORE: // '+'
                ADD_NIL_TRANSITION( smachine, last, next );
                ADD_NIL_TRANSITION( smachine, last, from );
                break;
            
            case RE_ZEROORONE: // '?'
                ADD_NIL_TRANSITION( smachine, last, next );
                ADD_NIL_TRANSITION( smachine, from, next );
                break;
            }

        } else {
            // Some really bogus error
            message = "Bad atype in machine generator [%d]" + to_string(it->atype);
            throw new bfsRegExpressionError( message );
        }

        // Move to the next state
        from = next;

    }

    // Return last state used
    return( next );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::match
// Description  : Recursive match function over the string (NFA traversal)
//
// Inputs       : state - the current state we are in
//                matchstr - string to match
//                pos - current position in the string
///               depth - the current recursion depth
// Outputs      : true if match, false if not, throws exception on error

bool bfsRegExpression::match( state_t state, string matchstr, size_t pos, size_t depth, states_t & nils ) {

    // Local variables
    states_t newnils;
    state_machine_t::iterator it;
    re_literals_t::iterator orit;
    string message;
    bool found;

    // Log, check to see if we have recursed into a transition cycle (infinite recursion)
    logMessage( UTIL_VRBLOG_LEVEL, "Entered state s%d, depth %d, pos=%d [%c], %d nils", state, depth, pos, matchstr[pos], nils.size() );
    logMessage( UTIL_VRBLOG_LEVEL, "REGEXP: \"%s\", str \"%s\"", regexp.c_str(), matchstr.c_str() );

    // Walk the applicable transitions
    for ( it=smachine.begin(); it!=smachine.end(); it++ ) {
        if ( it->fromstate == state ) {
            switch (it->literal.ltype) {

                // Normal literal, pass along
                case RE_NORMAL_LITERAL: // A normal literal, e.g., "a"
	            case RE_ANY_LITERAL: // A literal can be anything "."
	            case RE_WORD_LITERAL: // A word literal "\w", [a-zA-Z0-9_]
	            case RE_SPACE_LITERAL: // A space literal "\s", [ \t\n\r\f\v]
	            case RE_DIGIT_LITERAL: // A digit literal "\d", [0-9]
                    newnils.clear();
                    if ( (pos < matchstr.length()) && (matchLiteral(it->literal, matchstr[pos])) && 
                        (match(it->tostate, matchstr, pos+1, depth+1, newnils) == true) ) {
                        return( true );
                    }
                    break;

                // Check the NIL transition, abort if cycle
                case RE_NIL_LITERAL:
                    logMessage( UTIL_VRBLOG_LEVEL, "NIL transition match from state s%d to state s%d", state, it->tostate );
                    if ( std::find(nils.begin(), nils.end(), state) == nils.end() ) {
                        newnils = nils;
                        newnils.push_back( state );
                        if (  match(it->tostate, matchstr, pos, depth+1, newnils) == true ) {
                            return( true );
                        } 
                    } else {
                        logMessage( UTIL_VRBLOG_LEVEL, "Aborting NIL state cycle at state (%d)", state );
                    }
                    break;

                // Check the OR state
                case RE_OR_LITERAL:
                    if ( pos < matchstr.length() ) {
                        found = false;
                        for ( orit=it->orlist.begin(); (orit!=it->orlist.end()) && (!found); orit++ ) {
                            found |= matchLiteral( *orit, matchstr[pos] );
                        }
                        newnils.clear();
                        if ( (pos < matchstr.length()) && (found != it->invert) && 
                            (match(it->tostate, matchstr, pos+1, depth+1, newnils) == true) ) {
                            return( true );
                        }
                    }
                    break;
                    
                default: // Bad literal type, fail
                    throw new bfsRegExpressionError( "Bad literal type in match : " + it->literal.ltype );
            }
        }
    }

    // Check for the terminating state (failed or not)
    if ( pos >= matchstr.length() ) {
        return( state == last_state );
    }

    // Return failed to match
    return( false );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::matchLiteral
// Description  : Check a specific character for a match
//
// Inputs       : lit - the literal to match
//                ch - the character to match against
// Outputs      : true if successful, throws exception on error

bool bfsRegExpression::matchLiteral( re_literal_t & lit, char ch ) {

    // Local variables
    bool retval;

    // Switch on the literal type
    switch (lit.ltype) {
        case RE_NORMAL_LITERAL: // A normal literal, e.g., "a"
            retval = (lit.ch == ch);
            break;

        case RE_ANY_LITERAL: // A literal can be anything "."
            retval = true;
            break;

        case RE_WORD_LITERAL: // A word literal "\w", [a-zA-Z0-9_]
            retval = (re_WordChars.find(ch) != string::npos);
            break;

        case RE_SPACE_LITERAL:  // A space literal "\s", [ \t\n\r\f\v]
            retval = (re_SpaceChars.find(ch) != string::npos);
            break;

        case RE_DIGIT_LITERAL: // A digit literal "\d", [0-9]
            retval = (re_DigitChars.find(ch) != string::npos);
            break;

        default: // Should never happen
            throw new bfsRegExpressionError( "Bad literal type in state machine, matching failed" );
    }

    logMessage( UTIL_VRBLOG_LEVEL, "Literal %s/%c %s char %c", literalStrings[lit.ltype].c_str(), lit.ch,
        (retval == true) ? "matched" : "did not match", ch );

    // Return the match bool
    return( retval );
}

//
// Static class methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::unitTest
// Description  : Do the Unit test for this class
//
// Inputs       : none
// Outputs      : true if successful, throws exception on error

bool bfsRegExpression::unitTest( void ) {

    // Local variables
    bfsRegExpression expression;
    string re, restr;
    int i;

    const char * expressions[] = {
        // Expression with direct match
    /*
        "(m)+(s?)+", "mmm",
        "zx*(y*)", "zxxxxy",
        "y?(v?)[\\[\\.\\)\\+]", ".",
        "y[^\\)\\?]*d", "yd",
        "a.*", "a",
        "a.*", "ax",
        "[ab]", "a", 
        "abcde", "abcde",
        // Expression with zero or more
        "ab*c", "abbc",
        // Expression with group
        "a(bc)d", "abcd",
        "a(bc)*d", "abcd",
        "a(bc)+d", "abcd",
        "a(bc)?d", "abcd",
        // Expression with replication
        "a(bcd)*e", "abcdbcdbcde",
        // Expression with or list
        "a*[bcdef]+g", "aaadfg",
*/
         // Complex expressions
        "a*bc[^d]+e?.*", "abcxef",
        "a*bc[^d]+e?.*", "bcyef",
        "a*bc[^d]+e?.*", "aabcZef",
        // Some weird errors
        "\\]rvqb*vwz[^\\*\\(]j", "]rvqbbbvwzXj",
        "y*", "yyyy",
        "hiyw+v+way*", "hiywwwwwvvwayyyy",
        "(u[^k\\(f]+va*)\\[?", "utwhlv[",
        "hiyw+v+way*(u[^k\\(f]+va*)\\[?", "hiywwwwwvvwayyyyutwhlv[",
        NULL, NULL
    };

    // Log and do a simple regular expression
    logMessage( LOG_INFO_LEVEL, "Starting regular expression unit test." );
    i = 0;
    while ( expressions[i] != NULL ) {
        logMessage( LOG_INFO_LEVEL, "Trying expression : %s on %s", expressions[i], expressions[i+1] );
        expression.setExpression( expressions[i] );
        
        if ( expression.match(expressions[i+1]) == false ) {
            logMessage( LOG_ERROR_LEVEL, "Failed expression match %s not maching %s", expressions[i+1], expressions[i] );
            return( false );
        } else {
            logMessage( LOG_INFO_LEVEL, "Success expression match %s maching %s", expressions[i+1], expressions[i] );
        }
        i += 2;
    }

    // Create some random regular expressions and test them
    for ( i=0; i<REGEXP_UTEST_ITERATIONS; i++ ) {

        // Generate the regular expression and 
        re = generateRegExpression( REGEXP_UTEST_RE_SIZE );
        logMessage( UTIL_VRBLOG_LEVEL, "Generated expression %d : %s", i, re.c_str() );
        expression.setExpression( re );
        restr = generateRegexpString( expression.machine );
        logMessage( UTIL_VRBLOG_LEVEL, "Generated string %d : %s", i, restr.c_str() );

        // Now test the expression
        if ( expression.match(restr) != true ) {
            logMessage( UTIL_VRBLOG_LEVEL, "Failed match of expression \"%s\", string \"%s\"", re.c_str(), restr.c_str() );
            return( false );
        }
        logMessage( UTIL_LOG_LEVEL, "Success match of expression \"%s\", string \"%s\"", re.c_str(), restr.c_str() );
    }

    // Log and return successfully
    logMessage( LOG_INFO_LEVEL, "Regular expression unit test completed successfully" );
    return( true );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::generateRegexp
// Description  : generate a random regular expression (for testing)
//
// Inputs       : atms - the numbner of atoms to create
// Outputs      : string regular expression if successful, throws exception on error

string bfsRegExpression::generateRegExpression( uint8_t atms ) {

    // Local variables
    string regexp, specials = "wds";
    uint8_t i, j, orvals;
    uint32_t rnd;
    size_t idx;
    char ch;

    // Walk a certain number of atoms
    for ( i=0; i<atms; i++ ) {

        // Create the atom
        rnd = get_random_value( 1, 100 );
        if ( rnd < 80 ) { // Normal literal
            regexp += (char)('a' + ((char)get_random_value(0, 25)));
        } else if ( rnd < 85 ) {  // special chars
            ch = specials[get_random_value(0,(uint32_t)specials.length()-1)];
            regexp += '\\';
            regexp += ch;
        } else if ( rnd < 90 ) {  // escape chars
            ch = re_special_chars[get_random_value(0,(uint32_t)re_special_chars.length()-1)];
            regexp += '\\';
            regexp += ch;
        } else if ( rnd < 95 ) { // Group
            regexp += '(' + generateRegExpression((uint8_t)get_random_value(1, 4)) + ')';
        } else { // Or group
            regexp += (get_random_value(0,1)) ? "[" : "[^"; // Invert or not
            orvals = (uint8_t)get_random_value(1, 4); // Number of or values
            for ( j=0; j<orvals; j++ ) {
                if ( get_random_value(0, 1) ) {
                    idx = get_random_value(0,(uint32_t)re_special_chars.length()-1);
                    logMessage( LOG_INFO_LEVEL, "OR index : %d", idx );
                    regexp += '\\';
                    regexp += re_special_chars[idx];
                } else {
                    regexp += (char)('a' + ((char)get_random_value(0, 25)));
                }
            }
            regexp += ']';
        }

        // Add a modifier (maybe)
        rnd = get_random_value( 1, 100 );
        if ( rnd < 10 ) {
            regexp += '*';
        } else if ( rnd < 20 ) {
            regexp += '+';
        } else if ( rnd < 30 ) {
            regexp += '?';
        }

    }

    // Return successfully
    return( regexp );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::generateRegexpString
// Description  : generate a random string matching a regular expression
//
// Inputs       : atms - the numbner of atoms to create
// Outputs      : string macting regular expression or throws exception 

string bfsRegExpression::generateRegexpString( atom_t & atm ) {

    // Local variables
    atoms_t::iterator it;
    re_literals_t::iterator orit;
    bool done, found;
    size_t reps, i;
    string restr;
    char ch;

    // Set the number of repetitions
    switch (atm.modifier) {
        case RE_NOMODIFER: // None
            reps = 1;
            break;
        case RE_ZEROORMORE: // '*'
            reps = get_random_value(0, UTEST_STRING_MAXREPS);
            break;
        case RE_ONEORMORE:  // '+'
            reps = get_random_value(1, UTEST_STRING_MAXREPS);
            break;
        case RE_ZEROORONE: // '?'
            reps = get_random_value(0, 1);
            break;
    }

    // For each rep, just keep adding literals
    for ( i=0; i<reps; i++ ) {
        if ( atm.atype == RE_LITERAL ) {
            restr += generateLiteral( atm.literal );
        } else if ( atm.atype == RE_ORLIST ) {
            if ( atm.invert ) {
                done = found = false;
                while ( !done ) {
                    found = false;
                    ch = re_Printables[get_random_value(0, (uint32_t)re_Printables.length()-1)];
                    for ( orit=atm.orlist.begin(); (orit!=atm.orlist.end()) && (!found); orit++ ) {
                        found = (ch == orit->ch);
                    }
                    done = (!found);
                }
                restr += ch;
            } else {
                restr += generateLiteral( atm.orlist[get_random_value(0, (uint32_t)atm.orlist.size()-1)] );
            }
        } else if ( atm.atype == RE_GROUP ) {
            for ( it=atm.group.begin(); it!=atm.group.end(); it++ ) {
                restr += generateRegexpString( *it );
            }
        }
    }

    logMessage( UTIL_VRBLOG_LEVEL, "String subgen %s", restr.c_str() );

    // Next option (always available)
    return( restr );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::generateLiteral
// Description  : Generate a random literal from definition
//
// Inputs       : lit - the literal definition to generate from
// Outputs      : character literal or throws exception on error

char bfsRegExpression::generateLiteral( re_literal_t & lit ) {

    // Local variables
    char retval;

    // Switch on the literal type
    switch (lit.ltype) {
        case RE_NORMAL_LITERAL: // A normal literal, e.g., "a"
            retval = lit.ch;
            break;

        case RE_ANY_LITERAL: // A literal can be anything "."
            retval = re_Printables[get_random_value(0, (uint32_t)re_Printables.length()-1)];
            break;

        case RE_WORD_LITERAL: // A word literal "\w", [a-zA-Z0-9_]
            retval = re_WordChars[get_random_value(0, (uint32_t)re_WordChars.length()-1)];
            break;

        case RE_SPACE_LITERAL:  // A space literal "\s", [ \t\n\r\f\v]
            retval = re_SpaceChars[get_random_value(0, (uint32_t)re_SpaceChars.length()-1)];
            break;

        case RE_DIGIT_LITERAL: // A digit literal "\d", [0-9]
            retval = re_DigitChars[get_random_value(0, (uint32_t)re_DigitChars.length()-1)];
            break;

        default: // Should never happen
            throw new bfsRegExpressionError( "Bad literal type in state machine, matching failed" );
    }

    // Return the generated literal
    return( retval );
}