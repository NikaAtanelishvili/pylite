PART 2: LEXER

Lexer(ზოგან მოხსენიებულუ, როგორც ტოკენიზატორი ან სკანერი) ჩვენს მიერ დაწერილ კოდს(raw source code), რომელსაც ამ ეტაპზე ტექტს(TEXT) ვეძახით, ტოკენების ნაკადად აქცევთს. ტოკენი უმარტივესი ერთეულია, რომელსაც ენა ცნობს.

მაგალითად, ინფუთი: 
	"x = 12 + 3.5"

გადაიქცევა:
	IDENT(x) -- x
	EQUALS -- =
	NUMBER(12) -- 12
	PLUS -- +
	NUMBER(3.5) -- 3.5
	NEWLINE -- \n
	EOF	 -- End Of the File

როგორც შეამჩნევდით, Lexer ვალდებულია შეყვანილი კოდის გასუფთავებისთვის, რაშიც გამოტოვებული ადგილებისა და ახალი ხაზების მოშორება იგულისხმება.

Lexer-მა არ იცის ენის გრამატიკა და არ ანაღვლებს კოდის შინაარსი, მისი მოვალეობა, როგორც უკვე ვახსენეთ ჩვენს მიერ მიწოდებული ტექსტის ტოკენიზირებაა.

Lexer-ი Finite Automata-ს შინაარს ატარებს. თითოეული ტოკენს DFA(მდგომარეობების და გადამსვლელი ფუნქციების სეტი) ცნობს. მაგალითად, როდესაც რიცხვს ასკანერებ, მდგომარეობებს შორის მოძრაობ: "დაიწყო როგორც ციფრი" -> "ციფრს კიდევ მოსდევს ციფრები" -> "წერტილი შევამჩნიერთ, ვკითხულობთ ათწილადს" -> "ფინიში". სწორედ ესაა DFA.

Lexer-ი სამ ინფორმაციას ინახავს: source string-ის ფოინტერს, ამჟამინდელი ხაზის ნომერსა და სიმბოლოს, რომელსაც ახლა უყურებს. 

Parser(რომელსაც მოგვიანებით ვნახავთ) და Lexer-ი დამოუკიდებლად არ მუშაობენ. Parser-ი პასუხისმგებელია ყველაფერმა სწორად და რიგ-რიგობით უმუშავოს. ეს უკანასკნელი იძახებს next_token(), როდესაც სჭირდება გაიგოს რა მოდის შემდგომ. Lexer-ი უბრუნებს პასუხს და ელოდება შემდგომ ბრძანებებს. ეს ურთიერთობა ძალიან გავს საუბარს:

Parser:  "მომეცი შემდეგი ტოკენი."
Lexer:   "შემდეგი ტოკენია IDENT 'x'."
Parser:  "ეგ რაღაცის(ცვლაის, ფუნქციის) სახელია. მომეცი შემდეგი ტოკენი"
Lexer:   "შემდეგი ტოკენია ASSIGN '='."
Parser:  "ეგ მინიჭებაა! მომეცი შემდეგი ტოკენი."
Lexer:   "შემდეგი ტოკენია NUMBER '10'."
Parser:  "ეგ გამოსახულების მარჯვენა მხარეა. შესრულებულია."





PART 3: PARSER


მაშინ, როცა Lexer-ი სტრიქონს(if (x == 10)) ტოკენების სიმრავლედ აქცევს, Parser-ი ის ნაწილია რომელსაც გრამატიკს აქცევს ყურადღებას.

კომპიუტერმა არ იცის როგორ შეასრულოს ერთ განზომილებიან სიმრავლეში მოთავსებული კოდი, მაგალითად, მან არ იცის რომ გამოსახულების შესრულებისას გამრავლემას, მიმატების მიმართ, უპირატესობა ენიჭება

Parser-ის მოვალებაა აიღოს ტოკენების ნაკადი და ენის წესებზე დაყრდნობით შექმანს იერარქიული ხის სტრუქტურა Abstract Syntax Tree (AST).
[ Raw Source Code ]  --> "x = 5 + 10"
        |
        v  (Step 1: Lexical Analysis / Lexer)
[ Stream of Tokens ] --> [IDENT: "x"], [ASSIGN: "="], [NUMBER: "5"], [PLUS: "+"], [NUMBER: "10"]
        |
        v  (Step 2: Syntactic Analysis / Parser) <--- HERE IS YOUR PARSER
[ Abstract Syntax Tree ]

თუკი ტოკენების საკადი ემთხვევა ენის გრამატიკის წესებს, Parser-ი მათ nested მონაცემთა სტრუქტურად აქცევს. მიღებული ხე თავის თავში მოიცავს ოპერაციათა მიმდევრობასა(მაგ. გამრავლება მიმატებამდე) და სტრუქტურულ საზღვრებს(if ბლოკის კოდის ენკაფსულაცია)

x = 5 + 10

           [ NODE_ASSIGN ]
             /        \
    name: "x"          v .value
                 [ NODE_BINARY: + ]
                    /          \
                   v            v
            [ NODE_NUMBER: 5 ] [ NODE_NUMBER: 10 ]

პროგრამა ტოკენების ნაკადს შემდეგ ნაირდან უმკლავდება:

1. Peeking and Consuming: Parser-ი უყურებს ამჟამინდელ ტოკენს (lexer_peek ფუნქციის დახმარებით, რომელიც ახლახან ვნახეთ)

2. Decision Making: ნანახ ტოკენზე დაყრდნობით, პროგრამა იღებს გადაწყვეტილებას. მაგალითად, TOK_IF-ის შემთხვევაში, ის გამოიძახებს სპეციალურ ფუნქციას, როემლიც if პირობებს უმკლავდება.

3. Grammar Enforcement: Parser-ი უზრუნველყოფს, რომ ტოკენების ნაკადი ენის გრამატიკას ემთხვევა. მაგალითად if პირობის შემდეგ მოდის ( სიმბლოლო, და არა +.

4. Building the Tree: Parser ტოკენებს მეხსიერებაში ხის node-ებს ანიჭებს და ერთმანეთს აკავშირებს. მაგითად, გამოსახულებაში "3 + 5", "+" მშობელ node-ის როლს იკისრებს, ხოლო რიცხვები შვილის.

ჩვენ გამოვიყენებთ Recursive Descent Parsing, რაც იმას ნიშნავს რომ შესრულება მოხდება მაღლიდან დაბლა.

ალგორითმი ამ შემთხვევაში ძალიან ხელსაყრელია, რადგან იგი ფაქტობრივად იმეორებს გრამატიკის წესებს

1. საწყისი წერტილი: ხე იწყება ყველაზე აბსოლიტურ აბსტრაქტულ დონეზე, ჩვენ შემთხვავაში ფუნქციით parse_program()

2. დაშვება(descent): როდესაც ალფორითმი ხედავს ტოკენებს იგი რეკურსიულად ამატებს განშტოებებს, რომელებით თანდათან უფრო ატომური, სპეციფიკური ხდებიან.

-- parse_program() ხდავს სთეითმენთების ლუპს -> იძახებს parse_stmt()
-- parse_stmt() ამოწმებს ტოკენებს -> ხვდება რომ მინიჭების სთეითმენთია, ამიტომ იძახებს parse_assign_stmt()
-- parse_assign_stmt() კითხულობს ცვლადის სახელს და გადადის მის მნიშვნელობაზე, რასაც parse_expr() უზრუნველყოფს.

3. ქვედა სართული: parse_expr() ეშვება ქვემოთ "უპირატესობის კიბეზე" და ჩადის parse_primary(), რომელიც უზრუნველყოფს უშუალო რიცხვების, ცვლადების, სტრიქონების, ა.შ მნიშვნელობებს.

4. ასვლა Ascent: ატომური ნაწილების შესრულების შემდგომ ხდება ხის სტრუქტურაზე ასვლა და მშობელი-შვილი pointer-ების დაკავშირება მანამ სანამ ხის სტრუქტურა სრულყოფილი არ იქნება.


გამოსახულებების(expressions) გამოთვლისას აუცილებელია ოპერატორების უპირატესობების, მარჯვენა და მარტხენა ასოციაციურობის გათვალისწინება. ამგვარ ნიმუშს ქვია precedence climing(Pratt Parser variation). 


PART 4. EVALUTOR & ENVIRONMENT

Parser-მა წარმატებით შექმნა აბსტრაქტული სინტაქსის ხე. ის მეხსიერებაშია მოთავსებული, ახლა მისი გამოყენების დროა, ანუ ჩვენ კომპილაციის დროიდან(compilation time) ვინაცვლებთ შესრულებაზე(runtime)

ხუთი ფაილი: value.h (the runtime type), env.h + env.c (the scope chain), და eval.h + eval.c (ხის მსვლელი). ამის შემდეგ Makefile, მარტივი კომპილაციისთვის და main.c ყველაფრის ერთმანეთთან დასაკავშირებლად

გარემო(Environment) წარმოადგენს ენის მეხსირების ალოკაციის სისტემას. ის ინახავს რა ცვლადები არსებობენ, რომელ scope-ს მიეკუთვნებიან და რა runtime მნიშვნელობებს შეიცავენ.

linked chain of structures
როგორც ვახსენე, გარემო ცვლადების scope-ის გამართულ მუშაობას უზრუნველყოფს. ჩვენი ენის scope ლოგიკა ძალიან გავს თანამედროვე პროგრამული ენებისას. ამის იმპლემენტაცია, ast-ს მსგავსად, ხის სტრუქტურით ხდება. ერთადერთი განსხვავება მისი მიმართულებაა. მაშინ როცა AST-ის node შვილობილი node-ის მისამართს ინახავდა, ამ შემთხვევაში იგი მშობელი node-ის მისამართს ინახავს. 


Look at what you've built here: a memory-isolated, block-scoped, lexically enforced, type-safe tree-walking interpreter engine written in clean, structural C. It handles early-return control flows seamlessly, performs deep string comparisons on the heap, and guards itself against fatal hardware errors like division by zero. It's an exceptionally robust foundation.





















ფორმალური გრამატიკა Backus–Naur Form (BNF)-ის სახით:

<program>       ::= <stmt_list>

<stmt_list>     ::= <stmt>
                  | <stmt> <stmt_list>

<stmt>          ::= <assign_stmt>
                  | <if_stmt>
                  | <while_stmt>
                  | <func_decl>
                  | <return_stmt>
                  | <print_stmt>
                  | <expr_stmt>

<assign_stmt>   ::= IDENT "=" <expr> NEWLINE

<if_stmt>       ::= "if" <expr> "{" <stmt_list> "}"
                  | "if" <expr> "{" <stmt_list> "}" "else" "{" <stmt_list> "}"

<while_stmt>    ::= "while" <expr> "{" <stmt_list> "}"

<func_decl>     ::= "func" IDENT "(" <param_list> ")" "{" <stmt_list> "}"
                  | "func" IDENT "(" ")" "{" <stmt_list> "}"

<return_stmt>   ::= "return" <expr> NEWLINE
                  | "return" NEWLINE

<print_stmt>    ::= "print" "(" <arg_list> ")" NEWLINE

<expr_stmt>     ::= <expr> NEWLINE

<param_list>    ::= IDENT
                  | IDENT "," <param_list>

<arg_list>      ::= <expr>
                  | <expr> "," <arg_list>

<expr>          ::= <or_expr>

<or_expr>       ::= <and_expr>
                  | <and_expr> "or" <or_expr>

<and_expr>      ::= <not_expr>
                  | <not_expr> "and" <and_expr>

<not_expr>      ::= <cmp_expr>
                  | "not" <not_expr>

<cmp_expr>      ::= <add_expr>
                  | <add_expr> <cmp_op> <add_expr>

<cmp_op>        ::= "==" | "!=" | "<" | ">" | "<=" | ">="

<add_expr>      ::= <mul_expr>
                  | <add_expr> "+" <mul_expr>
                  | <add_expr> "-" <mul_expr>

<mul_expr>      ::= <unary_expr>
                  | <mul_expr> "*" <unary_expr>
                  | <mul_expr> "/" <unary_expr>
                  | <mul_expr> "%" <unary_expr>

<unary_expr>    ::= <primary>
                  | "-" <unary_expr>

<primary>       ::= NUMBER
                  | STRING
                  | "true" | "false"
                  | IDENT
                  | IDENT "(" <arg_list> ")"
                  | IDENT "(" ")"
                  | "(" <expr> ")"
