lexer grammar CommonLex;

// keyword
Int : 'int';
Float : 'float';
Void: 'void';
Const: 'const';
Return : 'return';
If : 'if';
Else : 'else';
For : 'for';
While : 'while';
Do : 'do';
Break : 'break';
Continue : 'continue'; 

// operator
Lparen : '(' ;
Rparen : ')' ;
Lbrkt : '[' ;
Rbrkt : ']' ;
Lbrace : '{' ;
Rbrace : '}' ;
Comma : ',' ;
Semicolon : ';';
Question : '?';
Colon : ':';

Minus : '-';
Exclamation : '!';
Tilde : '~';
Addition : '+';
Multiplication : '*';
Division : '/';
Modulo : '%';
LAND : '&&';
LOR : '||';
EQ : '==';
NEQ : '!=';
LT : '<';
LE : '<=';
GT : '>';
GE : '>=';


// integer
IntLiteral
    : [0-9]+ 
    | '0x' [0-9a-fA-F]+ 
    | '0X' [0-9a-fA-F]+ 
    ;

// float
HexadecimalDigits
    : '0x' [0-9a-fA-F]+ 
    | '0X' [0-9a-fA-F]+
    ;

ExponentPart
    : [eE] [+-] [0-9]+
    ;

FractionPart
    : [0-9]* '.' [0-9]+
    | [0-9]+ '.'
    ;

FloatLiteral
    : FractionPart (ExponentPart)?
    | [0-9]+ ExponentPart 
    | (HexadecimalDigits)? '.' HexadecimalDigits 
    | HexadecimalDigits '.'
    ;

//identifier
Identifier
    : [a-zA-Z_][a-zA-Z_0-9]*
    ;

STRING : '"'(ESC|.)*?'"';

fragment
ESC : '\\"'|'\\\\';

WS : 
    [ \t\r\n] -> skip
    ;

LINE_COMMENT : '//' .*? '\r'? '\n' -> skip;
COMMENT      :'/*'.*?'*/'-> skip ;