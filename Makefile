CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -D_GNU_SOURCE
INCLUDES = -Iinclude -I/usr/include/freerdp3 -I/usr/include/winpr3
LDFLAGS = -lfreerdp3 -lfreerdp-client3 -lwinpr3 -lpng

SRCDIR = src
INCDIR = include
BUILDDIR = build
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
TARGET = $(BUILDDIR)/bin/rcrdp

.PHONY: all clean install test test-build

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BUILDDIR)/bin
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/bin:
	mkdir -p $(BUILDDIR)/bin

$(BUILDDIR)/tests:
	mkdir -p $(BUILDDIR)/tests

clean:
	rm -rf $(BUILDDIR)

install: $(TARGET)
	install -D $(TARGET) /usr/local/bin/rcrdp

# Test targets
test-build: | $(BUILDDIR)/tests
	@if [ ! -f .env ]; then \
		echo "Error: .env file not found. Copy .env.example to .env and configure test settings."; \
		echo "Required variables: RCRDP_TEST_HOST, RCRDP_TEST_USER, RCRDP_TEST_PASS"; \
		exit 1; \
	fi
	$(CC) $(CFLAGS) $(INCLUDES) -o $(BUILDDIR)/tests/test_connection \
		tests/test_connection.c $(SRCDIR)/rdp_client.c $(SRCDIR)/commands.c \
		$(LDFLAGS)

test: test-build
	@echo "Loading test configuration from .env..."
	@set -a && . ./.env && set +a && \
	if [ -z "$$RCRDP_TEST_HOST" ] || [ -z "$$RCRDP_TEST_USER" ] || [ -z "$$RCRDP_TEST_PASS" ]; then \
		echo "Error: Missing required environment variables"; \
		echo "Please set RCRDP_TEST_HOST, RCRDP_TEST_USER, and RCRDP_TEST_PASS in .env file"; \
		exit 1; \
	fi && \
	echo "Test configuration:" && \
	echo "  Host: $$RCRDP_TEST_HOST" && \
	echo "  Port: $${RCRDP_TEST_PORT:-3389}" && \
	echo "  User: $$RCRDP_TEST_USER" && \
	echo "  Domain: $${RCRDP_TEST_DOMAIN:-<none>}" && \
	echo "" && \
	echo "Running RDP connection tests..." && \
	cd $(BUILDDIR)/tests && \
	set -a && . ../../.env && set +a && \
	./test_connection

# Dependencies
$(BUILDDIR)/main.o: $(INCDIR)/rcrdp.h $(INCDIR)/http_server.h
$(BUILDDIR)/rdp_client.o: $(INCDIR)/rcrdp.h
$(BUILDDIR)/commands.o: $(INCDIR)/rcrdp.h
$(BUILDDIR)/http_server.o: $(INCDIR)/http_server.h $(INCDIR)/rcrdp.h
$(BUILDDIR)/http_routes.o: $(INCDIR)/http_server.h $(INCDIR)/rcrdp.h