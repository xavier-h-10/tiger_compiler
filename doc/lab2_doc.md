# Document for lab 2

### How I handle comments

Matching comments in tiger is very similar to parenthesis matching. To deal with nested comments, I make use of the global variable in class Scanner called 'comment_level_'. And I also make use of the mini scanner provided by flex++.

 By the way, mini scanner is a very powerful and interesting feature in flex++, which allow the scanner to jump from one automata to another. (So it's not a traditional automata)

The lex code is as follow, let take a look on it and see how it works.

```c++
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
```

When the scanner reads /\*, then it will jump to the start state of COMMENT(using the feature of mini scanner). The scanner will switch its matching rules to those prefixed with <COMMENT> . Then initialize the comment level to 1, which means there are one /* to be matched by \*/. Yes, you might see, it just like doing the parenthesis matching with a stack. (But for comment, we don't need to store info, simple count will satisfy our needs, so I haven't used a stack)

Once the scanner reads /\*, increase the comment level by 1. 

Once the scanner reads \*/, decrease the comment level by 0. If the comment level has been decreased to **ZERO**,  that means the comment are matched.

Notice that I have used the function **more()** provided by flex++. It is used to prefix to the next matched token.

### How I handle strings

strings in tiger have many interesting features. 

#### Escape sequence

+ \n, \t: 

  linefeed, tab 

+ \ddd: 

  ddd is a 3-octal number (Transfer ascii to char, where ddd's value is ascii)

+ \”, \\: “, \

+ \\f\_\_\_f\\: 

  this sequence is ignored, where f___f stands for sequence of one or more formatted characters including at least space, tab, or newline, form feed

+ \^c 
  + ^@ (0, null, NUL, \0),  mark the end of a string
  + ^G (7, bell, BEL, \a), emit a warning of some kind
  + ...
  + ^[ (27, escape, ESC, \e (GCC only)). Introduces an escape sequence
  
  **\\^A ~ \\^Z are mapped to ascii 1 ~ 26**

To satisfy features mentioned above, I design the lex code:

```c++
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
```

It's very similar to the implement of comment matching. Except for in string matching, I use two mini scanner. (With IGNORE nested in STR, to handle the ignore sequence.)

To handle "\\ddd"

```c++
<STR>\\[0-9]{3} 
```

To handle "\\t", "\\n" ... and "\\^c"

```c++
<STR>\\\\ adjustStr('\\'); 
<STR>\\t adjustStr('\t');
<STR>\\n adjustStr('\n');
<STR>\\r adjustStr('\r');
<STR>\\\" adjustStr('\"');
<STR>\\\^@ adjustStr('\0');
<STR>\\\^[A-Z] adjustStr(*(matched().rbegin()) - 'A' + 1);
<STR>\\\^\[ adjustStr(3);
```

Once the scanner reads "\\",  (and is not matched by \\r, \t, etc.), it will jump to the mini scanner IGNORE to handle ignore sequence.

Once the scanner reads another "\\", matches ignore sequence and jumps back to the mini scanner STR.

Once the scanner reads another \", it will match the string and jump back to initial scanner.

Notice that I use the string_buf_ to store the string what we actually need(escape sequence should be transformed, and ignore sequence should be correctly handled.) By calling function **setMatched(cont std::string &)**, we can set the matched string.

**adjustStr(char ch)** will adjust the string_buf_:

```c++
inline void Scanner::adjustStr(char ch) {
  more();
  string_buf_ += ch;
}
```

### Error handling

If the current lex is rejected by all matching rules of valid token, then it would raise an error, which will call **errormsg_->Error(...)** to report the error:

```c++
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
```

(The dot refers to any character except for the \\n)

When trying to match a string, the scanner has to scan to the end until it has found another double quote. If not, which means the scanner has reached the EOF, the scanner would also raise an error and skip the first double quote by simply \"++char_pos_\" :

```c++
<STR><<EOF>> {
  ++char_pos_; //skip "
  begin(StartCondition__::INITIAL);
  errormsg_->Error(errormsg_->tok_pos_, "illegal string");
}
```

For ignore sequence, it is similar.

For comment, if the comment level hasn't been decreased to **ZERO**, it will raise an error and skip the "/*", still similar to string matching.

```c++
<COMMENT><<EOF>> {
  char_pos_ += 2; //skip /*
  begin(StartCondition__::INITIAL);
  errormsg_->Error(errormsg_->tok_pos_, "illegal comment");
}
```

### End-of-file handling

Flex++ offers a regular expression <<EOF>> for user to match the end-of-file.

If an EOF has been read in comment or string matching, it means there is no possibility to match, for there are no characters left. So, by handling EOF, we can know whether the match has succeeded or not. 

The way to handle EOF has just been shown above.

###  Other interesting features of your lexer

Still under development.