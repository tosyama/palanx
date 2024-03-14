Palan Abstract Syntax Tree Json Specification
============================================
 
ver. 0.0.1  

\* - Required

Root
----
- import\* - import file list
- export\* - export declaration list
- ast\* - AST model

Import file list
----------------
- path-type\* - Path type string: "src" "inc"
- path\* - Path string

Export declaration list
-------------------------
T.B.D

AST model
---------
- statements - Statement model list

Statement model
---------------
- stmt-type - Statement type: "import"
  1. import - import module statement
    - path-type\* - Path type string: "src" "inc"
    - path\* - Path string
    - targets - Import target name string list, null - all
    - alias - Alias name string
T.B.D

