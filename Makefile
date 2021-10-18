xjot-to-xml: xjot_to_xml.c
	gcc -o $@ $^

.PHONY: loop
loop:
	fswatch --event Updated --event Created -0 xjot_to_xml.c | xargs -0 -n 1 make xjot-to-xml
