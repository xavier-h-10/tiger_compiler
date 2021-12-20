%filenames parser
%scanner tiger/lex/scanner.h
%baseclass-preinclude tiger/absyn/absyn.h

 /*
  * Please don't modify the lines above.
  */

%union {
  int ival;
  int position;
  std::string* sval;
  sym::Symbol *sym;
  absyn::Exp *exp;
  absyn::ExpList *explist;
  absyn::Var *var;
  absyn::DecList *declist;
  absyn::Dec *dec;
  absyn::EFieldList *efieldlist;
  absyn::EField *efield;
  absyn::NameAndTyList *tydeclist;
  absyn::NameAndTy *tydec;
  absyn::FieldList *fieldlist;
  absyn::Field *field;
  absyn::FunDecList *fundeclist;
  absyn::FunDec *fundec;
  absyn::Ty *ty;
  }

%token <sym> ID
%token <sval> STRING
%token <ival> INT

%token
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK
  LBRACE RBRACE DOT
  EQ NEQ LT LE GT GE
  ASSIGN
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF 
  BREAK NIL
  FUNCTION VAR TYPE

 /* token priority */
 /* TODO: Put your lab3 code here */

/*
  %type binds the nonterminal/terminal symbols to a type defined(the type of semantics) in %union
*/

%type <position> pos
%type <exp> exp expseq subexp logicexp seq_or_one_exp
%type <explist> actuals nonemptyactuals sequencing sequencing_exps
%type <var> lvalue onevar oneormore
%type <declist> decs decs_nonempty
%type <dec> decs_nonempty_s vardec dec
%type <efieldlist> recs rec_nonempty
%type <efield> rec
%type <tydeclist> tydecs
%type <tydec> tydec_one
%type <fieldlist> tyfields tyfields_nonempty
%type <field> tyfield
%type <ty> ty
%type <fundeclist> fundecs
%type <fundec> fundec_one

%left PLUS MINUS
%left TIMES DIVIDE
%left OR
%left AND

%start program

%%
program:  exp  {absyn_tree_ = std::make_unique<absyn::AbsynTree>($1);};
pos: {$$ = scanner_.GetTokPos();};
/*
  lvalue:  ID  {$$ = new absyn::SimpleVar(scanner_.GetTokPos(), $1);}
  |  oneormore  {$$ = $1;}
  ;
*/

onevar:
    ID {$$ = new absyn::SimpleVar(scanner_.GetTokPos(), $1);}
  | onevar DOT ID {$$ = new absyn::FieldVar(scanner_.GetTokPos(), $1, $3);}  
  | ID subexp {
    auto var = new absyn::SimpleVar(scanner_.GetTokPos(), $1);
    $$ = new absyn::SubscriptVar(scanner_.GetTokPos(), var, $2);
  }
  | onevar subexp {
    $$ = new absyn::SubscriptVar(scanner_.GetTokPos(), $1, $2);
  }
  ;   
/************ exp begins here ******************/
subexp: LBRACK exp RBRACK {$$ = $2;};
seq_or_one_exp: LPAREN expseq RPAREN {$$ = $2;}
  | exp {$$ = $1;};
logicexp: exp EQ exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::EQ_OP, $1, $3);}
  | exp NEQ exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::NEQ_OP, $1, $3);}
  | exp LT exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::LT_OP, $1, $3);}
  | exp LE exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::LE_OP, $1, $3);}
  | exp GT exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::GT_OP, $1, $3);}
  | exp GE exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::GE_OP, $1, $3);}
  | logicexp AND logicexp {
    int pos = scanner_.GetTokPos();
    auto _if = $1;
    auto _then = $3;
    auto _else = new absyn::IntExp(pos, 0);
    $$ = new absyn::IfExp(pos, _if, _then, _else);  
  }
  | logicexp OR logicexp {
    int pos = scanner_.GetTokPos();
    auto _if = $1;
    auto _then = new absyn::IntExp(pos, 1);
    auto _else = $3;
    $$ = new absyn::IfExp(pos, _if, _then, _else);  
  }
  ;
exp: onevar {$$ = new absyn::VarExp(scanner_.GetTokPos(), $1);} 
  | NIL {$$ = new absyn::NilExp(scanner_.GetTokPos());}
  | INT {$$ = new absyn::IntExp(scanner_.GetTokPos(), $1);}
  | STRING {$$ = new absyn::StringExp(scanner_.GetTokPos(), $1);}
  | ID LPAREN sequencing_exps RPAREN {$$ = new absyn::CallExp(scanner_.GetTokPos(), $1, $3);}
  | ID LPAREN RPAREN {$$ = new absyn::CallExp(scanner_.GetTokPos(), $1, new absyn::ExpList());}
  | exp PLUS exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::PLUS_OP, $1, $3);}
  | exp MINUS exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::MINUS_OP, $1, $3);}
  | MINUS exp {
    int pos = scanner_.GetTokPos();
    $$ = new absyn::OpExp(pos, absyn::MINUS_OP, new absyn::IntExp(pos, 0), $2);
  }  
  | exp TIMES exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::TIMES_OP, $1, $3);}
  | exp DIVIDE exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::DIVIDE_OP, $1, $3);}
  | logicexp {$$ = $1;}
  | ID LBRACE recs RBRACE {$$ = new absyn::RecordExp(scanner_.GetTokPos(), $1, $3);}
  | ID LBRACE RBRACE {$$ = new absyn::RecordExp(scanner_.GetTokPos(), $1, new absyn::EFieldList);}
  | onevar ASSIGN exp {$$ = new absyn::AssignExp(scanner_.GetTokPos(), $1, $3);}
  | IF exp THEN seq_or_one_exp ELSE seq_or_one_exp {$$ = new absyn::IfExp(scanner_.GetTokPos(), $2, $4, $6);}
  | IF exp THEN seq_or_one_exp {$$ = new absyn::IfExp(scanner_.GetTokPos(), $2, $4, nullptr);}
  | WHILE exp DO seq_or_one_exp {$$ = new absyn::WhileExp(scanner_.GetTokPos(), $2, $4);}
  | FOR ID ASSIGN exp TO exp DO seq_or_one_exp {$$ = new absyn::ForExp(scanner_.GetTokPos(), $2, $4, $6, $8);}
  | BREAK {$$ = new absyn::BreakExp(scanner_.GetTokPos());}
  | expseq {$$ = $1;}
  | LET decs IN sequencing_exps END {
    auto seq = new absyn::SeqExp(scanner_.GetTokPos(), $4);
    $$ = new absyn::LetExp(scanner_.GetTokPos(), $2, seq);
  }
  | LET decs IN END {$$ = new absyn::LetExp(scanner_.GetTokPos(), $2, nullptr);}
  | ID subexp OF exp {$$ = new absyn::ArrayExp(scanner_.GetTokPos(), $1, $2, $4);}
  | LPAREN RPAREN {$$ = new absyn::VoidExp(scanner_.GetTokPos());}
  | LPAREN exp RPAREN {$$ = $2;}
  ;
  
expseq:
  LPAREN sequencing_exps RPAREN {$$ = new absyn::SeqExp(scanner_.GetTokPos(), $2);}
  ;
/************ dec starts here ******************/
dec: fundecs {$$ = new absyn::FunctionDec(scanner_.GetTokPos(), $1);}
  | VAR ID ASSIGN seq_or_one_exp {$$ = new absyn::VarDec(scanner_.GetTokPos(), $2, nullptr, $4);}
  | VAR ID COLON ID ASSIGN seq_or_one_exp {$$ = new absyn::VarDec(scanner_.GetTokPos(), $2, $4, $6);}
  | tydecs {$$ = new absyn::TypeDec(scanner_.GetTokPos(), $1);}
  ; 
  
/************ type starts here ******************/
ty: ID {$$ = new absyn::NameTy(scanner_.GetTokPos(), $1);}
  | LBRACE tyfields RBRACE {$$ = new absyn::RecordTy(scanner_.GetTokPos(), $2);}
  | ARRAY OF ID {$$ = new absyn::ArrayTy(scanner_.GetTokPos(), $3);} 
  ;   
  
/************ field starts here ******************/
tyfield: ID COLON ID {$$ = new absyn::Field(scanner_.GetTokPos(), $1, $3);};

/************ fieldlist starts here ******************/
tyfields: tyfield {$$ = new absyn::FieldList($1);}
  | tyfields COMMA tyfield {
    auto cpy = new absyn::FieldList($3);
    auto org_list = $1->GetList();
    for (auto it = org_list.rbegin(); it != org_list.rend(); ++it) {
      cpy = cpy->Prepend(*it);
    }
    $$ = cpy; 
  }
  ;
/************ explist starts here ******************/
sequencing_exps: exp {$$ = new absyn::ExpList($1);}
  | sequencing_exps SEMICOLON exp {
    auto cpy = new absyn::ExpList($3);
    auto org_list = $1->GetList();
    for (auto it = org_list.rbegin(); it != org_list.rend(); ++it) {
      cpy = cpy->Prepend(*it);
    }
    $$ = cpy;
  }
  | sequencing_exps COMMA exp {
    auto cpy = new absyn::ExpList($3);
    auto org_list = $1->GetList();
    for (auto it = org_list.rbegin(); it != org_list.rend(); ++it) {
      cpy = cpy->Prepend(*it);
    }
    $$ = cpy;
  }
  ;  

/************ fundec starts here ******************/
fundec_one: FUNCTION ID LPAREN tyfields RPAREN COLON ID EQ seq_or_one_exp 
  {$$ = new absyn::FunDec(scanner_.GetTokPos(), $2, $4, $7, $9);}
  | FUNCTION ID LPAREN RPAREN COLON ID EQ seq_or_one_exp 
  {$$ = new absyn::FunDec(scanner_.GetTokPos(), $2, new absyn::FieldList(), $6, $8);}  
  | FUNCTION ID LPAREN tyfields RPAREN EQ seq_or_one_exp 
  {$$ = new absyn::FunDec(scanner_.GetTokPos(), $2, $4, nullptr, $7);}
  | FUNCTION ID LPAREN RPAREN EQ seq_or_one_exp
  {$$ = new absyn::FunDec(scanner_.GetTokPos(), $2, new absyn::FieldList(), nullptr, $6);}
  ;

/************ fundeclist starts here ******************/ 
fundecs: fundec_one {$$ = new absyn::FunDecList($1);}
  | fundecs fundec_one {
    auto cpy = new absyn::FunDecList($2);
    auto org_list = $1->GetList();
    for (auto it = org_list.rbegin(); it != org_list.rend(); ++it) {
      cpy = cpy->Prepend(*it);
    }
    $$ = cpy;
  }
  ;

/************ declist starts here ******************/
decs: dec {$$ = new absyn::DecList($1);}
  | decs dec {
    auto cpy = new absyn::DecList($2);
    auto org_list = $1->GetList();
    for (auto it = org_list.rbegin(); it != org_list.rend(); ++it) {
      cpy = cpy->Prepend(*it);
    }
    $$ = cpy;
  }
  ;

/************ nameAndTy starts here ******************/
tydec_one: TYPE ID EQ ty {$$ = new absyn::NameAndTy($2, $4);};

/************ nameAndTyList starts here ******************/
tydecs: tydec_one {$$ = new absyn::NameAndTyList($1);} 
  | tydecs tydec_one {
    auto cpy = new absyn::NameAndTyList($2);
    auto org_list = $1->GetList();
    for (auto it = org_list.rbegin(); it != org_list.rend(); ++it) {
      cpy = cpy->Prepend(*it);
    }
    $$ = cpy;
  } 
  ;

/************ efield starts here ******************/  
rec: ID EQ exp {$$ = new absyn::EField($1, $3);};

/************ efieldlist starts here ******************/
recs: rec {$$ = new absyn::EFieldList($1);}
  | recs COMMA rec {
    auto cpy = new absyn::EFieldList($3);
    auto org_list = $1->GetList();
    for (auto it = org_list.rbegin(); it != org_list.rend(); ++it) {
      cpy = cpy->Prepend(*it);
    }
    $$ = cpy;
  } 
  ;  
 /* TODO: Put your lab3 code here */
