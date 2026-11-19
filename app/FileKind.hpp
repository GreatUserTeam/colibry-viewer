// What a file is, and how it may be shown (MIT). Reading a device or a pipe could block forever,
// so those are refused before anything is opened.
#pragma once

#include <filesystem>
#include <string>

namespace colibry::viewer {

enum class Kind {
	Text,       // shown as text
	Image,      // PNG, JPEG, GIF, BMP, WebP, SVG
	Audio,      // played by an external player
	Video,
	Binary,     // anything else: shown as a hex dump
	Directory,  // cannot be viewed
	Special,    // device, pipe or socket: refused
	Missing,    // does not exist
};

const char* kindName(Kind kind);

// Magic bytes win over the extension, except for audio and video, which go by extension.
Kind detect(const std::filesystem::path& path);
bool looksLikeText(const std::string& sample);   // no NUL bytes, few control characters, valid UTF-8 or plain 8-bit
Kind kindOfExtension(const std::string& extension);  // ".mp3" -> Audio, ".png" -> Image, otherwise Binary
bool hasImageMagic(const std::string& head);
constexpr std::size_t kSampleSize = 8192;

std::string humanSize(std::uintmax_t bytes);

}  // namespace colibry::viewer
