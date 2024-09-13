 # My third makefile
 
# Name of the project
PROJ_NAME=tests
# .c files
#test app files
C_SOURCE=$(wildcard ./sources/*.cpp)

#main app files
C_SOURCE+=$(wildcard ../sources/*.cpp)
C_SOURCE+=$(wildcard ../sources/ThreadPool/*.cpp)
 
# .h files
#test app files
H_SOURCE=$(wildcard ./sources/*.h)

#main app files
H_SOURCE+=$(wildcard ../sources/*.h)
H_SOURCE+=$(wildcard ../sources/ThreadPool/*.h)

objFolder:
	@ mkdir -p objects/tests
#	the main app directories
	@ mkdir -p objects/main
	@ mkdir -p objects/main/ThreadPool
	
	

prebuild:
# 	prepares the folder built/gui. This folder contains files copied from GUI/resources. These files contains the HTML5 User interface.
	@ mkdir ./build | true
	@ cp -r ./sources/copyToBuildFolder/* ./build | true
 
# Object files
OBJ=$(subst .cpp,.o,$(subst ./sources,./objects/tests,$(subst ../sources,./objects/main,$(C_SOURCE))))
 
# Compiler and linker
CC=g++
 
# Flags for compiler
CC_FLAGS=-c			\
		 -W         \
         -Wall      \
         -ansi      \
         -pedantic  \
		 -pthread   \
		 -g			\
		 -D__TESTING__ \
		 -std=c++11 \
		 -I"./sources/" \
		 -I"../sources/ThreadPool" \
		 -lssl \
		 -lcrypto \

# Flags for linker
LK_FLAGS=-W         \
         -Wall      \
         -ansi      \
         -pedantic  \
		 -pthread   \
		 -g			\
		 -D__TESTING__ \
		 -std=c++11 \
		 -I"./sources/" \
		 -I"../sources/ThreadPool" \
		 -lssl \
		 -lcrypto \
 
# Command used at clean target
RM = rm -rf
 
#
# Compilation and linking
#
all: objFolder prebuild $(PROJ_NAME)
 
$(PROJ_NAME): $(OBJ)
	@ echo 'Building binary using GCC linker: $@'
	$(CC) $^ $(LK_FLAGS) -o build/$@
	@ echo 'Finished building binary: $@'
	@ echo ' '
 
./objects/main/%.o: ../sources/%.cpp ../sources/%.h
	@ echo 'Building target using GCC compiler: $<'
	$(CC) $< $(CC_FLAGS) -o $@
	@ echo ' '

./objects/tests/%.o: ./sources/%.cpp ./sources/%.h
	@ echo 'Building target using GCC compiler: $<'
	$(CC) $< $(CC_FLAGS) -o $@
	@ echo ' '
 
./objects/tests/main.o: ./sources/main.cpp $(H_SOURCE)
	@ echo 'Building target using GCC compiler: $<'
	$(CC) $< $(CC_FLAGS) -o $@
	@ echo ' '
	
clean:
	@ $(RM) ./objects/*.o $(PROJ_NAME) *~
	@ rm -rf objects
 
.PHONY: all clean