OBJECTS  := main.o bogomips.o
GEN_DIR   = $(LINUX_DIR)/..

cpu_bench: $(OBJECTS)
	g++ $(OBJECTS) -o $@

main.o: $(LINUX_DIR)/main.cc $(GEN_DIR)/bogomips.h
	g++ -c -I$(GEN_DIR) -O2 -Wall -Wextra -Weffc++ -std=gnu++11 $< -o $@

bogomips.o: ../bogomips.s
	gcc -c -O2 -Wall -Wextra $< -o $@

clean:
	rm -f *~ cpu_bench bogomips.o main.o
