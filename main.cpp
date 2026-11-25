#include <chrono>
#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
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

#define Printf(Message, ...) \
    printf(Message "\n", ##__VA_ARGS__);


void Print(string Message)
{
    cout << Message << endl;
}

void Error(const string &Message)
{
    perror(Message.c_str());
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
#define WHITE          Color16(0, 0, 0)
#define BLACK          Color16(255, 255, 255)

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

bool InitializeFrameBuffer()
{
    int FrameBufferDevice = InitializeFrameBufferDevice();
    if(FrameBufferDevice < 0)
    {
        Error("Was not able to initialize frame buffer!");
        return false;
    }

    fb_fix_screeninfo FixedInfo;
    fb_var_screeninfo VariableInfo;
    ioctl(FrameBufferDevice, FBIOGET_FSCREENINFO, &FixedInfo);
    ioctl(FrameBufferDevice, FBIOGET_VSCREENINFO, &VariableInfo);

    // TODO: This stuff should be moved out of here
    int Height = VariableInfo.yres;
    int LineLength = FixedInfo.line_length;
    int BitsPerPixel = VariableInfo.bits_per_pixel; // 16 or 32
    size_t PageSize = static_cast<size_t>(Height) * LineLength;

    uint8_t* BackBuffer = new uint8_t[PageSize]();

    //Draw Background
    Clear(CLEAR_COLOR);

    // Draw Rectangle
    DrawRectangle(0,  40, 16, 16, CYAN);
    DrawRectangle(16, 40, 16, 16, MAGENTA);
    DrawRectangle(32, 40, 16, 16, GREEN);
    DrawRectangle(48, 40, 16, 16, BLUE);
    DrawRectangle(64, 40, 16, 16, RED);
    DrawRectangle(80, 40, 16, 16, YELLOW);
    DrawRectangle(96, 40, 16, 16, ORANGE);
    DrawRectangle(112, 40, 16, 16, BLACK);
    DrawRectangle(128, 40, 16, 16, WHITE);
    
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
                    if(BitsPerPixel == 16)
                    {
                        reinterpret_cast<uint16_t*>(Row)[DestX] = Pixel;
                    }
                    else
                    {
                        reinterpret_cast<uint16_t*>(Row)[DestX] = RGB565ToRGBA32(Pixel);
                    }
                }
            }
        }
    }
    

    pwrite(FrameBufferDevice, BackBuffer, PageSize, 0);
    
    //delete[] BackBuffer;

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
    
    bool IsAlive() { return bIsAlive;}

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
    int Speed = 1;

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
            Printf("Player Location: X: %d | Y: %d", PositionX, PositionY);  
        }

        if(InputY != 0)
        {
            PositionY += InputY * Speed;
            Printf("Player Location: X: %d | Y: %d", PositionX, PositionY);  
        }
    }
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
        }
        else
        {
            Error("Failed to initialize Frame Buffer");
        }
    }

    void GameLoop()
    {
        Initialize(); 
        SpawnEntity();
        while(bIsActive)
        {                   
            Update();
            CheckInput();
            Render();

            if(chrono::steady_clock::now() - LastFixedFrame >= (1000ms / FixedFPS))
            {       
                FixedUpdate();
            }
            
            CheckTrackedEntities();

            // Without this, we will run at >1,000,000 FPS
            this_thread::sleep_until( chrono::steady_clock::now() + FrameTime());
        }
    }

    void EndGame()
    {
        bIsActive = false;
        Print("Goodbye world!");
        Printf("Update Frames: %lu\nFixed Frames: %lu\n\n", UpdateFrames, FixedFrames);
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
    
    void Render()
    { 
        for(auto& CurEntity : Entities)
        {
            CurEntity->Render();
        }
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
            if(InputMap.contains(Ev.code))
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
                        Printf("Unknown key pressed, keycode %d", Ev.code);
                        break;
                }
            }
        }
    }
    

    void SpawnEntity()
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
    uint8_t FixedFPS = 30;
    uint16_t FPS = 300;
    
    // DEBUG
    uint64_t UpdateFrames = 0;
    uint64_t FixedFrames = 0;

    chrono::milliseconds FixedFrameTime()
    {
        return chrono::milliseconds(1000 / FixedFPS);
    }


    chrono::milliseconds FrameTime()
    {
        return chrono::milliseconds(1000 / FPS);
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
        Printf("Movement X: %d Y: %d", InputX, InputY);
    }

    void ArrowKeyReleased(int X, int Y)
    {
        InputX -= X;
        InputY -= Y;

        GamePlayer->Move(InputX, InputY);
        Printf("Movement X: %d Y: %d", InputX, InputY);
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

