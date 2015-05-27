HEADERS := $(wildcard ../include/android/*.h)

all: html jd

html: $(HEADERS) Doxyfile
	mkdir -p html
	doxygen

jd: $(HEADERS) Doxyfile header.jd
	mkdir -p jd
	HTML_HEADER=header.jd HTML_FOOTER=footer.jd HTML_OUTPUT=jd doxygen
	for file in jd/*.html; do mv "$${file}" "$${file/.html/.jd}"; done
	rm -f jd/index.jd
