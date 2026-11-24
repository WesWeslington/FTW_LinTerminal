#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>

// OS Specific
#include <linux/input.h>
#include <fcntl.h>
#include <glob.h>

using namespace std;

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
            perror("Was not able to initialize keyboard.");
        }
        else
        {
            printf("Successfully initialized keyboard!\n");
            ioctl(DeviceID, EVIOCGRAB, 1);
        }
    }

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
 *  Helpers
*/

#define Printf(Message, ...) \
    printf(Message "\n", ##__VA_ARGS__);

void Print(string Message)
{
    cout << Message << endl;
}
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

    int PositionX;
    int PositionY;
    int Speed = 1;

    int InputX;
    int InputY;

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
    
    void Initialize()
    {
        StartTime = chrono::steady_clock::now();
        FoundKeyboard = GameInput.GetDeviceID();
    }

    void GameLoop()
    {
        Initialize();
        //Render(); // DEBUG - TODO: remove this 
        SpawnEntity();
        while(bIsActive)
        {                   
            Update();
            CheckInput();
            //Render();

            if(chrono::steady_clock::now() - LastFixedFrame >= (1000ms / FixedFPS))
            {       
                FixedUpdate();
            }
            
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

        CheckTrackedEntities();
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

        CheckTrackedEntities();
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
    
    void Render()
    {
        for(int Y = 0; Y < 50; Y++)
        {
            for(int X = 0; X < 50; X++)
            {
                DrawPixel(X, Y, 0, 128, 128);
            }

            cout << '\n';
        }

        cout << "\033[0m";
    }

    void SpawnEntity()
    {
        unique_ptr<Player> NewPlayer = make_unique<Player>();
        GamePlayer = NewPlayer.get();
        Entities.push_back(move(NewPlayer));
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

    void DrawPixel(int X, int Y, int R, int G, int B)
    {
        cout << "\033[" << Y + 1 << ";" << (X*3) + 1 << "H";
        cout << "\033[48;2;" << R << ";" << G << ";" << B << "m";
        cout << "   ";
        cout << "\033[0m";
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

