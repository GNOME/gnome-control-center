introdir = $(datadir)/control-center/intro_page
intro_DATA = intro.xml faq.html

artdir = $(datadir)/pixmaps/gnomecc
art_DATA = background.png

intro.xml: intro.xml.in
	sed -e "s#@FAQDIR@#$(introdir)#" < $< > $@
