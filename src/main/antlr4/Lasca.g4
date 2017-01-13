/*
 [The "BSD licence"]
 Copyright (c) 2014 Leonardo Lucena
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:
 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
 3. The name of the author may not be used to endorse or promote products
    derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*
   Derived from http://www.scala-lang.org/files/archive/spec/2.11/13-syntax-summary.html
 */

grammar Lasca;

valDef: 'val' Id (':' type)? '=' expr;

literal:
      IntegerLiteral # integer
	| BooleanLiteral # boolean
	| StringLiteral  # stringLit
   ;

ident: Id;

externDef:
	 'extern' 'def' Id paramClause? (':' type)? ';'?
   ;

defDef
   : 'def' Id paramClause? (':' type)? '=' expr ';'?
   ;

paramClause
   : '(' params? ')'
   ;

params
   : param (',' param)*
   ;

param
   : Id (':' type)?
   ;

ifExpr: 'if' expr 'then' expr ('else' expr)?;

expr:
     ifExpr
   | infixExpr
   ;

infixExpr
   : prefixExpr
   | infixExpr op=('<<' | '>>') infixExpr
   | infixExpr op='%' infixExpr
   | infixExpr op=('*' | '/') infixExpr
   | infixExpr op='xor' infixExpr
   | infixExpr op=('+' | '-') infixExpr
   | infixExpr op=('==' | '!=' | '<' | '<=' | '>' | '>=') infixExpr
   | infixExpr op='and' infixExpr
   | infixExpr op='or' infixExpr
   ;

prefixExpr
   : UnaryOp? (blockExpr | simpleExpr1)
   ;

simpleExpr1:
     simpleExpr2
   | simpleExpr1 argumentExprs
   ;

simpleExpr2
   : ident
  | literal
  | '(' ex=expr ')'
  ;

argumentExprs
   : '(' exprs? ')'
   | simpleExpr2
   ;

exprs
   : expr (',' expr)*
   ;

blockExpr:
	'{' block '}'
   ;

block
   : blockStat (Semi blockStat)* expr?
   ;

blockStat
   : valDef
   | defDef
   | expr
   |
   ;

type: TypeId
   ;

compilationUnit
   : (externDef | defDef | valDef)*
   ;

// Lexer

BlockComment
   : '{-' .*? '-}' -> skip
   ;

InlineComment : '--' .*? '\n' -> skip
   ;

BooleanLiteral
   : 'true' | 'false'
   ;

StringLiteral
   : '"' StringElement* '"'
   ;

UnaryOp: ('-' | '+' | '~' | '!');

IntegerLiteral
   : DecimalNumeral
   ;

TypeId
   : Upper Id
   ;

Id
   : (Lower | Upper)+
   ;


Varid
   : Lower+
   ;


WS
   : [ \r\n\t] -> skip
   ;


Semi
   : ';'
   ;


Paren
   : '(' | ')' | '[' | ']' | '{' | '}'
   ;


Delim
   : '`' | '\'' | '"' | '.' | ';' | ','
   ;

// fragments

fragment WhiteSpace
   : '\u0020' | '\u0009' | '\u000D' | '\u000A'
   ;


fragment Upper
   : 'A' .. 'Z' | '$'
   ;

// and Unicode category Lu

fragment Lower
   : 'a' .. 'z'
   ;

// and Unicode category Ll

fragment Letter
   : Upper | Lower
   ;

fragment StringElement
   : '\u0020' | '\u0021' | '\u0023' .. '\u007F' | CharEscapeSeq
   ;

fragment CharEscapeSeq
   : '\\' ('b' | 't' | 'n' | 'f' | 'r' | '"' | '\'' | '\\')
   ;

fragment PrintableChar
   : '\u0020' .. '\u007F'
   ;

fragment DecimalNumeral
   : '0' | NonZeroDigit (Digit | '_')*
   ;


fragment HexDigit
   : '0' .. '9' | 'A' .. 'F' | 'a' .. 'f'
   ;

fragment HexNumeral
   : '0' 'x' HexDigit HexDigit +
   ;

fragment Digit
   : '0' | NonZeroDigit
   ;


fragment NonZeroDigit
   : '1' .. '9'
   ;
