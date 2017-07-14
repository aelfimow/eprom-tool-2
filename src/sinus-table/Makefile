CXXFLAGS = -W 
CXXFLAGS += -Werror 
CXXFLAGS += -Wall 
CXXFLAGS += -Wextra 
CXXFLAGS += -Wabi 
CXXFLAGS += -Wreorder 
CXXFLAGS += -Weffc++ 
CXXFLAGS += -Wold-style-cast 
CXXFLAGS += -Wsign-promo 
CXXFLAGS += -Wformat-nonliteral 
CXXFLAGS += -Wformat-security 
CXXFLAGS += -Wformat=2 
CXXFLAGS += -Winit-self 
CXXFLAGS += -Wuninitialized 
CXXFLAGS += -Wswitch 
CXXFLAGS += -Wswitch-default 
CXXFLAGS += -Wswitch-enum 
CXXFLAGS += -Wunused-parameter 
CXXFLAGS += -Wunused 
CXXFLAGS += -Wunknown-pragmas 
CXXFLAGS += -Wfloat-equal 
CXXFLAGS += -Wundef 
CXXFLAGS += -Wshadow 
CXXFLAGS += -Wpointer-arith 
CXXFLAGS += -Wcast-qual 
CXXFLAGS += -Wcast-align 
CXXFLAGS += -Wwrite-strings 
CXXFLAGS += -Wconversion 
CXXFLAGS += -Waggregate-return 
CXXFLAGS += -Wpacked 
CXXFLAGS += -Wpadded 
CXXFLAGS += -Wredundant-decls 
CXXFLAGS += -Winvalid-pch 

all:
	mingw32-gcc -c main.cpp -o main.o $(CXXFLAGS) -O3
	mingw32-gcc main.o -o main.exe $(CXXFLAGS)
	@echo
	@echo No errors.
	@echo

