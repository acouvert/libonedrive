ROOT_DIR  = $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
SRC_DIR   = $(ROOT_DIR)/src
BUILD_DIR = $(ROOT_DIR)/build

CC       ?= cc
CFLAGS    = -std=c99 -pedantic -Wall -Wextra -Wno-unused-parameter -g -O2 -D_POSIX_C_SOURCE=200809L
LIBAUDIOTAG_DIR = $(ROOT_DIR)/lib/libaudiotag
INC       = -I$(SRC_DIR) -I$(LIBAUDIOTAG_DIR)/bin/include $(shell pkg-config --cflags libcurl 2>/dev/null)
LDLIBS    = $(shell pkg-config --libs libcurl 2>/dev/null || echo "-lcurl") -lcjson

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))
PIC_OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/pic/%.o, $(SRC))

STATIC_LIB = $(BUILD_DIR)/libonedrive.a
SHARED_LIB = $(BUILD_DIR)/libonedrive.so

SAMPLE_SRC = $(wildcard $(ROOT_DIR)/sample/*.c)
SAMPLE_BIN = $(patsubst $(ROOT_DIR)/sample/%.c, $(BUILD_DIR)/sample/%, $(SAMPLE_SRC))

TEST_SRC = $(wildcard $(ROOT_DIR)/test/test_*.c)
TEST_DIR = $(ROOT_DIR)/test

COV_DIR = $(BUILD_DIR)/coverage

all: $(STATIC_LIB) $(SHARED_LIB) $(SAMPLE_BIN)

# Static library
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "[C] $(notdir $<)"
	@mkdir -p $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $(INC) -o $@ $<

$(STATIC_LIB): $(OBJ)
	@echo "[AR] $(notdir $@)"
	$(AR) rcs $@ $^

# Shared library
$(BUILD_DIR)/pic/%.o: $(SRC_DIR)/%.c
	@echo "[C-PIC] $(notdir $<)"
	@mkdir -p $(BUILD_DIR)/pic
	$(CC) -c $(CFLAGS) -fPIC -fvisibility=hidden $(INC) -o $@ $<

$(SHARED_LIB): $(PIC_OBJ)
	@echo "[SO] $(notdir $@)"
	$(CC) -shared $(CFLAGS) -o $@ $^ $(LDLIBS)

# Sample programs
$(BUILD_DIR)/sample/%: $(ROOT_DIR)/sample/%.c $(STATIC_LIB)
	@echo "[SAMPLE] $(notdir $@)"
	@mkdir -p $(BUILD_DIR)/sample
	$(CC) $(CFLAGS) $(INC) -o $@ $< $(STATIC_LIB) $(LDLIBS)

# Source files for mock-linked tests (exclude real http_client)
# Source files for tests (exclude onedrive_music.c which depends on libaudiotag)
TEST_SRC     = $(filter-out $(SRC_DIR)/onedrive_music.c, $(SRC))
MOCK_LIB_SRC = $(filter-out $(SRC_DIR)/http_client.c, $(TEST_SRC))
TEST_INC     = $(INC) -I$(ROOT_DIR)/test

# --- test_buffer: standalone, no external deps ---
$(BUILD_DIR)/test/test_buffer: $(TEST_DIR)/test_buffer.c $(SRC_DIR)/buffer.c
	@echo "[TEST] test_buffer"
	@mkdir -p $(BUILD_DIR)/test
	$(CC) $(CFLAGS) $(INC) -o $@ $(TEST_DIR)/test_buffer.c $(SRC_DIR)/buffer.c

# --- test_http_client: links real http_client.c + curl ---
$(BUILD_DIR)/test/test_http_client: $(TEST_DIR)/test_http_client.c $(TEST_SRC)
	@echo "[TEST] test_http_client"
	@mkdir -p $(BUILD_DIR)/test
	$(CC) $(CFLAGS) $(INC) -o $@ $(TEST_DIR)/test_http_client.c $(TEST_SRC) $(LDLIBS)

# --- test_json_utils: links mock_http_client + json_utils ---
$(BUILD_DIR)/test/test_json_utils: $(TEST_DIR)/test_json_utils.c $(TEST_DIR)/mock_http_client.c $(MOCK_LIB_SRC)
	@echo "[TEST] test_json_utils"
	@mkdir -p $(BUILD_DIR)/test
	$(CC) $(CFLAGS) $(TEST_INC) -o $@ $(TEST_DIR)/test_json_utils.c $(TEST_DIR)/mock_http_client.c $(MOCK_LIB_SRC) -lcjson

# --- test_onedrive: links mock_http_client + onedrive + http_utils ---
$(BUILD_DIR)/test/test_onedrive: $(TEST_DIR)/test_onedrive.c $(TEST_DIR)/mock_http_client.c $(MOCK_LIB_SRC)
	@echo "[TEST] test_onedrive"
	@mkdir -p $(BUILD_DIR)/test
	$(CC) $(CFLAGS) $(TEST_INC) -o $@ $(TEST_DIR)/test_onedrive.c $(TEST_DIR)/mock_http_client.c $(MOCK_LIB_SRC) -lcjson

# --- test_onedrive_music: links mock_http_client + onedrive_music + mock audio ---
MUSIC_TEST_SRC = $(filter-out $(SRC_DIR)/http_client.c, $(SRC))
$(BUILD_DIR)/test/test_onedrive_music: $(TEST_DIR)/test_onedrive_music.c $(TEST_DIR)/mock_http_client.c $(MUSIC_TEST_SRC)
	@echo "[TEST] test_onedrive_music"
	@mkdir -p $(BUILD_DIR)/test
	$(CC) $(CFLAGS) $(TEST_INC) -o $@ $(TEST_DIR)/test_onedrive_music.c $(TEST_DIR)/mock_http_client.c $(MUSIC_TEST_SRC) -lcjson

TEST_BINS = $(BUILD_DIR)/test/test_buffer \
            $(BUILD_DIR)/test/test_http_client \
            $(BUILD_DIR)/test/test_json_utils \
            $(BUILD_DIR)/test/test_onedrive \
            $(BUILD_DIR)/test/test_onedrive_music

test: $(TEST_BINS)
	@failed=0; \
	for bin in $(TEST_BINS); do \
		name=$$(basename $$bin); \
		echo "=== $$name ==="; \
		$$bin; \
		rc=$$?; \
		if [ $$rc -ne 0 ]; then failed=$$((failed + rc)); fi; \
	done; \
	exit $$failed

# Coverage
COV_CFLAGS = $(subst -O2,-O0,$(CFLAGS)) --coverage

# Compile each source file to a coverage .o once so .gcno files are shared
COV_OBJ_DIR = $(COV_DIR)/obj
COV_SRC_OBJ = $(patsubst $(SRC_DIR)/%.c, $(COV_OBJ_DIR)/%.o, $(TEST_SRC))
COV_ALL_SRC_OBJ = $(patsubst $(SRC_DIR)/%.c, $(COV_OBJ_DIR)/%.o, $(SRC))
COV_MOCK_OBJ = $(COV_OBJ_DIR)/mock_http_client.o
COV_MOCK_LIB_OBJ = $(filter-out $(COV_OBJ_DIR)/http_client.o, $(COV_SRC_OBJ))
COV_MUSIC_LIB_OBJ = $(filter-out $(COV_OBJ_DIR)/http_client.o, $(COV_ALL_SRC_OBJ))

$(COV_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(COV_OBJ_DIR)
	$(CC) -c $(COV_CFLAGS) $(INC) -o $@ $<

$(COV_OBJ_DIR)/mock_http_client.o: $(TEST_DIR)/mock_http_client.c
	@mkdir -p $(COV_OBJ_DIR)
	$(CC) -c $(COV_CFLAGS) $(TEST_INC) -o $@ $<

$(COV_OBJ_DIR)/test_buffer.o: $(TEST_DIR)/test_buffer.c
	@mkdir -p $(COV_OBJ_DIR)
	$(CC) -c $(COV_CFLAGS) $(INC) -o $@ $<

$(COV_OBJ_DIR)/test_http_client.o: $(TEST_DIR)/test_http_client.c
	@mkdir -p $(COV_OBJ_DIR)
	$(CC) -c $(COV_CFLAGS) $(INC) -o $@ $<

$(COV_OBJ_DIR)/test_json_utils.o: $(TEST_DIR)/test_json_utils.c
	@mkdir -p $(COV_OBJ_DIR)
	$(CC) -c $(COV_CFLAGS) $(TEST_INC) -o $@ $<

$(COV_OBJ_DIR)/test_onedrive.o: $(TEST_DIR)/test_onedrive.c
	@mkdir -p $(COV_OBJ_DIR)
	$(CC) -c $(COV_CFLAGS) $(TEST_INC) -o $@ $<

$(COV_OBJ_DIR)/test_onedrive_music.o: $(TEST_DIR)/test_onedrive_music.c
	@mkdir -p $(COV_OBJ_DIR)
	$(CC) -c $(COV_CFLAGS) $(TEST_INC) -o $@ $<

COV_BINS = $(COV_DIR)/test_buffer \
           $(COV_DIR)/test_http_client \
           $(COV_DIR)/test_json_utils \
           $(COV_DIR)/test_onedrive \
           $(COV_DIR)/test_onedrive_music

$(COV_DIR)/test_buffer: $(COV_OBJ_DIR)/test_buffer.o $(COV_OBJ_DIR)/buffer.o
	@mkdir -p $(COV_DIR)
	$(CC) $(COV_CFLAGS) -o $@ $^

$(COV_DIR)/test_http_client: $(COV_OBJ_DIR)/test_http_client.o $(COV_SRC_OBJ)
	@mkdir -p $(COV_DIR)
	$(CC) $(COV_CFLAGS) -o $@ $^ $(LDLIBS)

$(COV_DIR)/test_json_utils: $(COV_OBJ_DIR)/test_json_utils.o $(COV_MOCK_OBJ) $(COV_MOCK_LIB_OBJ)
	@mkdir -p $(COV_DIR)
	$(CC) $(COV_CFLAGS) -o $@ $^ -lcjson

$(COV_DIR)/test_onedrive: $(COV_OBJ_DIR)/test_onedrive.o $(COV_MOCK_OBJ) $(COV_MOCK_LIB_OBJ)
	@mkdir -p $(COV_DIR)
	$(CC) $(COV_CFLAGS) -o $@ $^ -lcjson

$(COV_DIR)/test_onedrive_music: $(COV_OBJ_DIR)/test_onedrive_music.o $(COV_MOCK_OBJ) $(COV_MUSIC_LIB_OBJ)
	@mkdir -p $(COV_DIR)
	$(CC) $(COV_CFLAGS) -o $@ $^ -lcjson

coverage: $(COV_BINS)
	@failed=0; \
	for bin in $(COV_BINS); do \
		name=$$(basename $$bin); \
		echo "=== $$name ==="; \
		$$bin; \
		rc=$$?; \
		if [ $$rc -ne 0 ]; then failed=$$((failed + rc)); fi; \
	done; \
	echo ""; \
	echo "=== Coverage summary ==="; \
	cd $(COV_OBJ_DIR) && gcov -n *.gcda 2>&1 \
		| awk ' \
			/^File ..*\/src\/.*\.c/ { \
				fname = $$0; \
				sub(/^File ..*\/src\//, "", fname); \
				sub(/'\''$$/, "", fname); \
				next_is_line = 1; \
				next; \
			} \
			next_is_line && /^Lines executed:/ { \
				next_is_line = 0; \
				match($$0,/[0-9]+\.[0-9]+%/); pct=substr($$0,RSTART,RLENGTH-1)+0; \
				match($$0,/of +([0-9]+)/); n=substr($$0,RSTART,RLENGTH); sub(/of +/,"",n); n=n+0; \
				hit+=pct*n/100; total+=n; \
				printf "  %-45s%7s\n", fname, pct"%"; \
				next; \
			} \
			{ next_is_line = 0 } \
			END { \
				printf "  %-45s%7s\n", "---", "--"; \
				if(total>0) printf "  %-45s%6.2f%%\n", "total", hit/total*100; \
			}'; \
	if [ $$failed -ne 0 ]; then exit $$failed; fi

clean:
	rm -rf $(BUILD_DIR) $(ROOT_DIR)/bin

# Package
LIB_DIR = $(ROOT_DIR)/bin

PUBLIC_HEADERS = onedrive.h onedrive_music.h

lib: libaudiotag $(STATIC_LIB) $(SHARED_LIB)
	@mkdir -p $(LIB_DIR)/include
	@printf "create $(LIB_DIR)/libonedrive.a\naddlib $(STATIC_LIB)\naddlib $(LIBAUDIOTAG_DIR)/bin/libaudiotag.a\nsave\nend\n" | $(AR) -M
	@cp $(SHARED_LIB) $(LIB_DIR)/
	@$(foreach h,$(PUBLIC_HEADERS),cp $(SRC_DIR)/$(h) $(LIB_DIR)/include/;)
	@echo "Packaged into $(LIB_DIR)/"
	@echo "  libonedrive.a"
	@echo "  libonedrive.so"
	@echo "  include/"
	@ls $(LIB_DIR)/include/ | sed 's/^/    /'

libaudiotag:
	$(MAKE) -C $(LIBAUDIOTAG_DIR) lib

.PHONY: all clean test coverage lib libaudiotag
.SILENT: