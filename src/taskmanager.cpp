#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <dirent.h>
#include <cstdint>
#include <cstdio>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#define WIDTH 1000
#define HEIGHT 800

enum SortMode
{
    SORT_CPU,
    SORT_MEMORY
};

typedef struct ProcessInfo
{
    int pid;
    std::string user;
    std::string name;
    bool foreground;
    bool user_process;
    float cpu_percent;
    long memory_kb;
} ProcessInfo;

typedef struct GProcess // graphical process
{
    SDL_Texture* icon_texture;
    SDL_Texture* pid_texture;
    SDL_Texture* name_texture;
    SDL_Texture* cpu_texture;
    SDL_Texture* mem_texture;

    SDL_Rect icon_rect;
    SDL_Rect pid_rect;
    SDL_Rect name_rect;
    SDL_Rect cpu_rect;
    SDL_Rect mem_rect;
} GProcess;

typedef struct AppData
{
    TTF_Font* font;

    SDL_Texture* foreground_icon;
    SDL_Texture* background_icon;

    std::vector<ProcessInfo*> processes;
    std::vector<GProcess*> graphics;

    int scroll_offset = 0;

    SortMode current_sort = SORT_CPU;
} AppData;

bool isNumber(const std::string& str);
bool compareCPU(ProcessInfo* a, ProcessInfo* b);
bool compareMemory(ProcessInfo* a, ProcessInfo* b);
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
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    // Create window and renderer
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &window, &renderer);

    // Create app state
    AppData data;

    SDL_Cursor* arrow_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    SDL_Cursor* hand_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);

    // Load font
    data.font = TTF_OpenFont("resrc/fonts/OpenSans-Regular.ttf", 24);

    // Load icons
    SDL_Surface* fg_surface = IMG_Load("resrc/images/foreground_icon.png");
    data.foreground_icon = SDL_CreateTextureFromSurface(renderer, fg_surface);
    SDL_FreeSurface(fg_surface);

    SDL_Surface* bg_surface = IMG_Load("resrc/images/background_icon.png");
    data.background_icon = SDL_CreateTextureFromSurface(renderer, bg_surface);
    SDL_FreeSurface(bg_surface);

    // Load the processes
    loadProcesses(&data);

    // Build the graphics
    buildGraphics(renderer, &data);

    bool running = true;

    uint32_t last_update = SDL_GetTicks();

    while (running)
    {
        // Mouse position
        int mouse_x, mouse_y;
        SDL_GetMouseState(&mouse_x, &mouse_y);

        // Check if hovering over buttons
        bool hovering_button = false;

        if ((mouse_x >= 20 && mouse_x <= 200 && mouse_y >= 45 + data.scroll_offset && mouse_y <= 85 + data.scroll_offset) || (mouse_x >= 220 && mouse_x <= 440 && mouse_y >= 45 + data.scroll_offset && mouse_y <= 85 + data.scroll_offset))
        {
            hovering_button = true;
        }

        // Change cursor
        if (hovering_button)
        {
            SDL_SetCursor(hand_cursor);
        }
        else
        {
            SDL_SetCursor(arrow_cursor);
        }

        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }

            if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                int mouse_x = event.button.x;
                int mouse_y = event.button.y;

                // CPU button
                if (mouse_x >= 20 && mouse_x <= 200 && mouse_y >= 45 + data.scroll_offset && mouse_y <= 85 + data.scroll_offset)
                {
                    data.current_sort = SORT_CPU;

                    loadProcesses(&data);
                    buildGraphics(renderer, &data);
                }

                // Memory button
                if (mouse_x >= 220 && mouse_x <= 440 && mouse_y >= 45 + data.scroll_offset && mouse_y <= 85 + data.scroll_offset)
                {
                    data.current_sort = SORT_MEMORY;

                    loadProcesses(&data);
                    buildGraphics(renderer, &data);
                }
            }

            if (event.type == SDL_MOUSEWHEEL)
            {
                data.scroll_offset += event.wheel.y * 20;

                // Prevent scrolling too far down
                if (data.scroll_offset > 0)
                {
                    data.scroll_offset = 0;
                }

                // Calculate bottom limit
                int content_height = 120 + (data.processes.size() * 35);

                int min_scroll = HEIGHT - content_height;

                // Prevent scrolling too far up
                if (data.scroll_offset < min_scroll)
                {
                    data.scroll_offset = min_scroll;
                }

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

    SDL_DestroyTexture(data.foreground_icon);
    SDL_DestroyTexture(data.background_icon);

    SDL_FreeCursor(arrow_cursor);
    SDL_FreeCursor(hand_cursor);

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

bool compareCPU(ProcessInfo* a, ProcessInfo* b)
{
    return a->cpu_percent > b->cpu_percent;
}

bool compareMemory(ProcessInfo* a, ProcessInfo* b)
{
    return a->memory_kb > b->memory_kb;
}

void clearGraphics(AppData* data)
{
    for (GProcess* g : data->graphics)
    {
        SDL_DestroyTexture(g->pid_texture);
        SDL_DestroyTexture(g->name_texture);
        SDL_DestroyTexture(g->cpu_texture);
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
    // Clear old process list
    clearProcesses(data);

    // Run the ps command
    FILE* pipe = popen("ps -eo user,pid,stat,comm,pcpu,rss --no-headers", "r");

    // If ps failed, stop
    if (!pipe)
    {
        return;
    }

    char buffer[256];

    // Read output line-by-line
    while (fgets(buffer, sizeof(buffer), pipe))
    {
        std::stringstream ss(buffer);

        // Create process object
        ProcessInfo* process = new ProcessInfo();

        // Parse:
        // PID STAT COMMAND CPU MEMORY
        std::string stat;
        ss >> process->user >> process->pid >> stat >> process->name >> process->cpu_percent >> process->memory_kb;

        process->user_process = (process->user != "root");
        process->foreground = (stat.find('+') != std::string::npos);

        // Add the process to our list
        data->processes.push_back(process);
    }

    pclose(pipe);

    // Sort the processes
    if (data->current_sort == SORT_CPU)
    {
        std::sort(data->processes.begin(), data->processes.end(), compareCPU);
    }
    else
    {
        std::sort(data->processes.begin(), data->processes.end(), compareMemory);
    }
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

        // Assign icons (processes are categorized by those that are running under the user account, and those that are running under root or any other account)
        if (p->user_process)
        {
            g->icon_texture = data->foreground_icon;
        }
        else
        {
            g->icon_texture = data->background_icon;
        }

        // Create textures
        g->pid_texture = createText(renderer, data->font, std::to_string(p->pid), color);
        g->name_texture = createText(renderer, data->font, p->name, color);
        g->cpu_texture = createText(renderer, data->font, std::to_string(p->cpu_percent) + "%", color);
        g->mem_texture = createText(renderer, data->font, std::to_string(p->memory_kb) + " KB", color);

        // Position rectangles
        g->icon_rect.x = 10;
        g->icon_rect.y = 120 + i * 35 + data->scroll_offset;
        g->icon_rect.w = 24;
        g->icon_rect.h = 24;

        g->pid_rect.x = 50;
        g->pid_rect.y = 120 + i * 35 + data->scroll_offset;
        g->pid_rect.w = 0;
        g->pid_rect.h = 0;

        g->name_rect.x = 170;
        g->name_rect.y = 120 + i * 35 + data->scroll_offset;
        g->name_rect.w = 0;
        g->name_rect.h = 0;

        g->cpu_rect.x = 470;
        g->cpu_rect.y = 120 + i * 35 + data->scroll_offset;
        g->cpu_rect.w = 0;
        g->cpu_rect.h = 0;
        
        g->mem_rect.x = 720;
        g->mem_rect.y = 120 + i * 35 + data->scroll_offset;
        g->mem_rect.w = 0;
        g->mem_rect.h = 0;
        
        SDL_QueryTexture(g->pid_texture, NULL, NULL, &g->pid_rect.w, &g->pid_rect.h);
        SDL_QueryTexture(g->name_texture, NULL, NULL, &g->name_rect.w, &g->name_rect.h);
        SDL_QueryTexture(g->cpu_texture, NULL, NULL, &g->cpu_rect.w, &g->cpu_rect.h);
        SDL_QueryTexture(g->mem_texture, NULL, NULL, &g->mem_rect.w, &g->mem_rect.h);

        // Store graphical entry
        data->graphics.push_back(g);
    }
}

void render(SDL_Renderer* renderer, AppData* data) // draws everything
{
    // Background color
    SDL_SetRenderDrawColor(renderer, 240, 247, 255, 255);

    // Clear screen (erase previous frame)
    SDL_RenderClear(renderer);

    SDL_Color black = {0,0,0};

    // Sort buttons
    SDL_Rect cpu_button = {20, 45 + data->scroll_offset, 180, 40};
    SDL_Rect mem_button = {220, 45 + data->scroll_offset, 220, 40};

    // CPU button color
    if (data->current_sort == SORT_CPU)
    {
        SDL_SetRenderDrawColor(renderer, 120, 170, 255, 255);
    }
    else
    {
        SDL_SetRenderDrawColor(renderer, 180, 210, 255, 255);
    }

    SDL_RenderFillRect(renderer, &cpu_button);

    // Memory button color
    if (data->current_sort == SORT_MEMORY)
    {
        SDL_SetRenderDrawColor(renderer, 120, 170, 255, 255);
    }
    else
    {
        SDL_SetRenderDrawColor(renderer, 180, 210, 255, 255);
    }

    SDL_RenderFillRect(renderer, &mem_button);

    SDL_Texture* cpu_sort_text = createText(renderer, data->font, "Sort by CPU", black);

    SDL_Texture* mem_sort_text = createText(renderer, data->font, "Sort by Memory", black);

    SDL_Rect cpu_sort_rect = {35, 55 + data->scroll_offset, 0, 0};
    SDL_Rect mem_sort_rect = {235, 55 + data->scroll_offset, 0, 0};

    SDL_QueryTexture(cpu_sort_text, NULL, NULL, &cpu_sort_rect.w, &cpu_sort_rect.h);

    SDL_QueryTexture(mem_sort_text, NULL, NULL, &mem_sort_rect.w, &mem_sort_rect.h);

    SDL_RenderCopy(renderer, cpu_sort_text, NULL, &cpu_sort_rect);
    SDL_RenderCopy(renderer, mem_sort_text, NULL, &mem_sort_rect);

    SDL_DestroyTexture(cpu_sort_text);
    SDL_DestroyTexture(mem_sort_text);

    // Header
    SDL_Texture* title = createText(renderer, data->font, "OS Task Manager", black);

    SDL_Rect title_rect = {20, 10 + data->scroll_offset, 0, 0};

    SDL_QueryTexture(title, NULL, NULL, &title_rect.w, &title_rect.h);

    // Draw the title
    SDL_RenderCopy(renderer, title, NULL, &title_rect);

    SDL_DestroyTexture(title);

    // Column labels
    SDL_Texture* pid_label = createText(renderer, data->font, "PID", black);
    SDL_Texture* name_label = createText(renderer, data->font, "Process Name", black);
    SDL_Texture* cpu_label = createText(renderer, data->font, "CPU %", black);
    SDL_Texture* mem_label = createText(renderer, data->font, "Memory Usage", black);

    SDL_Rect pid_rect = {50, 85 + data->scroll_offset, 0, 0};
    SDL_Rect name_rect = {170, 85 + data->scroll_offset, 0, 0};
    SDL_Rect cpu_rect = {470, 85 + data->scroll_offset, 0, 0};
    SDL_Rect mem_rect = {720, 85 + data->scroll_offset, 0, 0};

    SDL_QueryTexture(pid_label, NULL, NULL, &pid_rect.w, &pid_rect.h);
    SDL_QueryTexture(name_label, NULL, NULL, &name_rect.w, &name_rect.h);
    SDL_QueryTexture(cpu_label, NULL, NULL, &cpu_rect.w, &cpu_rect.h);
    SDL_QueryTexture(mem_label, NULL, NULL, &mem_rect.w, &mem_rect.h);

    SDL_RenderCopy(renderer, pid_label, NULL, &pid_rect);
    SDL_RenderCopy(renderer, name_label, NULL, &name_rect);
    SDL_RenderCopy(renderer, cpu_label, NULL, &cpu_rect);
    SDL_RenderCopy(renderer, mem_label, NULL, &mem_rect);

    SDL_DestroyTexture(pid_label);
    SDL_DestroyTexture(name_label);
    SDL_DestroyTexture(cpu_label);
    SDL_DestroyTexture(mem_label);

    // Draw processes
    for (int i = 0; i < data->graphics.size(); i++)
    {
        GProcess* g = data->graphics[i];

        // Zebra stripe background
        SDL_Rect row_rect;
        row_rect.x = 0;
        row_rect.y = g->pid_rect.y - 2;
        row_rect.w = WIDTH;
        row_rect.h = 32;

        // Alternate row colors
        if (i % 2 == 0)
        {
            SDL_SetRenderDrawColor(renderer, 235, 245, 255, 255);
        }
        else
        {
            SDL_SetRenderDrawColor(renderer, 220, 235, 250, 255);
        }

        SDL_RenderFillRect(renderer, &row_rect);

        // Draw process info on top
        SDL_RenderCopy(renderer, g->icon_texture, NULL, &g->icon_rect);
        SDL_RenderCopy(renderer, g->pid_texture, NULL, &g->pid_rect);
        SDL_RenderCopy(renderer, g->name_texture, NULL, &g->name_rect);
        SDL_RenderCopy(renderer, g->cpu_texture, NULL, &g->cpu_rect);
        SDL_RenderCopy(renderer, g->mem_texture, NULL, &g->mem_rect);
    }

    // Display the rendered frame on the screen
    SDL_RenderPresent(renderer);
}