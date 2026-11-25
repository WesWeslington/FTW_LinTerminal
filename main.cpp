#include <chrono>
#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

// OS Specific
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/input.h>
#include <fcntl.h>
#include <glob.h>

using namespace std;

 /*
 *  Helpers
*/

string TimestampedString(const string& Message)
{
    return std::format("[{:%Y-%m-%d %H:%M:%S UTC}] {}", 
            chrono::floor<chrono::seconds>(chrono::system_clock::now()), Message);
}

#define Printf(Message, ...) \
    Print(std::format(Message, ##__VA_ARGS__));

#define Errorf(Message, ...) \
    Error(std::format(Message, ##__VA_ARGS__));

void Print(string Message)
{
    cout << TimestampedString(Message) << endl;
}

void Error(const string &Message)
{
    perror(TimestampedString(Message).c_str());
    cout << endl;
}

 /*
 *    Rendering Boilerplate
*/

// Necessary to render over regular terminal
static int ConsoleFoundDevice = -1;
static void RestoreConsole()
{
    if(ConsoleFoundDevice != -1)
    {
        ioctl(ConsoleFoundDevice, KDSETMODE, KD_TEXT);
        close(ConsoleFoundDevice);
        ConsoleFoundDevice = -1;
    }
}

static void OnSignal(int Sig)
{
    RestoreConsole();
    signal(Sig, SIG_DFL);
    raise(Sig);
}

bool TakeOverConsole()
{
    // /dev/tty0 = whichever console is currently on screen
    ConsoleFoundDevice = open("/dev/tty0", O_RDWR);
    if(ConsoleFoundDevice == -1) { return false; }

    if(ioctl(ConsoleFoundDevice, KDSETMODE, KD_GRAPHICS) == -1)
    {
        close(ConsoleFoundDevice);
        ConsoleFoundDevice = -1;
        return false;
    }

    atexit(RestoreConsole);
    signal(SIGINT, OnSignal);
    signal(SIGTERM, OnSignal);
    signal(SIGSEGV, OnSignal);
    signal(SIGABRT, OnSignal);
    return true;
}

// Global Defaults
static const int GameWidth = 320;
static const int GameHeight = 240;
static const int PixelScale = 2;
static uint16_t GameBuffer[GameWidth * GameHeight];

struct Color16
{
    uint16_t Value;
    Color16(uint8_t R, uint8_t G, uint8_t B)
    {
        Value = ((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3); 
    }
};

void DrawPixel(int PosX, int PosY, Color16 Color)
{  
    if(PosX < 0 || PosX >= GameWidth || PosY < 0 || PosY >= GameHeight) { return; }

    GameBuffer[PosY * GameWidth + PosX] = Color.Value;
};

#define CLEAR_COLOR    Color16(0, 128, 128)
#define RED            Color16(255, 0, 0)
#define GREEN          Color16(0, 255, 0)
#define BLUE           Color16(0, 0, 255)
#define CYAN           Color16(0, 255, 255)
#define MAGENTA        Color16(255, 0, 255)
#define YELLOW         Color16(255, 255, 0)
#define ORANGE         Color16(255, 128, 0)
#define WHITE          Color16(255, 255, 255)
#define BLACK          Color16(0, 0, 0)

void Clear(Color16 Color)
{
    for(int Y = 0; Y < GameHeight; Y++)
    {
        for(int X = 0; X < GameWidth; X++)
        {
            DrawPixel(X, Y, Color);
        }
    }
}

void DrawRectangle(int PosX, int PosY, int SizeX, int SizeY, Color16 Color)
{ 
    for(int Y = PosY; Y < PosY + SizeY; Y++)
    {
        for(int X = PosX; X < PosX + SizeX; X++)
        {
            DrawPixel(X, Y, Color);
        }
    }
}

static inline uint32_t RGB565ToRGBA32(uint16_t Color565)
{
    uint8_t R = ((Color565 >> 11) & 0x1F) << 3;
    uint8_t G = ((Color565 >> 5) & 0x3F) << 2;
    uint8_t B = (Color565 & 0x1F) << 3;
    return (0xFF << 24) | (R << 16) | (G << 8) | B;
}

// Returns the ID of the FrameBuffer device (display)
int InitializeFrameBufferDevice()
{
    const char* FrameBufferDevice = getenv("FRAMEBUFFER");
    int FrameDevice = open(FrameBufferDevice ? FrameBufferDevice : "/dev/fb1", O_RDWR);
    if(FrameDevice == -1 && ! FrameBufferDevice) 
    {
        FrameDevice = open("/dev/fb0", O_RDWR);
    }

    return FrameDevice;
}

int FrameBufferDevice = -1;

bool InitializeFrameBuffer()
{
    FrameBufferDevice = InitializeFrameBufferDevice();
    ioctl(FrameBufferDevice, EVIOCGRAB, 1);
    if(FrameBufferDevice < 0)
    {
        Error("Was not able to initialize frame buffer!");
        return false;
    }

    return true;
}

 /*
 *    Input Boilerplate
*/

class Input
{

public:

    Input()
    {
        DeviceID = InitializeDevice();
        if(DeviceID < 0)
        {
            Error("Was not able to initialize keyboard.");
        }
        else
        {
            Print("Successfully initialized keyboard!");
            ioctl(DeviceID, EVIOCGRAB, 1);
        }
    }

    // Returns the ID of the found input device, -1 if failed
    int InitializeDevice()
    {
        glob_t Glob;
        if(glob("/dev/input/by-id/*-event-kbd", 0, nullptr, &Glob) != 0)
        {
            return -1;
        }

        int FoundDevice = -1;
        for(size_t Index = 0; Index < Glob.gl_pathc; ++Index)
        {
            string Path = Glob.gl_pathv[Index];
            if(Path.find("-if0") != string::npos)
            {
                continue;
            }
            FoundDevice = open(Path.c_str(), O_RDONLY | O_NONBLOCK);
            if(FoundDevice >= 0) break;
        }
        globfree(&Glob);
        return FoundDevice;
    }

    int GetDeviceID(){ return DeviceID; };

private:

    int DeviceID = -1;
    input_event Ev;
};

enum class InputKey
{
    Up, 
    Down, 
    Right, 
    Left,   
    Shoot, 
    Jump,  
    Quit   
};

map<int, InputKey> InputMap = 
{
    { KEY_UP,     InputKey::Up    },
    { KEY_DOWN,   InputKey::Down  },
    { KEY_RIGHT,  InputKey::Right },
    { KEY_LEFT,   InputKey::Left  },
    { KEY_G,      InputKey::Shoot },
    { KEY_SPACE,  InputKey::Jump  },
    { KEY_Q,      InputKey::Quit  }
};

 /*
 *  Entity
*/
class Game;
class Entity
{
public:
    friend Game;
    
    Entity(){}
    virtual ~Entity(){}
    
    const bool& IsAlive() { return bIsAlive;}

    void Kill()
    {
        bIsAlive = false;
    }

protected:

    virtual void Update(){}
    virtual void FixedUpdate(){}
    virtual void Render(){}
    bool bIsAlive = true;
};

 /*
 *   Static Globals
*/
vector<unique_ptr<Entity>> Entities;
int Gravity = 10;

/*
 *  Player
*/

class Player : public Entity
{

public:
    Player()
    {
        Print("Player has spawned!");
    }

    ~Player()
    {
        Print("Player has been deleted!");
    }
    
    void Shoot()
    {
    }

    void CancelShoot()
    {

    }
    
    void Jump()
    {
    }

    //TODO: Maybe a CancelJump() to have better control of jump height?

    int PositionX = 0;
    int PositionY = 0;
    int Speed = 4;

    int InputX = 0;
    int InputY = 0;

    void Move(const int& X, const int& Y)
    {
        PositionX += X;
        PositionY += Y;
        
        InputX = X;
        InputY = Y;
    }

protected:

    virtual void Update() override
    {

    }

    virtual void FixedUpdate() override
    {
        if(InputX != 0)
        {
            PositionX += InputX * Speed;
        }

        if(InputY != 0)
        {
            PositionY += InputY * Speed;
        }
    }

    Color16 Color = GREEN;

    virtual void Render() override
    {
        DrawRectangle(PositionX, PositionY * -1, 16, 16, Color);
    }
};

// Wall is also used as floor
class Wall : public Entity
{

public:
    Wall(int PositionX, int PositionY, int SizeX, int SizeY, Color16 Color):
       PositionX(PositionX), PositionY(PositionY), SizeX(SizeX), SizeY(SizeY), Color(Color)
    {}

protected:

    virtual void Render() override
    {
        DrawRectangle(PositionX + 1, PositionY + 1, SizeX - 1, SizeY - 1, Color); 

        // Black outline B)
        DrawRectangle(PositionX, PositionY,         SizeX, 1, BLACK);
        DrawRectangle(PositionX, PositionY + SizeY, SizeX + 1, 1, BLACK);
        DrawRectangle(PositionX, PositionY, 1,         SizeY, BLACK);
        DrawRectangle(PositionX + SizeX, PositionY, 1, SizeY, BLACK);
    }

private:

    int PositionX;
    int PositionY;
    int SizeX;
    int SizeY;
    Color16 Color;
};

 /*
 *  Game
*/

class Game
{
 
public:
    
    Game()
    {
        assert(TakeOverConsole() && "Console not found");
    }

    ~Game()
    {
        RestoreConsole();
    }
    void Initialize()
    {
        StartTime = chrono::steady_clock::now();
        FoundKeyboard = GameInput.GetDeviceID();

        if(InitializeFrameBuffer())
        {
            Print("Successfully initialized frame buffer!");
            InitializeRenderVariables();
        }
        else
        {
            Error("Failed to initialize Frame Buffer");
        }
    }

    void GameLoop()
    {
        Initialize(); 
        SpawnPlayer();
        SpawnLevel1();
        while(bIsActive)
        {                   
            Update();
            CheckInput();
            auto Now = chrono::steady_clock::now(); 
            if(Now - LastFixedFrame >= (1000ms / FixedFPS))
            {       
                FixedUpdate();
                Render();
            }
            CheckTrackedEntities();

            // Without this, we will run at >1,000,000 FPS
            this_thread::sleep_until(Now + FrameTime());
        }
    }

    void EndGame()
    {
        bIsActive = false;
        Print("Goodbye world!");
        Printf("Update Frames: {}\nFixed Frames: {}\n\n", UpdateFrames, FixedFrames);
    }
    
    chrono::steady_clock::time_point StartTime;
    chrono::steady_clock::time_point LastFixedFrame;
    chrono::milliseconds DeltaTime;

    void Update()
    {
        UpdateFrames++;
        UpdateEntities();    
    }
    
    void UpdateEntities()
    {
        for(auto& CurEntity : Entities)
        {
            CurEntity->Update();
        }
    }

    void FixedUpdate()
    {
        LastFixedFrame = chrono::steady_clock::now(); 
        FixedFrames++;
        
        FixedUpdateEntities();
    }
    
    void FixedUpdateEntities()
    {
        for(auto& CurEntity : Entities)
        {
            CurEntity->FixedUpdate();
        }
    }
   

    void InitializeRenderVariables()
    {
        fb_fix_screeninfo FixedInfo;
        fb_var_screeninfo VariableInfo;
        ioctl(FrameBufferDevice, FBIOGET_FSCREENINFO, &FixedInfo);
        ioctl(FrameBufferDevice, FBIOGET_VSCREENINFO, &VariableInfo);

        Height = VariableInfo.yres;
        LineLength = FixedInfo.line_length;
        BitsPerPixel = VariableInfo.bits_per_pixel; // 16 or 32
        PageSize = static_cast<size_t>(Height) * LineLength;

    } 
    
    int Height;
    int LineLength;
    int BitsPerPixel;
    size_t PageSize;

    void Render()
    { 
        uint8_t* BackBuffer = new uint8_t[PageSize]();

        //Draw Background
        Clear(CLEAR_COLOR);
        
        for(auto& CurEntity : Entities)
        {
            CurEntity->Render();
        }
        
        for(int GY = 0; GY < GameHeight; GY++)
        {
            for(int SY = 0; SY < PixelScale; SY++)
            {
                int DestRow = GY * PixelScale + SY;
                if(DestRow >= Height) { break; }
                uint8_t* Row = BackBuffer + DestRow * LineLength;

                for(int GX = 0; GX < GameWidth; GX++)
                {
                    uint16_t Pixel = GameBuffer[GY * GameWidth + GX];
                    
                    for(int SX = 0; SX < PixelScale; SX++)
                    {
                        int DestX = GX * PixelScale + SX;
                        
                        // TODO: Hide this behind an ifdef
                        //if(BitsPerPixel == 16)
                        //{
                           reinterpret_cast<uint16_t*>(Row)[DestX] = Pixel;
                        //}
                        //else
                        //{
                        //  reinterpret_cast<uint16_t*>(Row)[DestX] = RGB565ToRGBA32(Pixel);
                        //}
                    }
                }
            }
        }
        

        pwrite(FrameBufferDevice, BackBuffer, PageSize, 0);
        
        delete[] BackBuffer;
    }

    void CheckTrackedEntities()
    {
        erase_if(Entities, [](const unique_ptr<Entity>& E){ return !E || !E->IsAlive(); } );
    }

    void CheckInput()
    {
        input_event Ev;
        if(read(FoundKeyboard, &Ev, sizeof(Ev)) == sizeof(Ev))
        {
            if(Ev.value != 2 && InputMap.contains(Ev.code))
            {   
                switch(InputMap[Ev.code])
                {
                    case(InputKey::Jump):
                        if(Ev.value == 1)
                        {
                            Jump();
                        }
                        break;
                    case(InputKey::Shoot):
                        if(Ev.value == 1)
                        {
                            ShootDown();
                        }
                        else if(Ev.value == 0)
                        {
                            ShootReleased();
                        }
                        break;
                    case(InputKey::Up):
                       
                        if(Ev.value == 1)
                        {
                            ArrowKeyDown(0, 1);
                        }
                        else if(Ev.value == 0)
                        {
                            ArrowKeyReleased(0, 1);
                        }
                        break;
                    case(InputKey::Down):
                        
                        if(Ev.value == 1)
                        {
                            ArrowKeyDown(0, -1);
                        }
                        else if(Ev.value == 0)
                        {
                            ArrowKeyReleased(0, -1);
                        }
                        break;
                    case(InputKey::Right):
                        if(Ev.value == 1)
                        {
                            ArrowKeyDown(1, 0);
                        }
                        else if(Ev.value == 0)
                        {
                            ArrowKeyReleased(1, 0);
                        }
                        break;
                    case(InputKey::Left):
                        if(Ev.value == 1)
                        {
                            ArrowKeyDown(-1, 0);
                        }
                        else if(Ev.value == 0)
                        {
                            ArrowKeyReleased(-1, 0);
                        }
                        break;
                    case(InputKey::Quit):
                        Quit();
                        break;
                    default:
                        Printf("Unknown key pressed, keycode {}", Ev.code);
                        break;
                }
            }
        }
    }
    
    void SpawnLevel1()
    {
        SpawnWall();
    }

    // TODO: make this more generic
    void SpawnWall()
    {
        // Bottom 2
        Entities.push_back(make_unique<Wall>(0,  232, 128, 8, ORANGE));
        Entities.push_back(make_unique<Wall>(GameWidth - 128, 232, 128, 8, ORANGE));
        
        // First middle
        Entities.push_back(make_unique<Wall>(GameWidth - 288, 168, 256, 8, ORANGE));
        
        // Upper 2
        Entities.push_back(make_unique<Wall>(0,  104, 128, 8, ORANGE));
        Entities.push_back(make_unique<Wall>(GameWidth - 128, 104, 128, 8, ORANGE));
        
        // Top middle
        Entities.push_back(make_unique<Wall>(GameWidth - 288, 40, 256, 8,  ORANGE));
    }    

    void SpawnPlayer()
    {
        unique_ptr<Player> NewPlayer = make_unique<Player>();
        GamePlayer = NewPlayer.get();
        Entities.push_back(std::move(NewPlayer));
    } 

 private:
    Input GameInput;
    Player* GamePlayer;

    int FoundKeyboard = -1;
    bool bIsActive = true;
    double FixedFPS = 30.0;
    double FPS = 60.0;
    
    // DEBUG
    uint64_t UpdateFrames = 0;
    uint64_t FixedFrames = 0;

    chrono::duration<float, milli> FixedFrameTime()
    {
        return chrono::duration<float, milli>(1000.0 / FixedFPS);
    }


    chrono::duration<float, milli> FrameTime()
    {
        return chrono::duration<float, milli>(1000.0 / FPS);
    }

    void ShootDown()
    {
        GamePlayer->Shoot();
        Print("Shoot Down");
    }

    void ShootReleased()
    {
        GamePlayer->CancelShoot();
        Print("Shoot Released");
    }

    void Jump()
    {
        GamePlayer->Jump();
        Print("Jump Input");
    }
    
    int InputX = 0;
    int InputY = 0;

    void ArrowKeyDown(int X, int Y)
    {
        InputX += X;
        InputY += Y;
        
        GamePlayer->Move(InputX, InputY);
        Printf("Movement X: {} Y: {}", InputX, InputY);
    }

    void ArrowKeyReleased(int X, int Y)
    {
        InputX -= X;
        InputY -= Y;

        GamePlayer->Move(InputX, InputY);
        Printf("Movement X: {} Y: {}", InputX, InputY);
    }

    void Quit()
    {
        Print("Quit Down");
        EndGame();
    }

};

 /*
 *  Main
*/

int main()
{
    Print("Hello world!");
    Game* CurGame = new Game();
    CurGame->GameLoop();
    
    delete CurGame;

    return 0;
}

