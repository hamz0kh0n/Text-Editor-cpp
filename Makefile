CXX = g++
FLAGS = -std=c++11 -Wall -Wextra

all: main test_editor
main: main.cpp editor.h
	$(CXX) $(FLAGS) main.cpp -o main
test_editor: test_editor.cpp editor.h
	$(CXX) $(FLAGS) test_editor.cpp -o test_editor
test: test_editor
	./test_editor
clean:
	rm -f main test_editor *.exe *.o
