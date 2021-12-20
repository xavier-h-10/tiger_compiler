%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */
 /* TODO: Put your lab2 code here */

%x COMMENT STR IGNORE
%%


 /*
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */

 /* reserved words */
"array" {adjust(); return Parser::ARRAY;}
"if" {adjust(); return Parser::IF;}
"then" {adjust(); return Parser::THEN;}
"else" {adjust(); return Parser::ELSE;}
"while" {adjust(); return Parser::WHILE;}
"for" {adjust(); return Parser::FOR;}
"to" {adjust(); return Parser::TO;}
"do" {adjust(); return Parser::DO;}
"let" {adjust(); return Parser::LET;}
"in" {adjust(); return Parser::IN;}
"end" {adjust(); return Parser::END;}
"of" {adjust(); return Parser::OF;}
"break" {adjust(); return Parser::BREAK;}
"nil" {adjust(); return Parser::NIL;}
"function" {adjust(); return Parser::FUNCTION;}
"var" {adjust(); return Parser::VAR;}
"type" {adjust(); return Parser::TYPE;}
 /* TODO: Put your lab2 code here */
\" { 
  more();
  errormsg_->tok_pos_ = char_pos_;
  begin(StartCondition__::STR); 
  string_buf_.clear();
}
<STR>\\[0-9]{3} {
  int len = length();
  std::string ddd = matched().substr(len - 3, 3);
  char ch = (char) atoi(ddd.c_str());
  adjustStr(ch);
}
<STR>\\\\ adjustStr('\\'); 
<STR>\\t adjustStr('\t');
<STR>\\n adjustStr('\n');
<STR>\\r adjustStr('\r');
<STR>\\\" adjustStr('\"');
<STR>\\\^@ adjustStr('\0');
<STR>\\\^[A-Z] adjustStr(*(matched().rbegin()) - 'A' + 1);
<STR>\\\^\[ adjustStr(3);
<STR>\\ {
  begin(StartCondition__::IGNORE); 
  more();
}
<IGNORE>\\ {
  more();
  begin(StartCondition__::STR); 
}
<IGNORE>.|\n more();
<IGNORE><<EOF>> {
  ++char_pos_; //skip "
  begin(StartCondition__::INITIAL);
  errormsg_->Error(errormsg_->tok_pos_, "illegal ignore sequence");
}
<STR>\" { 
  char_pos_ += length();
  setMatched(string_buf_);
  begin(StartCondition__::INITIAL);
  return Parser::STRING;
}
<STR><<EOF>> {
  ++char_pos_; //skip "
  begin(StartCondition__::INITIAL);
  errormsg_->Error(errormsg_->tok_pos_, "illegal string");
}
<STR>. adjustStr(*(matched().rbegin()));
[a-zA-Z][_a-zA-Z0-9]* {adjust(); return Parser::ID;}
[0-9]+ {adjust(); return Parser::INT;}
\( {adjust(); return Parser::LPAREN;}
\) {adjust(); return Parser::RPAREN;}
\[ {adjust(); return Parser::LBRACK;}
\] {adjust(); return Parser::RBRACK;}
\{ {adjust(); return Parser::LBRACE;}
\} {adjust(); return Parser::RBRACE;}
:= {adjust(); return Parser::ASSIGN;}
: {adjust(); return Parser::COLON;}
\<= {adjust(); return Parser::LE;}
\>= {adjust(); return Parser::GE;}
= {adjust(); return Parser::EQ;}
& {adjust(); return Parser::AND;}
\| {adjust(); return Parser::OR;}
, {adjust(); return Parser::COMMA;}
; {adjust(); return Parser::SEMICOLON;}
\. {adjust(); return Parser::DOT;}
\<\> {adjust(); return Parser::NEQ;}
\< {adjust(); return Parser::LT;}
\> {adjust(); return Parser::GT;}
\/\* {
  more();
  comment_level_ = 1;
  begin(StartCondition__::COMMENT);
}
<COMMENT>\/\* {
  more();
  ++comment_level_;
} 
<COMMENT>\*\/ {
  --comment_level_;
  if (comment_level_ == 0) {
    adjust();
    begin(StartCondition__::INITIAL);
  }
  else
    more();
} 
<COMMENT><<EOF>> {
  char_pos_ += 2; //skip /*
  begin(StartCondition__::INITIAL);
  errormsg_->Error(errormsg_->tok_pos_, "illegal comment");
}
<COMMENT>\n|. more();
\+ {adjust(); return Parser::PLUS;}
- {adjust(); return Parser::MINUS;}
\* {adjust(); return Parser::TIMES;}
\/ {adjust(); return Parser::DIVIDE;}
 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}

 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
