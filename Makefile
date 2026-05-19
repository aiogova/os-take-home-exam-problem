CXX= g++
CXXFLAGS= -std=c++17

INCLUDE=
LIB= -lSDL2 -lSDL2_ttf

SRCDIR= src
OBJDIR= obj
BINDIR= bin

OBJS= $(addprefix $(OBJDIR)/, taskmanager.o)
EXEC= $(addprefix $(BINDIR)/, taskmanager)

# CREATE DIRECTORIES (IF THEY DON'T EXIST)
mkdirs := $(shell mkdir -p $(OBJDIR) $(BINDIR))

# BUILD EVERYTHING
all: $(EXEC)

$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIB)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INCLUDE)

# REMOVE OLD FILES
clean:
	rm -f $(OBJS) $(EXEC)