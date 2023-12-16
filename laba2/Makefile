ifdef OS
	RM = del /Q
else
	RM = rm -f
endif

all: hw
hw: hw.o func.o
	g++ -g hw.o func.o -o hw
hw.o:
	g++ -c -Wall -g hw.cpp
func.o:
	g++ -c -Wall -g func.cpp

clear:
	$(RM) *.o
	$(RM) *.exe