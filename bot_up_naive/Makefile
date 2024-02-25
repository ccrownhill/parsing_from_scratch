%.yy.c: %.flex
	flex -o $@ $<

parser: lexer.yy.c parser.c parser.h
	gcc lexer.yy.c parser.c -o parser
