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
	| unaryOp unaryExp					# unary3;

unaryOp: '+' | '-' | '!';

funcRParams: funcRParam (',' funcRParam)*;

funcRParam: exp # expAsRParam | STRING # stringAsRParam;

lexp:
	lexp '||' lexp		# lOrExp
	| lexp '&&' lexp	# lAndExp
	| exp				# exp1;

exp:
	exp ('==' | '!=') exp				# eqExp
	| exp ('<' | '>' | '<=' | '>=') exp	# relExp
	| exp ('*' | '/' | '%') exp			# mulExp
	| exp ('+' | '-') exp				# addExp
	| unaryExp							# exp2;

constExp: exp;
