# FileExplorer

Building:
Compile file_dialogue.cpp
- make build_deps && make buildo

Include file_dialogue.o and file_dialogue.hpp in your project

Usage:
See main.cpp for example.

Construct file dialogue (it is recommended to use std::unique_ptr<FileDialogue>):
FileDialogue(std::string title, std::filesystem::path path, std::vector<FileDialogue::filter> filters = {}, uint8_t flags = FLAGS_NONE, uint8_t state = STATE_NONE, ImVec2 windowSize = {0,0});

Render file dialogue:
STATUS render();

When STATUS::NONE is returned, do nothing.
When STATUS::CANCEL is returned, destruct the file dialogue.
when STATUS::SUCCESS is returned, call bool getNextEntry(entry *res); until false to get list of selected files.