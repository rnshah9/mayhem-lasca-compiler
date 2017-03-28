--------------------------------------------------------------------
-- |
-- Module    :  Parser
-- Copyright :  (c) Stephen Diehl 2013
-- License   :  MIT
-- Maintainer:  stephen.m.diehl@gmail.com
-- Stability :  experimental
-- Portability: non-portable
--
--------------------------------------------------------------------
{-# LANGUAGE OverloadedStrings #-}

module Parser where

import Text.Parsec
import Text.Parsec.String (Parser)
import qualified Data.Text as T
import Control.Applicative ((<$>))

import qualified Text.Parsec.Expr as Ex
import qualified Text.Parsec.Token as Tok

import Lexer
import Syntax
import Type

integerLit :: Parser Expr
integerLit = Literal . IntLit . fromIntegral <$> integer

floating :: Parser Expr
floating = Literal . FloatLit <$> float

strToBool :: String -> Bool
strToBool "true" = True
strToBool _ = False

boolLit :: Parser Expr
boolLit = Literal . BoolLit . strToBool <$> (true <|> false)
  where
    true = reserved "true" >> return "true"
    false = reserved "false" >> return "false"

stringLit :: Parser Expr
stringLit = Literal . StringLit <$> stringLiteral

binop = Ex.Infix parser Ex.AssocLeft
  where parser = (\op lhs rhs -> Apply (Var op) [lhs, rhs]) <$> op
unop = Ex.Prefix parser
  where parser = (\op expr -> Apply (Var ("unary" ++ op)) [expr]) <$> op

binary s assoc = Ex.Infix parser assoc
  where parser = reservedOp s >> return (\lhs rhs -> Apply (Var s) [lhs, rhs])

op :: Parser String
op = do
  whitespace
  o <- operator
  whitespace
  return o

binops = [
          [binary "*" Ex.AssocLeft, binary "/" Ex.AssocLeft],
          [binary "+" Ex.AssocLeft, binary "-" Ex.AssocLeft],
          [binary "<" Ex.AssocLeft, binary "==" Ex.AssocLeft, binary "!=" Ex.AssocLeft],
          [binary "and" Ex.AssocLeft],
          [binary "or" Ex.AssocLeft]
          ]

operatorTable = binops ++ [[unop], [binop]]

expr :: Parser Expr
expr =  Ex.buildExpressionParser operatorTable apply

variable :: Parser Expr
variable = Var <$> identifier

typeAscription :: Parser Type
typeAscription = do
  reservedOp ":"
  name <- identifier
  return $ TCon name

funcArgument :: Parser Name
funcArgument = do
  name <- identifier
  typeAsc <- option typeAny typeAscription
  return name

function :: Parser Expr
function = do
  reserved "def"
  name <- identifier
  args <- parens $ commaSep arg
  tpe <- option typeAny typeAscription
  reservedOp "="
  body <- expr
  return (Function name tpe args body)

extern :: Parser Expr
extern = do
  reserved "extern"
  reserved "def"
  name <- identifier
  args <- parens $ commaSep arg
  reservedOp ":"
  tpe <- identifier
  return (Extern name (TCon tpe) args)

arg :: Parser Arg
arg = do
  name <- identifier
  tpe <- option typeAny typeAscription
  return (Arg name tpe)

apply :: Parser Expr
apply = try methodCall <|> try call <|> factor

methodCall = do
  arg <- factor
  string "."
  func <- identifier
  return (Apply (Var func) [arg])

call :: Parser Expr
call = do
  name <- factor
  args <- parens $ commaSep expr
  return (Apply name args)

ifthen :: Parser Expr
ifthen = do
  reserved "if"
  cond <- expr
  reserved "then"
  tr <- expr
  reserved "else"
  fl <- expr
  return (If cond tr fl)

letins :: Parser Expr
letins = do
  reserved "let"
  defs <- commaSep $ do
    var <- identifier
    reservedOp "="
    val <- expr
    return (var, val)
  reserved "in"
  body <- expr
  return $ foldr (uncurry Let) body defs

closure = do
  c <- braces cls
  return c
    where cls = do
            args <- commaSep arg
            reservedOp "->"
            letin <- blockStmts
            let lambdas = foldr (\(Arg a _) expr -> Lam a expr) letin args
            return lambdas

data LetVal = Named Name Expr | Stmt Expr

valdef = do
  id <- identifier
  reservedOp "="
  e <- expr
  return (Named id e)

unnamedStmt = do
  e <- expr
  return (Stmt e)

blockStmts = do
  exprs <- (try valdef <|> unnamedStmt) `sepEndBy` semi
  let letin = foldStmtsIntoOneLetExpr (reverse exprs)
  return letin
  where
        foldStmtsIntoOneLetExpr [] = Literal UnitLit
        foldStmtsIntoOneLetExpr exprs@(lst : init) = do
          let (init', last') = case lst of
                                (Stmt e) -> (init, e)
                                (Named _ _) -> (exprs, Literal UnitLit)
          let namedExprs = go init' 1
          foldl (\acc (name, e) -> Let name e acc) last' namedExprs

        go ((Stmt e) : exprs) idx = ('_' : show idx, e) : go exprs (idx + 1)
        go ((Named id e) : exprs) idx = (id, e) : go exprs idx
        go [] _ = []


block :: Parser Expr
block = braces blockStmts

dataDef :: Parser Expr
dataDef = do
  reserved "data"
  typeName <- identifier
  reservedOp "="
  constructors <- dataConstructor `sepBy` reservedOp "|"
  return (Data typeName constructors)

dataConstructor = do
  name <- identifier
  args <- parens (arg `sepEndBy` comma)
  return (DataConst name args)

factor :: Parser Expr
factor = try floating
      <|> try boolLit
      <|> try letins
      <|> try stringLit
      <|> try integerLit
--       <|> try apply
      <|> try variable
      <|> try closure
      <|> ifthen
      <|> block
      <|> (parens expr)

defn :: Parser Expr
defn = try extern
    <|> try function
    <|> try dataDef
    <|> expr

contents :: Parser a -> Parser a
contents p = do
  Tok.whiteSpace lexer
  r <- p
  eof
  return r

toplevel :: Parser [Expr]
toplevel = many $ do
    def <- defn
    return def

parseExpr :: String -> Either ParseError Expr
parseExpr s = parse (contents expr) "<stdin>" s

parseToplevel :: String -> Either ParseError [Expr]
parseToplevel s = parse (contents toplevel) "<stdin>" s
