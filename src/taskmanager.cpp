#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <dirent.h>
#include <cstdint>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define WIDTH 1000
#define HEIGHT 800

typedef struct ProcessInfo
{
    int pid;
    std::string name;
    long memory_kb;
} ProcessInfo;

typedef struct GProcess // graphical process
{
    SDL_Texture* pid_texture;
    SDL_Texture* name_texture;
    SDL_Texture* mem_texture;

    SDL_Rect pid_rect;
    SDL_Rect name_rect;
    SDL_Rect mem_rect;
} GProcess;

typedef struct AppData
{
    TTF_Font* font;

    std::vector<ProcessInfo*> processes;
    std::vector<GProcess*> graphics;

    int scroll_offset = 0;
} AppData;

bool isNumber(const std::string& str);
bool compareProcesses(ProcessInfo* a, ProcessInfo* b);
void clearGraphics(AppData* data);
void clearProcesses(AppData* data);
void loadProcesses(AppData* data);
SDL_Texture* createText(SDL_Renderer* renderer, TTF_Font* font, std::string text, SDL_Color color);
void buildGraphics(SDL_Renderer* renderer, AppData* data);
void render(SDL_Renderer* renderer, AppData* data);

int main(int argc, char* argv[])
{
    // Initialize SDL2 (including image and font loaders)
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    // Create window and renderer
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &window, &renderer);

    // Create app state
    AppData data;

    // Load font
    data.font = TTF_OpenFont("resrc/fonts/OpenSans-Regular.ttf", 20);

    // Load the processes
    loadProcesses(&data);

    // Build the graphics
    buildGraphics(renderer, &data);

    bool running = true;

    uint32_t last_update = SDL_GetTicks();

    while (running)
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }

            if (event.type == SDL_MOUSEWHEEL)
            {
                data.scroll_offset += event.wheel.y * 20;
                buildGraphics(renderer, &data);
            }
        }

        uint32_t current = SDL_GetTicks();

        // Refresh process list every 500ms
        if (current - last_update > 500)
        {
            loadProcesses(&data);
            buildGraphics(renderer, &data);

            last_update = current;
        }

        // Render everything
        render(renderer, &data);

        SDL_Delay(16); // prevents CPU from maxing out
    }

    // Cleanup
    clearGraphics(&data);
    clearProcesses(&data);

    TTF_CloseFont(data.font);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    return 0;
}

bool isNumber(const std::string& str)
{
    for (char c : str)
    {
        if (!isdigit(c))
            return false;
    }
    return true;
}

bool compareProcesses(ProcessInfo* a, ProcessInfo* b)
{
    return a->memory_kb > b->memory_kb;
}

void clearGraphics(AppData* data)
{
    for (GProcess* g : data->graphics)
    {
        SDL_DestroyTexture(g->pid_texture);
        SDL_DestroyTexture(g->name_texture);
        SDL_DestroyTexture(g->mem_texture);
        delete g;
    }

    data->graphics.clear();
}

void clearProcesses(AppData* data)
{
    for (ProcessInfo* p : data->processes)
    {
        delete p;
    }

    data->processes.clear();
}

void loadProcesses(AppData* data)
{
    // Clear out old processes
    clearProcesses(data);

    // Open /proc
    DIR* proc = opendir("/proc");

    // If /proc didn't open correctly (NULL), then return
    if (!proc) 
    {
        return;
    }

    struct dirent* entry;

    // Loop through all the folders and files in /proc
    while ((entry = readdir(proc)) != NULL)
    {
        // Get the folder name
        std::string dir_name = entry->d_name;

        // Skip the non-numeric folders (only numeric folders are processes)
        if (!isNumber(dir_name))
        {
            continue;
        }

        // Convert the PID to an int (the directory name is the PID)
        int pid = std::stoi(dir_name);

        // Build status file path
        std::string status_path = "/proc/" + dir_name + "/status";

        // Open file (read process status info)
        std::ifstream file(status_path);

        // If file wasn't opened successfully
        if (!file.is_open())
        {
            continue;
        }

        // Create process object
        ProcessInfo* process = new ProcessInfo();
        process->pid = pid;
        process->memory_kb = 0;

        std::string line;

        // Read file line by line
        while (std::getline(file, line))
        {
            // Find process name
            if (line.rfind("Name:", 0) == 0)
            {
                process->name = line.substr(6);
            }

            // Find memory usage
            if (line.rfind("VmRSS:", 0) == 0)
            {
                std::stringstream ss(line);

                std::string temp;
                ss >> temp;
                ss >> process->memory_kb;
            }
        }

        // Add the process to our list
        data->processes.push_back(process);
    }

    // Close directory
    closedir(proc);

    // Sort the processes
    std::sort(data->processes.begin(), data->processes.end(), compareProcesses);
}

SDL_Texture* createText(SDL_Renderer* renderer, TTF_Font* font, std::string text, SDL_Color color) // convert text to SDL texture
{
    SDL_Surface* surf = TTF_RenderText_Solid(font, text.c_str(), color);

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surf);

    SDL_FreeSurface(surf);

    return texture;
}

void buildGraphics(SDL_Renderer* renderer, AppData* data) // converts the process data into graphical objects
{
    clearGraphics(data);

    // RGB black
    SDL_Color color = {0, 0, 0};

    // Loop through processes and create one graphical row per process
    for (int i = 0; i < data->processes.size(); i++)
    {
        // Get process
        ProcessInfo* p = data->processes[i];

        // Create graphical object
        GProcess* g = new GProcess();

        // Create textures
        g->pid_texture = createText(renderer, data->font, std::to_string(p->pid), color);
        g->name_texture = createText(renderer, data->font, p->name, color);
        g->mem_texture = createText(renderer, data->font, std::to_string(p->memory_kb) + " KB", color);

        // Position rectangles
        g->pid_rect.x = 20;
        g->pid_rect.y = 60 + i * 35 + data->scroll_offset;
        g->pid_rect.w = 0;
        g->pid_rect.h = 0;

        g->name_rect.x = 150;
        g->name_rect.y = 60 + i * 35 + data->scroll_offset;
        g->name_rect.w = 0;
        g->name_rect.h = 0;
        
        g->mem_rect.x = 600;
        g->mem_rect.y = 60 + i * 35 + data->scroll_offset;
        g->mem_rect.w = 0;
        g->mem_rect.h = 0;
        
        SDL_QueryTexture(g->pid_texture, NULL, NULL, &g->pid_rect.w, &g->pid_rect.h);
        SDL_QueryTexture(g->name_texture, NULL, NULL, &g->name_rect.w, &g->name_rect.h);
        SDL_QueryTexture(g->mem_texture, NULL, NULL, &g->mem_rect.w, &g->mem_rect.h);

        // Store graphical entry
        data->graphics.push_back(g);
    }
}

void render(SDL_Renderer* renderer, AppData* data) // draws everything
{
    // Background color
    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);

    // Clear screen (erase previous frame)
    SDL_RenderClear(renderer);

    SDL_Color black = {0,0,0};

    // Header
    SDL_Texture* title = createText(renderer, data->font, "Task Manager", black);

    SDL_Rect title_rect = {20, 10, 0, 0};

    SDL_QueryTexture(title, NULL, NULL, &title_rect.w, &title_rect.h);

    // Draw the title
    SDL_RenderCopy(renderer, title, NULL, &title_rect);

    SDL_DestroyTexture(title);

    // Column labels
    SDL_Texture* pid_label = createText(renderer, data->font, "PID", black);
    SDL_Texture* name_label = createText(renderer, data->font, "Process Name", black);
    SDL_Texture* mem_label = createText(renderer, data->font, "Memory Usage", black);

    SDL_Rect pid_rect = {20, 40, 0, 0};
    SDL_Rect name_rect = {150, 40, 0, 0};
    SDL_Rect mem_rect = {600, 40, 0, 0};

    SDL_QueryTexture(pid_label, NULL, NULL, &pid_rect.w, &pid_rect.h);
    SDL_QueryTexture(name_label, NULL, NULL, &name_rect.w, &name_rect.h);
    SDL_QueryTexture(mem_label, NULL, NULL, &mem_rect.w, &mem_rect.h);

    SDL_RenderCopy(renderer, pid_label, NULL, &pid_rect);
    SDL_RenderCopy(renderer, name_label, NULL, &name_rect);
    SDL_RenderCopy(renderer, mem_label, NULL, &mem_rect);

    SDL_DestroyTexture(pid_label);
    SDL_DestroyTexture(name_label);
    SDL_DestroyTexture(mem_label);

    // Draw processes
    for (GProcess* g : data->graphics)
    {
        SDL_RenderCopy(renderer, g->pid_texture, NULL, &g->pid_rect);
        SDL_RenderCopy(renderer, g->name_texture, NULL, &g->name_rect);
        SDL_RenderCopy(renderer, g->mem_texture, NULL, &g->mem_rect);
    }

    // Display the rendered frame on the screen
    SDL_RenderPresent(renderer);
}