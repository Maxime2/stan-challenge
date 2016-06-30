APXS=apxs
CC=gcc

all: mod_stan.la

install: all
	@sudo $(APXS) -i -a -n stan mod_stan.la

mod_stan.la: mod_stan.c 
	@$(APXS) -c -I./ -lyajl -o $@ $^ --shared

.PHONY: clean

clean:
	rm -rf *.o *~ *.la *.lo *.slo .libs
