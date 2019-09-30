
#CFLAGS="-O3 -Wall -std=c++0x"
#CFLAGS="-O0 -Wall -std=c++0x"
#CFLAGS="-O0 -pg -Wall"

CFLAGS="-O0 -Wall"
#CFLAGS="-O0 -Wall -UNDEBUG"
CFLAGS="-O3 -Wall"

sources = lm.cpp lm_dynamic.cpp lm_python.cpp lm_merged.cpp pool_allocator.cpp
headers = lm.h lm_dynamic.h lm_dynamic_impl.h lm_dynamic_kn.h \
          lm_dynamic_cached.h lm_merged.h


lm.so : $(sources) $(headers)
	CFLAGS=$(CFLAGS) python setup.py build
	cp ./build/lib.*/lm.so .

Debug : lm.so

all : clean lm.so

.PHONY : clean
clean :
	python setup.py clean
	-rm lm.so
	-rm -rf ./build

install:
	python setup.py install

