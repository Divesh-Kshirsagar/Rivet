# Keywords and Tokens

Rivet uses negative integer values for keywords and dynamic token categories. Single-character punctuation/operators are returned as ASCII integer values.

## Dynamic Tokens

| Category | Token Value | Meaning |
|---|---:|---|
| `identifier` | `-2` | Variable/function/module names |
| `number` | `-3` | Integer literal |
| `string_literal` | `-4` | String literal |

## Types and Memory Keywords

| Keyword | Token Value | Status |
|---|---:|---|
| `int` | `-10` | Implemented |
| `void` | `-11` | Tokenized; parser integration limited |
| `str` | `-12` | Implemented |
| `ref` | `-13` | Implemented |
| `address_of` | `-14` | Implemented |
| `deref` | `-15` | Implemented |
| `optref` | `-16` | Tokenized; partial support |

## Logical and Bitwise Keywords

| Keyword | Token Value | Notes |
|---|---:|---|
| `and` | `-20` | Parsed as binary operator |
| `or` | `-21` | Parsed as binary operator |
| `not` | `-22` | Token exists; unary lowering incomplete |
| `lsft` | `-23` | Parsed as shift-left-like operator |
| `rsft` | `-24` | Parsed as arithmetic shift-right-like operator |
| `==` | `-25` | Equality compare |
| `!=` | `-26` | Not-equal compare |

## Control/Module Keywords

| Keyword | Token Value | Status |
|---|---:|---|
| `if` | `-30` | Implemented |
| `else` | `-31` | Implemented |
| `while` | `-32` | Implemented |
| `for` | `-33` | Implemented |
| `fun` | `-34` | Tokenized; function declarations in progress |
| `return` | `-35` | Tokenized; return flow in progress |
| `import` | `-36` | Implemented |
| `in` | `-37` | Implemented |
| `step` | `-38` | Implemented |
| `to` | `-39` | Implemented |

## ASCII Operator Tokens

These are returned directly as character code values by the lexer:

- `=` (`61`)
- `<` (`60`)
- `>` (`62`)
- `+` (`43`)
- `-` (`45`)
- `*` (`42`)
- `/` (`47`)
- `(`, `)`, `{`, `}`, `,`, `;`, `[`, `]`

This is why AST debug output may show numbers like `43` for `+` and `61` for `=`.
