(identifier) @variable
(type_identifier) @type
(builtin_type) @type.builtin
(qualified_type module: (identifier) @namespace)
(parameter name: (identifier) @variable.parameter)
(member_expression property: (identifier) @property)
(field_initializer name: (identifier) @property)
(definition_body (variable_declaration name: (identifier) @property))
(enum_definition name: (type_identifier) @enum)
(enum_member name: (identifier) @variant)
(function_definition name: (identifier) @function)
(qualified_function_name name: (identifier) @function)
(operator_name) @function
(call_expression function: (identifier) @function)
(generic_call_expression function: (identifier) @function)
(generic_function_expression function: (identifier) @function)
(call_expression function: (member_expression property: (identifier) @function))
(generic_call_expression function: (member_expression property: (identifier) @function))
(generic_function_expression function: (member_expression property: (identifier) @function))
(builtin_function) @function.builtin
(special_identifier) @variable.special
(link_statement (identifier) @namespace)
(comment) @comment
(string) @string
(template_string) @string
(template_content) @string
(interpolation "${" @punctuation.special "}" @punctuation.special)
(escape_sequence) @string.escape
(number) @number
(boolean) @boolean
(null) @constant.builtin
(modifier) @keyword
["def" "enum" "class" "template" "typename" "where" "auto" "func"
 "if" "else" "do" "while" "for" "in" "guard" "match" "switch" "case" "default" "static_cast" "try" "catch" "return"
 "break" "continue" "link"  "as" "pragma" "strict" "nostrict" "decltype"
 "await" "spawn" "thread" "move" "atomic"] @keyword
["+" "-" "*" "/" "%" "=" "+=" "-=" "*=" "/=" "%="
 "==" "!=" "<" ">" "<=" ">=" "&&" "||" "!" "&" "|" "^" "~"
 "shl" "shr" ".." "++" "--" "?" ":" ".:" "->" "=>" "~>" "and" "or" "not"] @operator
["(" ")" "[" "]" "{" "}"] @punctuation.bracket
[";" "," "." "::"] @punctuation.delimiter
(type_arguments ["<" ">"] @punctuation.bracket)
(type_parameters ["<" ">"] @punctuation.bracket)
(template_clause ["<" ">"] @punctuation.bracket)


(type_test_operator) @function
(type_test_expression ["<" ">"] @punctuation.bracket)
