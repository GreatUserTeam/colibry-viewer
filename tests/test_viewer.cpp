// Viewer tests: file kinds, text loading, hex dump, siblings, the player with a fake program, window logic.
#include "ViewerApp.hpp"

#include "check.hpp"

#include <unistd.h>

#include <fstream>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace colibry::viewer;

static void write(const fs::path& p, const std::string& data) {
	fs::create_directories(p.parent_path());
	std::ofstream(p, std::ios::binary) << data;
}

// A real 1x1 PNG.
static const unsigned char kPng[] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
    0x08,0x06,0x00,0x00,0x00,0x1F,0x15,0xC4,0x89,0x00,0x00,0x00,0x0D,0x49,0x44,0x41,0x54,0x78,0x9C,0x63,0xF8,0xCF,0xC0,0xF0,0x1F,0x00,0x05,0x00,0x01,0xFF,
    0x89,0x99,0x3D,0x1D,0x00,0x00,0x00,0x00,0x49,0x45,0x4E,0x44,0xAE,0x42,0x60,0x82};

static int testKinds(const fs::path& d) {
	write(d / "a.txt", "hello\n");
	write(d / "b.bin", std::string("\x00\x01\x02\xff", 4));
	write(d / "c.png", std::string(reinterpret_cast<const char*>(kPng), sizeof(kPng)));
	write(d / "renamed.dat", std::string(reinterpret_cast<const char*>(kPng), sizeof(kPng)));  // magic bytes beat the name
	write(d / "d.mp3", "not really audio");
	write(d / "e.mkv", "x");
	write(d / "empty.txt", "");
	fs::create_directories(d / "folder");
	EXPECT(detect(d / "a.txt") == Kind::Text && detect(d / "empty.txt") == Kind::Text);
	EXPECT(detect(d / "b.bin") == Kind::Binary && detect(d / "c.png") == Kind::Image && detect(d / "renamed.dat") == Kind::Image);
	EXPECT(detect(d / "d.mp3") == Kind::Audio && detect(d / "e.mkv") == Kind::Video);
	EXPECT(detect(d / "folder") == Kind::Directory && detect(d / "missing") == Kind::Missing);
	EXPECT(detect("/dev/null") == Kind::Special);  // a device is never opened
	EXPECT(looksLikeText("plain\ttext\n") && !looksLikeText(std::string("a\0b", 3)) && !looksLikeText(std::string(100, '\x01')));
	EXPECT(hasImageMagic("GIF89a....") && hasImageMagic(std::string("RIFF\0\0\0\0WEBP", 12)) && !hasImageMagic("RIFF....WAVE"));
	EXPECT(humanSize(1) == "1 byte" && humanSize(2048) == "2.0 KB" && humanSize(5 * 1024 * 1024) == "5.0 MB");
	return 0;
}

static int testLoading(const fs::path& d) {
	write(d / "bom.txt", "\xEF\xBB\xBFline1\r\nline2\r\n");
	auto bom = loadText(d / "bom.txt");
	EXPECT(bom.ok && bom.text == "line1\nline2\n");
	write(d / "latin.txt", "caf\xE9");
	EXPECT(loadText(d / "latin.txt").text == "caf\xC3\xA9");
	std::string cyr;
	for (int i = 0; i < 100; ++i) cyr += "я";
	write(d / "cyr.txt", cyr);
	auto cut = loadText(d / "cyr.txt", 11);  // 11 bytes would split the sixth letter
	EXPECT(cut.ok && cut.truncated && cut.text.size() == 10 && cut.fileSize == 200);
	EXPECT(!loadText(d / "folder").ok && !loadText(d / "nothing").ok && !loadText("/dev/zero").ok);
	EXPECT(loadText(d / "empty.txt").ok);

	EXPECT(hexDump("Hello world\n").find("48 65 6c 6c 6f 20 77 6f") != std::string::npos);
	EXPECT(hexDump("Hello world\n").find("|Hello world.|") != std::string::npos);
	EXPECT(hexDump(std::string(17, 'a'), 0x10).rfind("00000020", 0) == std::string::npos && hexDump(std::string(17, 'a'), 0x10).find("00000020") != std::string::npos);
	EXPECT(hexDump("").empty());
	auto hex = loadHex(d / "b.bin");
	EXPECT(hex.ok && hex.text.find("00 01 02 ff") != std::string::npos);

	write(d / "sib" / "B.txt", "b");
	write(d / "sib" / "a.txt", "a");
	write(d / "sib" / ".hidden", "h");
	fs::create_directories(d / "sib" / "dir");
	auto s = siblings(d / "sib" / "a.txt");
	EXPECT(s.size() == 2 && s[0].filename() == "a.txt" && s[1].filename() == "B.txt");  // case-insensitive order, no hidden, no folders
	return 0;
}

static int testPlayer(const fs::path& d) {
	auto mpv = Player::buildArgv("mpv", "/m/a b.mp3", false);
	EXPECT(mpv[0] == "mpv" && std::find(mpv.begin(), mpv.end(), "--no-video") != mpv.end() && mpv[mpv.size() - 2] == "--" && mpv.back() == "/m/a b.mp3");
	auto video = Player::buildArgv("/usr/bin/mpv", "/m/v.mp4", true);
	EXPECT(std::find(video.begin(), video.end(), "--no-video") == video.end());
	auto ff = Player::buildArgv("ffplay", "/m/a.mp3", false);
	EXPECT(std::find(ff.begin(), ff.end(), "-nodisp") != ff.end() && ff.back() == "/m/a.mp3");
	EXPECT(Player::buildArgv("paplay", "-weird.wav", false) == std::vector<std::string>({"paplay", "--", "-weird.wav"}));

	Player none({"definitely-not-a-player-xyz"}, d / "run1");
	auto failed = none.start("/m/a.mp3");
	EXPECT(!failed.ok && failed.error.find("No player") == 0 && none.running() == 0 && !none.stop());

	Player fake({"sleep"}, d / "run2");  // "sleep 30" stands in for a player
	auto started = fake.start("30");
	EXPECT(started.ok && started.program == "sleep" && started.pid > 1);
	EXPECT(fake.running() == started.pid && fs::exists(fake.pidFile()));
	Player other({"sleep"}, d / "run2");  // another run of the program finds the same playback
	EXPECT(other.running() == started.pid);
	EXPECT(other.stop() && fake.running() == 0 && !fs::exists(fake.pidFile()));
	EXPECT(!other.stop());

	// a pid file that points at some other program is stale and is never signalled
	write(fake.pidFile(), "1\n");
	EXPECT(fake.running() == 0 && !fs::exists(fake.pidFile()));
	write(fake.pidFile(), std::to_string(getpid()) + "\n");  // this test program is not "sleep"
	EXPECT(fake.running() == 0 && !fake.stop());
	write(fake.pidFile(), "garbage\n");
	EXPECT(fake.running() == 0);
	return 0;
}

static int testWindow(const fs::path& d) {
	Player fake({"sleep"}, d / "run3");
	ViewerApp app(std::move(fake));
	raven::Window window = colibry::window_from_file(std::string(COLIBRY_SOURCE_DIR) + "/app/viewer.xml");
	EXPECT(window.is_valid());
	app.attach(window);
	EXPECT(app.pageIndex() == 2);

	EXPECT(app.open(d / "a.txt") && app.kind() == Kind::Text && app.pageIndex() == 0 && app.shownText() == "hello\n");
	EXPECT(app.open(d / "b.bin") && app.kind() == Kind::Binary && app.shownText().find("00 01 02 ff") != std::string::npos);
	EXPECT(!app.open(d / "folder") && app.pageIndex() == 2);
	EXPECT(!app.open(d / "missing") && app.status().find("does not exist") != std::string::npos);
	EXPECT(!app.open("/dev/null") && app.status().find("device") != std::string::npos);
	app.open(d / "c.png");  // the null backend may not decode textures: either page is fine, nothing may crash
	EXPECT(app.kind() == Kind::Image && (app.pageIndex() == 1 || app.pageIndex() == 2));

	app.open(d / "d.mp3");
	EXPECT(app.canPlay() && app.pageIndex() == 2);
	app.open(d / "a.txt");
	app.play();
	EXPECT(app.status() == "Nothing to play.");

	// next / previous walk the folder and wrap around
	app.open(d / "sib" / "a.txt");
	app.next();
	EXPECT(app.current().filename() == "B.txt");
	app.next();
	EXPECT(app.current().filename() == "a.txt");
	app.previous();
	EXPECT(app.current().filename() == "B.txt");
	app.stop();
	EXPECT(app.status() == "Nothing is playing.");
	return 0;
}

int main() {
	TempDir tmp("viewer");
	RUN(testKinds(tmp.path));
	RUN(testLoading(tmp.path));
	RUN(testPlayer(tmp.path));
	EXPECT(colibry::init({"viewer-test", true}));
	RUN(testWindow(tmp.path));
	raven::shutdown();
	std::printf("viewer: all checks passed\n");
	return 0;
}
