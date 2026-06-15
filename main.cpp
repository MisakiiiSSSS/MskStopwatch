#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <windows.h>
#include <string>
#include <ctime>

using std::string;

// 监测按键状态，返回是否按下
bool IsKeyDown(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

// 监测组合键状态，返回是否按下（只在第一次按下时返回true）
bool PressedCombo(int keyVk, int modVk1 = 0, int modVk2 = 0)
{
    static bool prevCombo = false;

    bool keyDown = IsKeyDown(keyVk);
    bool mod1Down = (modVk1 == 0) || IsKeyDown(modVk1);
    bool mod2Down = (modVk2 == 0) || IsKeyDown(modVk2);

    bool comboDown = keyDown && mod1Down && mod2Down;
    bool edge = comboDown && !prevCombo;

    prevCombo = comboDown;
    return edge;
}

class MskWindow
{
private:
    SDL_Window *m_window;
    SDL_Renderer *m_renderer;
    TTF_Font *m_font;

    // in config.json need to load
    int delayTime = 25; // ms
    int delayTimes = 10; // 延迟次数，防止多次触发
    int startStopKey = 0x20; // space键的虚拟键码

public:
    MskWindow()
    {
        SDL_Init(SDL_INIT_VIDEO);
        TTF_Init();
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        m_window = SDL_CreateWindow("Misaki's Stopwatch", 300, 100, SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP);
        m_renderer = SDL_CreateRenderer(m_window, 0);
        m_font = TTF_OpenFont("./res/font.ttf", 75);
        SDL_SetWindowPosition(m_window, 10, 10);
        SDL_SetWindowIcon(m_window, SDL_LoadBMP("./res/icon.bmp"));
        loadConfig();
    }
    ~MskWindow()
    {
        TTF_CloseFont(m_font);
        SDL_DestroyRenderer(m_renderer);
        SDL_DestroyWindow(m_window);
        TTF_Quit();
        SDL_Quit();
    }
private:
    void loadConfig()
    {
        std::ifstream configFile("./config.json");
        if (configFile.is_open())
        {
            nlohmann::json configJson;
            configFile >> configJson;
            if (configJson.contains("delayTime"))
                delayTime = configJson["delayTime"].get<int>();
            if (configJson.contains("delayTimes"))
                delayTimes = configJson["delayTimes"].get<int>();
            if (configJson.contains("startStopKey"))
            {
                string keyStr = configJson["startStopKey"].get<string>();
                if (!keyStr.empty())
                {
                    startStopKey = std::stoi(keyStr, nullptr, 16); // 以16进制解析键码
                }
            }
        }
        else
        {
            // 如果配置文件不存在，创建一个默认配置文件
            nlohmann::json defaultConfig = {
                {"delayTime", delayTime},
                {"delayTimes", delayTimes},
                {"startStopKey", to_hex_string(startStopKey)}
            };
            std::ofstream outFile("./config.json");
            outFile << defaultConfig.dump(4);
        }
    }
    string to_hex_string(int value)
    {
        char buffer[9]; // 8位十六进制 + 1位空字符
        snprintf(buffer, sizeof(buffer), "%02X", value);
        return string(buffer);
    }
private:
    bool isStarted = false;
    double timeBegin = 0.0;
    double timeCount = 0.0;
    SDL_Color m_color = {255, 255, 255, 255}; // white

    string timeToArr()
    {
        if (isStarted)
            timeCount = (clock() - timeBegin) / 1000.0;
        if (timeCount < 60)
            m_color = {0, 255, 0, 255}; // green
        else if (timeCount < 90)
            m_color = {255, 255, 0, 255}; // yellow
        else if (timeCount < 180)
            m_color = {255, 165, 0, 255}; // orange
        else
            m_color = {255, 0, 0, 255}; // red
        string timeArr(8, '0');
        int minutes = static_cast<int>(timeCount / 60);
        int seconds = static_cast<int>(timeCount) % 60;
        int milliseconds = static_cast<int>((timeCount - static_cast<int>(timeCount)) * 1000);
        timeArr[0] = minutes / 10 + '0';
        timeArr[1] = minutes % 10 + '0';
        timeArr[2] = ':';
        timeArr[3] = seconds / 10 + '0';
        timeArr[4] = seconds % 10 + '0';
        timeArr[5] = ':';
        timeArr[6] = milliseconds / 100 + '0';
        timeArr[7] = (milliseconds / 10) % 10 + '0';
        if (timeArr[0] == '9' && timeArr[1] == '9')
        {
            isStarted = false;
            timeBegin = 0.0;
            timeCount = 0.0;
        }
        return timeArr;
    }

    void renderNums()
    {
        SDL_Surface *surface = TTF_RenderText_Solid(m_font, timeToArr().c_str(), 0, m_color);
        SDL_Texture *texture = SDL_CreateTextureFromSurface(m_renderer, surface);
        SDL_DestroySurface(surface);
        SDL_FRect dstRect = {0, 0, static_cast<float>(surface->w), static_cast<float>(surface->h)};
        SDL_RenderTexture(m_renderer, texture, NULL, &dstRect);
        SDL_DestroyTexture(texture);
    }
    void render()
    {
        SDL_RenderClear(m_renderer);
        renderNums();
        SDL_RenderPresent(m_renderer);
    }

private:
    SDL_Event m_event;
    bool isRunning = true;

    int delayTimesCount = 0;

public:
    void run()
    {
        while (isRunning)
        {
            while (SDL_PollEvent(&m_event))
            {
                if (m_event.type == SDL_EVENT_QUIT)
                    isRunning = false;
                else if (m_event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                {
                    if (m_event.button.button == SDL_BUTTON_RIGHT)
                        isRunning = false;
                    else if (m_event.button.button == SDL_BUTTON_LEFT)
                    {
                        if (!isStarted)
                            timeBegin = clock();
                        isStarted = !isStarted;
                    }
                }
            }
            // handle space event to start/stop the stopwatch
            if (delayTimesCount == 0 && IsKeyDown(startStopKey))
            {
                if (!isStarted)
                    timeBegin = clock();
                isStarted = !isStarted;
                delayTimesCount++;
            }
            if (delayTimesCount > 0)
            {
                delayTimesCount++;
                if (delayTimesCount > delayTimes)
                    delayTimesCount = 0;
            }

            render();

            SDL_Delay(delayTime);
        }
    }
};

int main(int argc, char* argv[])
{
    MskWindow window;
    window.run();
    return 0;
}
