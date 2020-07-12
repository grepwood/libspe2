

images = singlemini.pdf pthreadmini.pdf event1.pdf
all: $(images)


$(images): %.pdf: %.dot
	dot -Tps $< | epstopdf -f -o $@


clean:
	rm $(images)