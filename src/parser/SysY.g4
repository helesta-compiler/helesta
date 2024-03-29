grammar SysY;

import CommonLex;

compUnit: (decl | funcDef)* EOF;

decl: constDecl | varDecl;

constDecl: 'const' bType constDef (',' constDef)* ';';

bType: 'int' | 'float';

constDef: Identifier ('[' constExp ']')* '=' constInitVal;

constInitVal:
	constExp										# scalarConstInitVal
	| '{' (constInitVal (',' constInitVal)*)? '}'	# listConstInitVal;

varDecl: bType varDef (',' varDef)* ';';

varDef:
	Identifier ('[' constExp ']')*					# uninitVarDef
	| Identifier ('[' constExp ']')* '=' initVal	# initVarDef;

initVal:
	exp									# scalarInitVal
	| '{' (initVal (',' initVal)*)? '}'	# listInitval;

funcDef: funcType Identifier '(' (funcFParams)? ')' block;

funcType: 'void' | 'int' | 'float';

funcFParams: funcFParam (',' funcFParam)*;

funcFParam: bType Identifier ('[' ']' ('[' constExp ']')*)?;

block: '{' (blockItem)* '}';

blockItem: decl | stmt;

stmt:
	lVal '=' exp ';'						# assignment
	| (exp)? ';'							# expStmt
	| block									# blockStmt
	| 'if' '(' cond ')' stmt				# ifStmt1
	| 'if' '(' cond ')' stmt 'else' stmt	# ifStmt2
	| 'while' '(' cond ')' stmt				# whileStmt
	| 'break' ';'							# breakStmt
	| 'continue' ';'						# continueStmt
	| 'return' (exp)? ';'					# returnStmt;

cond: lexp;

lVal: Identifier ('[' exp ']')*;

primaryExp:
	'(' exp ')'	# primaryExp1
	| lVal		# primaryExp2
	| number	# primaryExp3;

number: IntLiteral | FloatLiteral;

unaryExp:
	primaryExp							# unary1
	| Identifier '(' (funcRParams)? ')'	# unary2
	| ('+' | '-' | '!') unaryExp		# unary3;

funcRParams: funcRParam (',' funcRParam)*;

funcRParam: exp # expAsRParam | STRING # stringAsRParam;

lexp:
	exp										# exp1
	| lexp ('<' | '>' | '<=' | '>=') lexp	# relExp
	| lexp ('==' | '!=') lexp				# eqExp
	| lexp '&&' lexp						# lAndExp
	| lexp '||' lexp						# lOrExp;

exp:
	unaryExp					# exp2
	| exp ('*' | '/' | '%') exp	# mulExp
	| exp ('+' | '-') exp		# addExp;

constExp: exp;
