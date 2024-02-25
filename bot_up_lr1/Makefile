CC = clang

SRC = ${filter-out ${wildcard *.yy.*},${wildcard *.c}}
HDR = ${wildcard *.h}

%.yy.c: %.flex
	flex -o $@ $<

parser: lexer.yy.c $(SRC) $(HDR)
	$(CC) lexer.yy.c $(SRC) -o $@

