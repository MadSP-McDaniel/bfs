

TODO:

    X + Add comments to parsing
    X + parse tree implementation
    X + Add list of configurations to config 
    X + Fix the exceptions in the parser/store to be descriptive.
    X + Convert code to use logging 
    X + Rename classes
    + Create and eliminate left-recursion problem
    + Perform a memory check on the parser/configuration
        - check all of the destructors

    Patch in new regular expression system
        X + Replace regex from includes, code (bfsCfgParser.cpp)
            X - include file
            X - replace std::regexp_error with bfsRegExpressionError in try   
        X + Replace reex from includes, code (bfsCfgParserSymbol)
            X - replace symbolMatcher with bfsRegExpression object
            X - replace matching function and constructor
            X - cleanup bfsCfgParserSymbol .cpp file
        + run unit tests

Get
https://web.cs.wpi.edu/~kal/PLT/PLT4.1.2.html


            idx = stoi(cfgtag.substr(pos));
