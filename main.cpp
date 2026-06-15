#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <windows.h>
#include <string>
#include <ctime>
#include <vector>
#include <filesystem>
#include <iostream>

using std::string;
using std::vector;

// 常量定义
namespace constNums
{
    const int DEFAULT_POS_X = 10;
    const int DEFAULT_POS_Y = 10;
    const int DEFAULT_WINDOW_WIDTH = 300;
    const int DEFAULT_WINDOW_HEIGHT = 100;
    const int DEFAULT_FONT_SIZE = 75;        // 字体大小
    const int DEFAULT_DELAY_TIME = 25;       // ms
    const int DEFAULT_DELAY_TIMES = 10;      // 延迟次数，防止多次触发
    const int DEFAULT_START_STOP_KEY = 0x20; // space键的虚拟键码
    const int MAX_TIME = 99 * 60 + 59;       // 最大时间为99:59
}

// 将整数转换为两位十六进制字符串
string to_hex_string(int value)
{
    char buffer[9]; // 8位十六进制 + 1位空字符
    snprintf(buffer, sizeof(buffer), "%02X", value);
    return string(buffer);
}

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

// 定义颜色范围结构体
struct ColorRange
{
    SDL_Color color;
    int timeBegin;
    int timeEnd;
};

// 窗口类，负责创建窗口、渲染时间和处理事件
class MskWindow
{
private:
    SDL_Window *m_window;
    SDL_Renderer *m_renderer;
    TTF_Font *m_font;
    string root = "./"; // 可执行文件所在目录

    // in config.json need to load
    SDL_Color windowBackgroundColor = {0, 0, 0, 255};                           // black
    int posX = constNums::DEFAULT_POS_X;                                        // 窗口位置
    int posY = constNums::DEFAULT_POS_Y;                                        // 窗口位置
    int windowWidth = constNums::DEFAULT_WINDOW_WIDTH;                          // 窗口宽高
    int windowHeight = constNums::DEFAULT_WINDOW_HEIGHT;                        // 窗口宽高
    int fontSize = constNums::DEFAULT_FONT_SIZE;                                // 字体大小
    int delayTime = constNums::DEFAULT_DELAY_TIME;                              // ms
    int delayTimes = constNums::DEFAULT_DELAY_TIMES;                            // 延迟次数，防止多次触发
    int startStopKey = constNums::DEFAULT_START_STOP_KEY;                       // space键的虚拟键码
    vector<ColorRange> colorRanges{{{255, 0, 0, 255}, 2, constNums::MAX_TIME}}; // 默认时间超过1:30时变红

public:
    MskWindow()
    {
        loadConfig();
        SDL_Init(SDL_INIT_VIDEO);
        TTF_Init();
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        m_window = SDL_CreateWindow("Misaki's Stopwatch", windowWidth, windowHeight, SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_TRANSPARENT);
        m_renderer = SDL_CreateRenderer(m_window, 0);
        m_font = TTF_OpenFont((root + "res/font.ttf").c_str(), fontSize);
        SDL_SetWindowPosition(m_window, posX, posY);
        SDL_SetWindowIcon(m_window, SDL_LoadBMP((root + "res/icon.bmp").c_str()));
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
        const std::filesystem::path configPath = std::filesystem::path(root) / "config.json";

        auto writeDefaultConfig = [&]()
        {
            nlohmann::json colorRangeJson = {
                {"r", 255},
                {"g", 0},
                {"b", 0},
                {"a", 255},
                {"timeBegin", 120},
                {"timeEnd", constNums::MAX_TIME}};

            nlohmann::json defaultConfig = {
                {"posX", posX},
                {"posY", posY},
                {"windowWidth", windowWidth},
                {"windowHeight", windowHeight},
                {"fontSize", fontSize},
                {"delayTime", delayTime},
                {"delayTimes", delayTimes},
                {"startStopKey", to_hex_string(startStopKey)},
                {"windowBackgroundColor", {{"r", windowBackgroundColor.r}, {"g", windowBackgroundColor.g}, {"b", windowBackgroundColor.b}, {"a", windowBackgroundColor.a}}},
                {"colorRanges", nlohmann::json::array({colorRangeJson})}};

            std::filesystem::create_directories(configPath.parent_path());

            std::ofstream outFile(configPath);
            if (outFile.is_open())
            {
                outFile << defaultConfig.dump(4);
                SDL_Log("已创建默认配置文件: %s", configPath.string().c_str());
            }
            else
            {
                SDL_Log("无法创建配置文件: %s", configPath.string().c_str());
            }
        };

        if (std::filesystem::exists(configPath))
        {
            SDL_Log("配置文件存在，加载配置文件: %s", configPath.string().c_str());

            std::ifstream configFile(configPath);
            if (!configFile.is_open())
            {
                SDL_Log("配置文件打开失败，使用默认配置");
                writeDefaultConfig();
                return;
            }

            try
            {
                nlohmann::json configJson;
                configFile >> configJson;

                if (configJson.contains("posX") && configJson["posX"].is_number_integer())
                    posX = configJson["posX"].get<int>();
                if (configJson.contains("posY") && configJson["posY"].is_number_integer())
                    posY = configJson["posY"].get<int>();
                if (configJson.contains("windowWidth") && configJson["windowWidth"].is_number_integer())
                    windowWidth = configJson["windowWidth"].get<int>();
                if (configJson.contains("windowHeight") && configJson["windowHeight"].is_number_integer())
                    windowHeight = configJson["windowHeight"].get<int>();
                if (configJson.contains("fontSize") && configJson["fontSize"].is_number_integer())
                    fontSize = configJson["fontSize"].get<int>();
                if (configJson.contains("delayTime") && configJson["delayTime"].is_number_integer())
                    delayTime = configJson["delayTime"].get<int>();
                if (configJson.contains("delayTimes") && configJson["delayTimes"].is_number_integer())
                    delayTimes = configJson["delayTimes"].get<int>();

                if (configJson.contains("startStopKey"))
                {
                    if (configJson["startStopKey"].is_string())
                    {
                        std::string keyStr = configJson["startStopKey"].get<std::string>();
                        if (!keyStr.empty())
                            startStopKey = std::stoi(keyStr, nullptr, 16);
                    }
                    else if (configJson["startStopKey"].is_number_integer())
                    {
                        startStopKey = configJson["startStopKey"].get<int>();
                    }
                }

                if (configJson.contains("colorRanges") && configJson["colorRanges"].is_array())
                {
                    colorRanges.clear();

                    for (const auto &rangeJson : configJson["colorRanges"])
                    {
                        if (!rangeJson.is_object())
                            continue;

                        if (rangeJson.contains("r") && rangeJson.contains("g") &&
                            rangeJson.contains("b") && rangeJson.contains("a") &&
                            rangeJson.contains("timeBegin") && rangeJson.contains("timeEnd"))
                        {
                            ColorRange range;
                            range.color = {
                                static_cast<Uint8>(rangeJson["r"].get<int>()),
                                static_cast<Uint8>(rangeJson["g"].get<int>()),
                                static_cast<Uint8>(rangeJson["b"].get<int>()),
                                static_cast<Uint8>(rangeJson["a"].get<int>())};

                            range.timeBegin = rangeJson["timeBegin"].get<int>();
                            range.timeEnd = rangeJson["timeEnd"].get<int>();
                            colorRanges.push_back(range);
                        }
                    }

                    if (colorRanges.empty())
                    {
                        colorRanges = {{{255, 0, 0, 255}, 120, constNums::MAX_TIME}};
                    }
                }

                if (configJson.contains("windowBackgroundColor") &&
                    configJson["windowBackgroundColor"].is_object())
                {
                    const auto &bgColorJson = configJson["windowBackgroundColor"];
                    if (bgColorJson.contains("r") && bgColorJson.contains("g") &&
                        bgColorJson.contains("b") && bgColorJson.contains("a"))
                    {
                        windowBackgroundColor = {
                            static_cast<Uint8>(bgColorJson["r"].get<int>()),
                            static_cast<Uint8>(bgColorJson["g"].get<int>()),
                            static_cast<Uint8>(bgColorJson["b"].get<int>()),
                            static_cast<Uint8>(bgColorJson["a"].get<int>())};
                    }
                }
            }
            catch (const std::exception &e)
            {
                SDL_Log("配置文件解析失败: %s", e.what());
                writeDefaultConfig();
            }
        }
        else
        {
            SDL_Log("配置文件不存在，正在创建默认配置文件...");
            writeDefaultConfig();
        }
    }

private:
    bool isStarted = false;
    double timeBegin = 0.0;
    double timeCount = 0.0;
    SDL_Color m_color = {255, 255, 255, 255}; // white

    string timeToArr()
    {
        if (timeCount > constNums::MAX_TIME)
        {
            timeCount = constNums::MAX_TIME;
            isStarted = false; // 超过最大时间后停止计时
        }
        if (isStarted)
            timeCount = (clock() - timeBegin) / 1000.0;
        // 检查当前时间属于哪个颜色范围
        m_color = {255, 255, 255, 255}; // 默认白色
        for (const auto &range : colorRanges)
        {
            if (timeCount >= range.timeBegin && timeCount < range.timeEnd)
            {
                m_color = range.color;
                break;
            }
        }
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
        return timeArr;
    }

    void renderNums()
    {
        SDL_Surface *surface = TTF_RenderText_Blended(m_font, timeToArr().c_str(), 0, m_color);
        SDL_Texture *texture = SDL_CreateTextureFromSurface(m_renderer, surface);
        SDL_FRect dstRect = {0, 0, static_cast<float>(surface->w), static_cast<float>(surface->h)};
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND); // 设置混合模式以支持透明度
        SDL_RenderTexture(m_renderer, texture, NULL, &dstRect);
        SDL_DestroySurface(surface);
        SDL_DestroyTexture(texture);
    }
    void render()
    {
        SDL_SetRenderDrawColor(m_renderer, windowBackgroundColor.r, windowBackgroundColor.g, windowBackgroundColor.b, windowBackgroundColor.a);
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

int main(int argc, char *argv[])
{
    MskWindow window;
    window.run();
    return 0;
}
