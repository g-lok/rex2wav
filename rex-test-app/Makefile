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
# Using the SDK from the repository
REX_ROOT       := ../SDKs/REXSDK_Mac_1.9.2
FRAMEWORK_DIR  := $(REX_ROOT)/Mac/Deployment

#
# Build Configuration
# -------------------
# By default, builds universal binary (x86_64 + arm64)
# - Universal binary: make (default)
# - Native only:      make ARCHFLAGS="-arch $(shell uname -m)"
#
CC = clang
HOST_ARCH := $(shell uname -m)
ARCHFLAGS ?= -arch x86_64 -arch arm64
CFLAGS = -Wall -Wextra -std=c11 -g $(ARCHFLAGS)
# CXX      := g++
# CXXFLAGS := -std=c++11 \
#             -DREX_MAC=1 -DREX_WINDOWS=0 \
#             -I$(REX_ROOT) \
#             -O2
LDFLAGS  = $(ARCHFLAGS) \
            -F$(FRAMEWORK_DIR) \
            -framework "REX Shared Library" \
            -framework CoreFoundation \
            -Wl,-rpath,@executable_path
SRCS = rex2wav.c REX.c Wav.c argparse.c 
# SRCDIR   := src
OBJS = $(SRCS:.c=.o)
# OBJDIR   := obj
# BINDIR   := bin
TARGET   = rex2wav
FRAMEWORK_NAME = REX Shared Library.framework
BUNDLE_NAME = REX Shared Library.bundle

.PHONY: all clean universal

all: $(TARGET)
	@echo ""
	@echo "✓ Build complete! Run with: ./$(TARGET)"

universal:
	@$(MAKE) clean
	@$(MAKE) ARCHFLAGS="-arch x86_64 -arch arm64"

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(TARGET)
	@echo "Embedding REX library for standalone use..."
	@rm -rf "$(FRAMEWORK_NAME)" "$(BUNDLE_NAME)"
	@cp -R "$(FRAMEWORK_DIR)/$(FRAMEWORK_NAME)" .
	@cp -R "$(FRAMEWORK_DIR)/$(BUNDLE_NAME)" .
	@install_name_tool -change \
		"@loader_path/../Frameworks/REX Shared Library.framework/Versions/A/REX Shared Library" \
		"@executable_path/REX Shared Library.framework/Versions/A/REX Shared Library" \
		$(TARGET)
	@echo "REX library embedded alongside executable."

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf "$(FRAMEWORK_NAME)" "$(BUNDLE_NAME)"
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
