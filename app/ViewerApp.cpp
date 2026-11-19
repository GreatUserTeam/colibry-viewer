#include "ViewerApp.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

namespace fs = std::filesystem;

namespace colibry::viewer {

ViewerApp::ViewerApp(Player player) : m_player(std::move(player)) {}

void ViewerApp::attach(raven::Window window) {
	m_window = window;
	raven::Widget root(window.get_content());
	m_editor = raven::TextEditor(root.find("editor"));
	m_picture = raven::Image(root.find("picture"));
	m_message = raven::Label(root.find("message"));
	m_statusLabel = raven::Label(root.find("status"));
	m_switcher = raven::Switcher(root.find("switcher"));
	m_pathInput = raven::TextInput(root.find("path"));
	m_playButton = raven::Button(root.find("play"));
	m_stopButton = raven::Button(root.find("stop"));

	m_editor.set_read_only(1);
	m_picture.set_stretch(R_STRETCH_FIT);
	auto bind = [](const char* name, auto fn) { raven::action_bind(name, [fn](Rid, const char*) { fn(); }); };
	bind("viewer.next", [this] { next(); });
	bind("viewer.previous", [this] { previous(); });
	bind("viewer.play", [this] { play(); });
	bind("viewer.stop", [this] { stop(); });
	bind("viewer.open", [this] { open(m_pathInput.get_text()); });
	showMessage("Open a file: type its path above and press Enter.");
}

bool ViewerApp::open(fs::path path) {
	m_kind = detect(path);
	m_current = path;
	m_shown.clear();
	m_playButton.set_enabled(0);
	m_stopButton.set_enabled(0);
	m_pathInput.set_text(path.string().c_str());

	switch (m_kind) {
		case Kind::Missing: showMessage("The file does not exist."); return false;
		case Kind::Directory: showMessage("This is a folder; it cannot be viewed."); return false;
		case Kind::Special: showMessage("This is a device or a pipe; reading it could block, so it is not opened."); return false;
		default: break;
	}
	std::error_code ec;
	m_siblings = siblings(path);
	const std::string name = path.filename().string();
	const std::string size = humanSize(fs::file_size(path, ec));

	switch (m_kind) {
		case Kind::Text: {
			Loaded r = loadText(path);
			if (!r.ok) { showMessage(r.error); return false; }
			m_shown = r.text;
			m_editor.set_text(m_shown.c_str());
			m_switcher.set_index(0);
			m_page = 0;
			setStatus(name + " - text, " + size + (r.truncated ? " (only the first part is shown)" : ""));
			return true;
		}
		case Kind::Binary: {
			Loaded r = loadHex(path);
			if (!r.ok) { showMessage(r.error); return false; }
			m_shown = r.text;
			m_editor.set_text(m_shown.c_str());
			m_switcher.set_index(0);
			m_page = 0;
			setStatus(name + " - binary, " + size + (r.truncated ? " (hex dump of the first 64 KB)" : ""));
			return true;
		}
		case Kind::Image: {
			raven::Texture texture = raven::Texture::create_from_file(path.string().c_str());
			if (!texture.is_valid()) { showMessage("This picture cannot be read."); return false; }
			if (m_texture.is_valid()) m_texture.destroy();
			m_texture = texture;
			m_picture.set_texture(m_texture.id());
			m_switcher.set_index(1);
			m_page = 1;
			setStatus(name + " - image, " + size);
			return true;
		}
		case Kind::Audio:
		case Kind::Video:
			showMessage(name + "\n" + std::string(kindName(m_kind)) + ", " + size + "\nPress Play to listen; playback goes on when this window is closed.");
			m_playButton.set_enabled(1);
			m_stopButton.set_enabled(1);
			setStatus(name + " - " + kindName(m_kind) + ", " + size);
			return true;
		default: return false;
	}
}

void ViewerApp::next() { step(+1); }
void ViewerApp::previous() { step(-1); }

void ViewerApp::step(int direction) {
	if (m_siblings.empty()) return;
	auto it = std::find(m_siblings.begin(), m_siblings.end(), m_current);
	long index = it == m_siblings.end() ? 0 : static_cast<long>(it - m_siblings.begin()) + direction;
	const long count = static_cast<long>(m_siblings.size());
	index = (index % count + count) % count;  // wraps around
	open(m_siblings[static_cast<std::size_t>(index)]);
}

void ViewerApp::play() {
	if (!canPlay()) { setStatus("Nothing to play."); return; }
	PlayerResult r = m_player.start(m_current, m_kind == Kind::Video);
	setStatus(r.ok ? "Playing with " + r.program : r.error);
}

void ViewerApp::stop() { setStatus(m_player.stop() ? "Stopped." : "Nothing is playing."); }

void ViewerApp::showMessage(const std::string& text) {
	m_message.set_text(text.c_str());
	m_switcher.set_index(2);
	m_page = 2;
	setStatus(text.substr(0, text.find('\n')));
}

void ViewerApp::setStatus(const std::string& text) {
	m_statusText = text;
	m_statusLabel.set_text(text.c_str());
}

}  // namespace colibry::viewer
