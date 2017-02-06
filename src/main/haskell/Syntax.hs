--------------------------------------------------------------------
-- |
-- Module    :  Syntax
-- Copyright :  (c) Stephen Diehl 2013
-- License   :  MIT
-- Maintainer:  stephen.m.diehl@gmail.com
-- Stability :  experimental
-- Portability: non-portable
--
--------------------------------------------------------------------

module Syntax where

import Data.Text

type Name = String

data Expr
  = Literal Lit
  | Var String
  | Apply Name [Expr]
  | Function Name Type [Arg] Expr
  | Extern Name Type [Arg]
  | If Expr Expr Expr
  | Let Name Expr Expr
  deriving (Eq, Ord, Show)

data Type = Type Name | UnitType | AnyType deriving (Eq, Ord, Show)
data Lit = IntLit Int
  | FloatLit Double
  | BoolLit Bool
  | StringLit String
  deriving (Eq, Ord, Show)

data Arg = Arg Name Type deriving (Eq, Ord, Show)

boolType = Type "Bool"
intType = Type "Int"
float64Type = Type "Float64"