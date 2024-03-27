# (=​｀ω´=) MEWA
A simple Math EWAluator written on C.

## Installation
```sh
git clone https://github.com/mandriota/mewa
cd mewa
./configure && make && sudo make install
```

## Featchers
- [x] Basic arithmetic operators
- [x] Operators priority and associativity
- [x] Type inference
- [ ] Functions
- [ ] Function specialization
- [ ] Function ranged specialization
- [x] Command-line arguments and redirects handling
- [x] REPL
- [ ] REPL: multiline input
- [x] REPL: history
- [x] REPL: escape handling

## Naming Conventions
### Constants, Enums, Defines
All caps, snake case.

### Structs
Title, camel case.

### Functions, local variables
All script, snake case.

### Methods
Same as functions. First word is an abbreviation of struct name.

### Names abbreviations list
| Name          | Abbreviation |
|:--------------|:-------------|
| `Primitive`   | `PM`/`PRIM`  |
| `Reader`      | `RD`         |
| `Token`       | `TK`         |
| `TokenType`   | `TT`         |
| `Lexer`       | `LX`         |
| `Node`        | `ND`         |
| `NodeType`    | `NT`         |
| `Parser`      | `PR`         |
| `Priority`    | `PT`         |
| `Interpreter` | `IR`         |

## Acknowledgements
- Thanks to [Giovanni Crisalfi](https://github.com/gicrisf) for [the kaomoji collection](https://github.com/gicrisf/kaomel) used in this file.
