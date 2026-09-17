#pragma once

#include "imgui/imgui.h"

#include <cstdint>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>


class FileDialogue{
public:
    struct entry{
        std::string fname;
        std::string ext;
        bool isDir;
        bool selected = 0;
        uintmax_t fsize = 0;
        std::filesystem::file_status status{};
        std::filesystem::file_time_type timeType{};
    };
    struct sortMethod{
        enum class FIELD : uint8_t{
            NAME_ALPHA, NAME_LENGTH, SIZE, LAST_MODIFIED
        };
        FIELD field;
        bool ascending;
    };
    struct filter{
        std::string ext;
        bool selected = true;
    };
    enum class STATUS : uint8_t{
        NONE,
        SUCCESS,
        FAILURE,
        CANCEL
    };
    const static uint8_t FLAGS_NONE                     = 0b00000000;

    const static uint8_t FLAGS_MULTI_SELECT             = 0b00000001;

    const static uint8_t STATE_NONE                     = 0b00000000;
    const static uint8_t STATE_EDIT_DIR                 = 0b00000001;
    const static uint8_t STATE_LOCKED                   = 0b00000010;

    const static uint8_t STATE_DIALOGUE_INVALID_PATH    = 0b00100000;
    const static uint8_t STATE_DIALOGUE_MISSING_PATH    = 0b01000000;
    const static uint8_t STATE_DIALOGUE_CONFIRM_WRITE   = 0b10000000;
    
private:
    static constexpr uint_fast16_t PATH_SIZE = 512 * sizeof(char);
    static constexpr uint_fast16_t FNAME_SIZE = 128 * sizeof(char);

    std::string title;
    std::filesystem::path path;
    std::unordered_map<std::string, bool> filters;
    const uint8_t flags;
    std::size_t index;

    uint8_t state;
    std::timed_mutex entriesMtx;
    std::vector<entry> entries;
    sortMethod sort;
    ImVec2 windowSize;

    bool searchDirTerminate;
    bool searchDirFlush;
    std::mutex searchDirContinueMtx;
    std::condition_variable searchDirContinueCV;
    bool searchDirContinue;
    std::thread searchDir;
    
    char *pathbuff;
    char *fnamebuff;
    
    static void searchDirFunc(FileDialogue *self);
public:
    FileDialogue(std::string title, std::filesystem::path path, std::vector<FileDialogue::filter> filters = {}, uint8_t flags = FLAGS_NONE, uint8_t state = STATE_NONE, ImVec2 windowSize = {0,0});
    ~FileDialogue();

    STATUS render();
    bool changePath(std::filesystem::path newPath);
    void getPath(std::filesystem::path *res);
    // Returns true if an entry is found, false if all entries have been looped through
    bool getNextEntry(entry *res);
private:
    void renderPathBar();
    STATUS renderDir();
    // Returns true if clicked
    bool renderEntry(entry &e, bool skipFilter);
    STATUS renderButtons();
    STATUS renderDialogues();
    void searchDirEnable();
    void deselectAll();
};