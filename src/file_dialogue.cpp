#include "file_dialogue.hpp"

#include <iostream>
#include <queue>

void FileDialogue::searchDirFunc(FileDialogue *self){
    // @TODO: Replace with circular buffer
    std::queue<entry> entries;
    std::filesystem::path path;
    std::filesystem::directory_iterator di;
    while(true){
        std::unique_lock<std::mutex> lock(self->searchDirContinueMtx);
        self->searchDirContinueCV.wait(lock, [self]{return self->searchDirContinue;});
        lock.unlock();
        if(self->searchDirTerminate){
            return;
        }
        if(self->searchDirFlush){
            while(!entries.empty()){
                entries.pop();
            }
            self->searchDirFlush = false;
            path = self->path;
            di = std::filesystem::directory_iterator{path};
            entry currentDir = {};
            entry parentDir = {};
            currentDir.fname = ".";
            currentDir.isDir = true;
            parentDir.fname = "..";
            parentDir.isDir = true;
            entries.push(currentDir);
            entries.push(parentDir);
        }
        bool allowAll = self->filters.contains(".*") || self->filters.empty();
        for(int i=0;i<10 && di != std::filesystem::end(di);++i){
            entry e;
            e.fname = di->path().filename().string();
            e.isDir = di->is_directory();
            e.ext = di->path().extension();
            if(allowAll || self->filters.contains(e.ext) || e.isDir){
                // e.fsize = di->file_size();
                // e.status = di->status();
                // e.timeType = di->last_write_time();
                entries.push(e);
            }
            ++di;
        }
        if(!entries.empty() && !(self->state & STATE_LOCKED)){
            using namespace std::chrono_literals;
            std::unique_lock<std::timed_mutex> lock{self->entriesMtx,10ms};
            if(lock.owns_lock()){
                for(int i=0;i<10 && !entries.empty();++i){
                    self->entries.push_back(entries.front());
                    entries.pop();
                }
                lock.unlock();
            }
        }
        if(di == std::filesystem::end(di) && entries.size() == 0){
            self->searchDirContinue = false;
        }

    }
}

FileDialogue::FileDialogue(std::string title, std::filesystem::path path, std::vector<FileDialogue::filter> filters, uint8_t flags, uint8_t state, ImVec2 windowSize):
title{title},
path{},
filters{},
flags{flags},
index{0},
state{state},
entriesMtx{},
entries{},
sort{},
windowSize{windowSize},
searchDirTerminate{false},
searchDirFlush{false},
searchDirContinueMtx{},
searchDirContinueCV{},
searchDirContinue{false},
searchDir{&this->searchDirFunc, this},
pathbuff{nullptr},
fnamebuff{nullptr}{
    while(!this->changePath(path)){
        path = path.parent_path();
    }
    this->searchDirEnable();
    if(this->windowSize.x == 0){
        this->windowSize.x = 600;
    }
    if(this->windowSize.y == 0){
        this->windowSize.y = 600;
    }
    for(FileDialogue::filter &f : filters){
        this->filters.insert(std::pair<std::string, bool>{f.ext,f.selected});
    }
}

FileDialogue::~FileDialogue(){
    this->searchDirTerminate = true;
    this->searchDirEnable();
    if(this->searchDir.joinable()){
        this->searchDir.join();
    }
    if(this->pathbuff != nullptr){
        delete this->pathbuff;
    }
    if(this->fnamebuff != nullptr){
        delete this->fnamebuff;
    }
}

FileDialogue::STATUS FileDialogue::render(){
    ImGui::SetNextWindowSize(this->windowSize,ImGuiCond_Appearing);
    ImGui::SetNextWindowPos({0,0},ImGuiCond_Appearing);
    ImGui::Begin(this->title.c_str());
    this->renderPathBar();
    FileDialogue::STATUS res = this->renderDir();
    if(res == FileDialogue::STATUS::NONE){
        res = this->renderButtons();
    }
    ImGui::End();
    if(res == FileDialogue::STATUS::NONE){
        res = this->renderDialogues();
    }
    return res;
}
bool FileDialogue::changePath(std::filesystem::path newPath){
    if(this->state & STATE_LOCKED){
        return false;
    }
    newPath = newPath.lexically_normal();
    if(std::filesystem::exists(newPath) || newPath.empty()){
        this->path = newPath;
        this->searchDirContinue = false;
        this->searchDirFlush = true;
        this->entries.clear();
        return true;
    }
    return false;
}
void FileDialogue::getPath(std::filesystem::path *res){
    *res = this->path;
}
bool FileDialogue::getNextEntry(entry *res){
    while(this->index < this->entries.size()){
        entry e = this->entries.at(this->index++);
        if(e.selected){
            *res = e;
            return true;
        }
    }
    return false;
}


void FileDialogue::renderPathBar(){
    std::string pathBarTitle = "pathbar" + this->title;
    ImGui::BeginChild(pathBarTitle.c_str(),{this->windowSize.x-25,50});
    if(this->state & FileDialogue::STATE_EDIT_DIR){
        if(ImGui::InputText("##dir",this->pathbuff,FileDialogue::PATH_SIZE,ImGuiInputTextFlags_EnterReturnsTrue)){
            std::filesystem::path newPath{this->pathbuff};
            if(!this->changePath(newPath)){
                this->state |= FileDialogue::STATE_DIALOGUE_INVALID_PATH;
            }else{
                this->searchDirEnable();
            }
            delete this->pathbuff;
            this->pathbuff = nullptr;
            this->state &= ~FileDialogue::STATE_EDIT_DIR;
        }
    }else{
        std::filesystem::path::iterator dir = this->path.begin();
        int i = 0;
        while(++dir != this->path.end() && dir->string().size() > 0){
            ImGui::Text("/");
            ImGui::SameLine();
            std::string dirName = dir->string() + "##" + std::to_string(i++);
            if(ImGui::Button(dirName.c_str()) && !(this->state & STATE_LOCKED)){
                std::filesystem::path newPath = this->path;
                while(++dir != this->path.end()){
                    newPath = newPath.parent_path();
                }
                if(!this->changePath(newPath)){
                    this->state |= FileDialogue::STATE_DIALOGUE_MISSING_PATH;
                }else{
                    this->searchDirEnable();
                }
                break;
            }
            ImGui::SameLine();
        }
        if(ImGui::Button("##editpath") && !(this->state & STATE_LOCKED)){
            this->pathbuff = new char[FileDialogue::PATH_SIZE];
            memset(this->pathbuff, '\0', FileDialogue::PATH_SIZE);
            memcpy(this->pathbuff, this->path.c_str(), std::min(FileDialogue::PATH_SIZE, this->path.string().size()));
            this->state |= FileDialogue::STATE_EDIT_DIR;
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Refresh")){
        if(this->changePath(this->path)){
            this->searchDirEnable();
        }
    }
    ImGui::EndChild();
}
FileDialogue::STATUS FileDialogue::renderDir(){
    static std::chrono::time_point<std::chrono::steady_clock> lastClick;
    static ImVec2 lastMousePos = ImGui::GetMousePos();

    FileDialogue::STATUS res = FileDialogue::STATUS::NONE;

    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    ImVec2 currentMousePos = ImGui::GetMousePos();

    int64_t dt = std::chrono::duration_cast<std::chrono::milliseconds>(now-lastClick).count();
    float dx = currentMousePos.x-lastMousePos.x, dy = currentMousePos.y-lastMousePos.y;

    bool doubleClicked = dt < 500 && abs(dx) < 2 && abs(dy) < 2;

    std::string dirTitle = "dir" + this->title;
    ImGui::BeginChild(dirTitle.c_str(),{this->windowSize.x-25,this->windowSize.y-150});
    
    std::unique_lock lock{this->entriesMtx};
    auto e = this->entries.begin();
    bool skipFilter = this->filters.empty() || (this->filters.contains(".*") && this->filters.at(".*"));
    while(e != this->entries.end()){
        if(this->renderEntry(*e, skipFilter)){
            if(doubleClicked){
                if(e->isDir){
                    std::filesystem::path newPath = this->path / e->fname;
                    this->changePath(newPath);
                    this->searchDirEnable();
                    lastClick -= std::chrono::seconds(1);
                    break;
                }else{
                    this->state |= STATE_LOCKED;
                    res = FileDialogue::STATUS::SUCCESS;
                }
            }else{
                lastClick = now;
                lastMousePos = currentMousePos;
                this->deselectAll();
                e->selected = true;
            }
        }
        ++e;
    }
    lock.unlock();
    ImGui::EndChild();
    return res;
}
bool FileDialogue::renderEntry(entry &e, bool skipFilter){
    // Check if filter allows rendering
    if(!(skipFilter || e.isDir || (this->filters.contains(e.ext) && this->filters.at(e.ext)))){
        e.selected = false;
        return false;
    }
    int popColor = 0;
    // Render multi-select if allowed
    if(this->flags & FileDialogue::FLAGS_MULTI_SELECT){
        std::string selectableName = "##sel_" + e.fname;
        ImGui::Checkbox(selectableName.c_str(),&e.selected);
        ImGui::SameLine();
    }
    // Recolor if selected
    if(e.selected){
        if(e.isDir && !(this->flags & FileDialogue::FLAGS_MULTI_SELECT)){
            ImGui::PushStyleColor(ImGuiCol_Button,{1,0,0,1});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,{0,1,0,1});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0,0,1,1});
            popColor += 3;
        }else{
            ImGui::PushStyleColor(ImGuiCol_Button,{1,1,0,1});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,{0,1,1,1});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{1,0,1,1});
            popColor += 3;
        }
    }
    // Draw entry's button
    if(ImGui::Button(e.fname.c_str()) && !(this->state & STATE_LOCKED)){
        ImGui::PopStyleColor(popColor);
        return true;
    }else{
        ImGui::PopStyleColor(popColor);
        return false;
    }
}
FileDialogue::STATUS FileDialogue::renderButtons(){
    FileDialogue::STATUS res = FileDialogue::STATUS::NONE;
    std::string buttonTitle = "buttons" + this->title;
    ImGui::BeginChild(buttonTitle.c_str(),{this->windowSize.x-25,50});
    if(ImGui::Button("Cancel")){
        res = FileDialogue::STATUS::CANCEL;
    }
    ImGui::SameLine();
    if(ImGui::Button("Confirm")){
        res = FileDialogue::STATUS::SUCCESS;
        this->state |= STATE_LOCKED;
    }
    ImGui::EndChild();
    return res;
}
FileDialogue::STATUS FileDialogue::renderDialogues(){
    if(this->state & FileDialogue::STATE_DIALOGUE_INVALID_PATH){
        ImGui::Begin("Invalid Path");
        if(ImGui::Button("Ok")){
            this->state &= ~FileDialogue::STATE_DIALOGUE_INVALID_PATH;
        }
        ImGui::End();
    }
    if(this->state & FileDialogue::STATE_DIALOGUE_MISSING_PATH){
        ImGui::Begin("Missing Path (it was probably deleted by another process)");
        if(ImGui::Button("Ok")){
            this->state &= ~FileDialogue::STATE_DIALOGUE_MISSING_PATH;
        }
        ImGui::End();
    }
    return FileDialogue::STATUS::NONE;
}

void FileDialogue::searchDirEnable(){
    std::lock_guard lock{this->searchDirContinueMtx};
    this->searchDirContinue = true;
    this->searchDirContinueCV.notify_one();
}
void FileDialogue::deselectAll(){
    for(auto &entry : this->entries){
        entry.selected = false;
    }
}