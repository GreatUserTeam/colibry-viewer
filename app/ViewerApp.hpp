// The viewer window logic on Raven (MIT). Keep the object alive while the window is open.
#pragma once

#include "FileKind.hpp"
#include "Player.hpp"
#include "TextLoad.hpp"

#include <colibry/app.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace colibry::viewer {

class ViewerApp {
public:
	explicit ViewerApp(Player player = Player());

	// Connects the widgets of viewer.xml (editor, picture, message, switcher, path, status, play, stop)
	// and the viewer.* actions.
	void attach(raven::Window window);

	// Shows a file: text in the editor, binary as a hex dump, an image as a picture; audio and video
	// get the Play button. Returns false and shows the reason when the file cannot be shown.
	bool open(std::filesystem::path path);  // by value: callers may pass an element of our own sibling list
	void next();
	void previous();
	void play();
	void stop();

	const std::filesystem::path& current() const { return m_current; }
	Kind kind() const { return m_kind; }
	const std::string& status() const { return m_statusText; }
	const std::string& shownText() const { return m_shown; }
	// 0 = text/hex editor, 1 = picture, 2 = message
	int pageIndex() const { return m_page; }
	bool canPlay() const { return m_kind == Kind::Audio || m_kind == Kind::Video; }
	Player& player() { return m_player; }

private:
	void step(int direction);
	void showMessage(const std::string& text);
	void setStatus(const std::string& text);

	Player m_player;
	std::filesystem::path m_current;
	std::vector<std::filesystem::path> m_siblings;
	Kind m_kind = Kind::Missing;
	int m_page = 2;
	std::string m_statusText, m_shown;

	raven::Window m_window;
	raven::TextEditor m_editor;
	raven::Image m_picture;
	raven::Label m_message, m_statusLabel;
	raven::Switcher m_switcher;
	raven::TextInput m_pathInput;
	raven::Button m_playButton, m_stopButton;
	raven::Texture m_texture;
};

}  // namespace colibry::viewer
