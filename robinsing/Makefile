NAME    := robinsing
SRC     := src/main.c src/warble.c src/platform.c
HDR     := src/version.h src/warble.h src/platform.h src/warble_table.h
LYRICS  := lyrics/sway-the-beat-in-cosmos.txt lyrics/if-i-can-stop-one-heart-from-breaking.txt lyrics/hope-is-the-sing-with-feathers.txt lyrics/had-i-not-seen-the-sun.txt
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Werror
DIST    := dist
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Darwin)
BIN   := $(DIST)/$(NAME)-darwin-universal
LINK  := cc $(CFLAGS) -mmacosx-version-min=12.0 -arch arm64 -arch x86_64
else ifeq ($(UNAME_S),Linux)
BIN   := $(DIST)/$(NAME)-linux-$(UNAME_M)
LINK  := gcc $(CFLAGS) -static
else
$(error macOS / Linux 用 make，Windows 用 build.bat)
endif

# PowerPC（Mac OS X 10.5 / gcc 4.2）。现代 clang 已经没有 ppc 后端了，
# 必须在 ppc 工具链上编：make ppc
#
# 工具链从 Xcode 3.2.6 的 dmg 里抽出来（见 Columbula-PowerPC移植方案.md），
# 装好后 make ppc 直接可用；换位置时用 make ppc XC326=/path/to/xcode326 覆盖
PPC_BIN  := $(DIST)/$(NAME)-darwin-ppc
XC326    ?= /usr/local/toolchains/xcode326
PPC_SDK  := $(XC326)/MacOSX10.5.sdk
PPC_GCC  := $(XC326)/gcc42/bin/powerpc-apple-darwin10-gcc-4.2.1
LINK_PPC ?= $(PPC_GCC) -isysroot $(PPC_SDK) -std=gnu99 -O2 -Wall -Wextra -mmacosx-version-min=10.5

# 交叉编译 Linux（zig 自带的 musl 工具链，产物完全静态，不依赖目标机任何运行库）
#   make amd64-linux  -> dist/robinsing-linux-amd64   x86_64
#   make arm64-linux  -> dist/robinsing-linux-arm64   AArch64
#   make ppc-linux    -> dist/robinsing-linux-ppc     32 位大端 PowerPC
ZIG_CC := zig cc -std=c11 -O2 -Wall -Wextra -Werror -Wl,-s -o

.PHONY: all check clean ppc ppc-linux arm64-linux amd64-linux
.DELETE_ON_ERROR:

all: $(BIN)

$(BIN): $(SRC) $(HDR)
	@mkdir -p $(DIST)
	$(LINK) -o $@ $(SRC)
	@strip $@ 2>/dev/null || true
	@codesign --force --sign - $@ 2>/dev/null || true

src/warble_table.h: $(LYRICS) tools/gen_table.awk
	@mkdir -p src
	@LC_ALL=C awk -f tools/gen_table.awk $(LYRICS) > $@

# ppc 用 gcc 4.2 编，不带 -Werror：老编译器的 -Wextra 警告比新 clang 多，
# 一旦 -Werror 会因为无关警告失败
# gcc-4.2 会去 PATH 里找 as / ld，必须让老工具链排在系统工具之前，
# 否则会捡到系统的 as（它背后是 clang，直接拒绝 -arch ppc）
ppc: $(SRC) $(HDR)
	@test -x $(PPC_GCC) || { echo "缺少 ppc 工具链: $(PPC_GCC)"; exit 1; }
	@mkdir -p $(DIST)
	PATH=$(XC326)/cli/bin:$(XC326)/gcc42/bin:$$PATH $(LINK_PPC) -o $(PPC_BIN) $(SRC)

ppc-linux: $(SRC) $(HDR)
	@mkdir -p $(DIST)
	$(ZIG_CC) $(DIST)/$(NAME)-linux-ppc -target powerpc-linux-musleabihf $(SRC)

arm64-linux: $(SRC) $(HDR)
	@mkdir -p $(DIST)
	$(ZIG_CC) $(DIST)/$(NAME)-linux-arm64 -target aarch64-linux-musl $(SRC)

amd64-linux: $(SRC) $(HDR)
	@mkdir -p $(DIST)
	$(ZIG_CC) $(DIST)/$(NAME)-linux-amd64 -target x86_64-linux-musl $(SRC)

check: $(BIN)
	@echo "  裸 robinsing 进入选歌菜单（EOF 退出 2）"
	@printf '' | ./$(BIN) | diff -u tests/expected/menu.txt -
	@! printf '' | ./$(BIN) >/dev/null 2>&1
	@echo "  菜单选歌"
	@cat tests/expected/menu.txt tests/expected/sway.txt > $(DIST)/menu-sway.txt
	@printf '1\n' | ./$(BIN) | diff -u $(DIST)/menu-sway.txt -
	@cat tests/expected/menu.txt tests/expected/if.txt > $(DIST)/menu-if.txt
	@printf '2\n' | ./$(BIN) | diff -u $(DIST)/menu-if.txt -
	@cat tests/expected/menu.txt tests/expected/hope.txt > $(DIST)/menu-hope.txt
	@printf '3\n' | ./$(BIN) | diff -u $(DIST)/menu-hope.txt -
	@cat tests/expected/menu.txt tests/expected/had.txt > $(DIST)/menu-had.txt
	@printf '4\n' | ./$(BIN) | diff -u $(DIST)/menu-had.txt -
	@printf '4\n' | ./$(BIN) h | diff -u $(DIST)/menu-had.txt -
	@echo "  无效编号重画菜单"
	@cat tests/expected/menu.txt tests/expected/menu.txt tests/expected/if.txt > $(DIST)/menu-twice-if.txt
	@printf '0\n2\n' | ./$(BIN) | diff -u $(DIST)/menu-twice-if.txt -
	@echo "  数字 / 短名 / 长名"
	@./$(BIN) 1 | diff -u tests/expected/sway.txt -
	@./$(BIN) sway | diff -u tests/expected/sway.txt -
	@./$(BIN) sway-the-beat-in-cosmos | diff -u tests/expected/sway.txt -
	@./$(BIN) 2 | diff -u tests/expected/if.txt -
	@./$(BIN) if | diff -u tests/expected/if.txt -
	@./$(BIN) if-i-can-stop-one-heart-from-breaking | diff -u tests/expected/if.txt -
	@./$(BIN) 3 | diff -u tests/expected/hope.txt -
	@./$(BIN) hope | diff -u tests/expected/hope.txt -
	@./$(BIN) hope-is-the-sing-with-feathers | diff -u tests/expected/hope.txt -
	@./$(BIN) 4 | diff -u tests/expected/had.txt -
	@./$(BIN) had | diff -u tests/expected/had.txt -
	@./$(BIN) had-i-not-seen-the-sun | diff -u tests/expected/had.txt -
	@echo "  唯一前缀"
	@./$(BIN) s | diff -q tests/expected/sway.txt - >/dev/null
	@./$(BIN) sway-t | diff -q tests/expected/sway.txt - >/dev/null
	@./$(BIN) if-i | diff -q tests/expected/if.txt - >/dev/null
	@./$(BIN) hope-is | diff -q tests/expected/hope.txt - >/dev/null
	@./$(BIN) had-i | diff -q tests/expected/had.txt - >/dev/null
	@echo "  不区分大小写"
	@./$(BIN) SWAY-THE-BEAT-IN-COSMOS | diff -q tests/expected/sway.txt - >/dev/null
	@./$(BIN) If-I-Can-Stop-One-Heart-From-Breaking | diff -q tests/expected/if.txt - >/dev/null
	@echo "  四首歌词互不相同"
	@./$(BIN) 1 > $(DIST)/song1.txt
	@./$(BIN) 2 > $(DIST)/song2.txt
	@./$(BIN) 3 > $(DIST)/song3.txt
	@./$(BIN) 4 > $(DIST)/song4.txt
	@! diff -q $(DIST)/song1.txt $(DIST)/song2.txt >/dev/null
	@! diff -q $(DIST)/song1.txt $(DIST)/song3.txt >/dev/null
	@! diff -q $(DIST)/song1.txt $(DIST)/song4.txt >/dev/null
	@! diff -q $(DIST)/song2.txt $(DIST)/song3.txt >/dev/null
	@! diff -q $(DIST)/song2.txt $(DIST)/song4.txt >/dev/null
	@! diff -q $(DIST)/song3.txt $(DIST)/song4.txt >/dev/null
	@echo "  --version"
	@./$(BIN) -v | grep -Eqx '[0-9]+\.[0-9]+\.[0-9]+'
	@echo "  速度单位"
	@./$(BIN) 1 -s 30ms -p | diff -q tests/expected/sway.txt - >/dev/null
	@./$(BIN) 1 -s=1.2s -p | diff -q tests/expected/sway.txt - >/dev/null
	@./$(BIN) 1 -s500 -p | diff -q tests/expected/sway.txt - >/dev/null
	@echo "  非法速度被拒"
	@! ./$(BIN) 1 -s 5x -p >/dev/null 2>&1
	@! ./$(BIN) 1 -s >/dev/null 2>&1
	@! ./$(BIN) 1 -s 0 -p >/dev/null 2>&1
	@echo "  不认识的歌"
	@./$(BIN) nope | diff -u tests/expected/unknown.txt -
	@./$(BIN) 5 | diff -q tests/expected/unknown.txt - >/dev/null
	@! ./$(BIN) nope >/dev/null 2>&1
	@echo "  歧义 / 多余参数"
	@! ./$(BIN) h < /dev/null >/dev/null 2>&1
	@! ./$(BIN) sway if >/dev/null 2>&1
	@echo "  高潮段 -c / --chorus / --climax"
	@./$(BIN) 1 -c | diff -u tests/expected/sway-chorus.txt -
	@./$(BIN) sway --chorus | diff -q tests/expected/sway-chorus.txt - >/dev/null
	@./$(BIN) 2 --climax | diff -u tests/expected/if-chorus.txt -
	@./$(BIN) 3 -c | diff -u tests/expected/hope-chorus.txt -
	@./$(BIN) 1 -c -s 30ms -p | diff -q tests/expected/sway-chorus.txt - >/dev/null
	@./$(BIN) had -c | diff -u tests/expected/had-chorus.txt -
	@echo "  尾声 -e / --end / --ends"
	@./$(BIN) 1 -e | diff -u tests/expected/sway-end.txt -
	@./$(BIN) sway --end | diff -q tests/expected/sway-end.txt - >/dev/null
	@./$(BIN) 2 --ends | diff -u tests/expected/if-end.txt -
	@./$(BIN) 3 -e | diff -u tests/expected/hope-end.txt -
	@./$(BIN) had -e | diff -u tests/expected/had-end.txt -
	@cat tests/expected/menu.txt tests/expected/had-end.txt > $(DIST)/menu-had-end.txt
	@printf '4\n' | ./$(BIN) -e | diff -u $(DIST)/menu-had-end.txt -
	@! ./$(BIN) 1 -c -e >/dev/null 2>&1
	@echo "  菜单也能选高潮"
	@cat tests/expected/menu.txt tests/expected/sway-chorus.txt > $(DIST)/menu-sway-chorus.txt
	@printf '1\n' | ./$(BIN) -c | diff -u $(DIST)/menu-sway-chorus.txt -
	@printf '1\n' | ./$(BIN) h -c | diff -u $(DIST)/menu-sway-chorus.txt -
	@./$(BIN) nope -c | diff -q tests/expected/unknown.txt - >/dev/null
	@echo "  参数顺序 / -s 取值 / --"
	@./$(BIN) -c 1 | diff -q tests/expected/sway-chorus.txt - >/dev/null
	@./$(BIN) 1 -c | diff -q tests/expected/sway-chorus.txt - >/dev/null
	@./$(BIN) -s=1 1 | diff -q tests/expected/sway.txt - >/dev/null
	@./$(BIN) -s 1 1 | diff -q tests/expected/sway.txt - >/dev/null
	@printf '2\n' | ./$(BIN) -s 1 | diff -u $(DIST)/menu-if.txt -
	@./$(BIN) -- 1 | diff -q tests/expected/sway.txt - >/dev/null
	@./$(BIN) -c -- 1 | diff -q tests/expected/sway-chorus.txt - >/dev/null
	@! ./$(BIN) -- 1 -c >/dev/null 2>&1
	@! ./$(BIN) -- -c >/dev/null 2>&1
	@echo "  颜色编号"
	@./$(BIN) --color 200 1 | diff -q tests/expected/sway.txt - >/dev/null
	@./$(BIN) --color=93 1 | diff -q tests/expected/sway.txt - >/dev/null
	@ROBINSING_COLOR=200 ./$(BIN) 1 | diff -q tests/expected/sway.txt - >/dev/null
	@! ./$(BIN) --color 300 1 >/dev/null 2>&1
	@! ./$(BIN) --color abc 1 >/dev/null 2>&1
	@! ./$(BIN) --color >/dev/null 2>&1
	@! ROBINSING_COLOR=999 ./$(BIN) 1 >/dev/null 2>&1
	@echo "  全部通过"

clean:
	rm -rf $(DIST) src/warble_table.h
