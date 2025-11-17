# CC = clang
# CFLAGS = -Wall -Wextra -std=c11 -g -v
# LDFLAGS = -framework "REX Shared Library"
# TARGET = rextest-compiled
# SRCS = REXTest.c REX.c WAV.c
# OBJS = $(SRCS:.c=.o)
# all: $TARGET
# $(TARGET): $(OBJS)
# 	$(CC) $(OBJS) $(LDFLAGS) -o $@
#
# %.o: %.c
# 	$(CC) $(CFLAGS) -c $< -o $@
#
# clean:
# 	rm -f $(OBJS) $(TARGET)

# — point this at your SDK root —
REX_ROOT       := $(HOME)/SDKs/REXSDK_Mac_1.9.2
FRAMEWORK_DIR  := $(REX_ROOT)/Mac/Deployment

CC = clang
CFLAGS = -Wall -Wextra -std=c11 -g
# CXX      := g++
# CXXFLAGS := -std=c++11 \
#             -DREX_MAC=1 -DREX_WINDOWS=0 \
#             -I$(REX_ROOT) \
#             -O2
LDFLAGS  = -F$(FRAMEWORK_DIR) \
            -framework "REX Shared Library"
SRCS = REXTest.c REX.c WAV.c 
# SRCDIR   := src
OBJS = $(SRCS:.c=.o)
# OBJDIR   := obj
# BINDIR   := bin
TARGET   = rex2wav

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
# SOURCES  := $(SRCDIR)/main.cpp
# OBJECTS  := $(OBJDIR)/main.o
#
# .PHONY: all clean
#
# all: $(BINDIR)/$(TARGET)

# link step
# $(BINDIR)/$(TARGET): $(OBJECTS) | $(BINDIR)
# 	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)
#
# # compile step
# $(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
# 	$(CXX) $(CXXFLAGS) -c $< -o $@
#
# # ensure build dirs exist
# $(OBJDIR):
# 	mkdir -p $(OBJDIR)
#
# $(BINDIR):
# 	mkdir -p $(BINDIR)
#
# clean:
# 	rm -rf $(OBJDIR) $(BINDIR)
