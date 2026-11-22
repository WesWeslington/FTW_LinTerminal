#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>

using namespace std;

 /*
 *    Input Boilerplate
*/

class Input
{
public:
    Input()
    {
        tcgetattr(STDIN_FILENO, &Orig);
        termios Raw = Orig;
        Raw.c_lflag &= ~(ICANON | ECHO);
        Raw.c_cc[VMIN] = 0;
        Raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &Raw);
    }

    ~Input()
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &Orig);
    }

    int PollKey()
    {
        char InputChar;
        return read(STDIN_FILENO, &InputChar, 1) == 1 ? InputChar : -1;
    }

private:
    termios Orig;
};

enum class InputKey
{
    Up,    // Arrow - 65
    Down,  // Arrow - 66
    Right, // Arrow - 67
    Left,  // Arrow - 68 
    Shoot, // G     - 103
    Jump,  // Space - 32
    Quit   // q     - 113
};

map<int, InputKey> InputMap = 
{
    {65,  InputKey::Up},
    {66,  InputKey::Down},
    {67,  InputKey::Right},
    {68,  InputKey::Left},
    {103, InputKey::Shoot},
    {32,  InputKey::Jump},
    {113, InputKey::Quit}
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
 *  Player
*/

class Player
{

public:

    void Shoot()
    {
    }
    
    void Jump()
    {
    }

};

 /*
 *  Game
*/

class Game
{
 public:

    void GameLoop()
    {
        StartTime = chrono::steady_clock::now();

        while(bIsActive)
        {                   
            Update();
            
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
        
        CheckInput();
    }
    
    void FixedUpdate()
    {
        LastFixedFrame = chrono::steady_clock::now(); 
        FixedFrames++;
        
    }

    void CheckInput()
    {
        int InputVal = GameInput.PollKey();
        if(InputVal > -1)
        {
            if(InputMap.contains(InputVal))
            {   
                switch(InputMap[InputVal])
                {
                    case(InputKey::Jump):
                        Print("Jump Input");
                        break;
                    case(InputKey::Shoot):
                        Print("Shoot Input");
                        break;
                    case(InputKey::Up):
                        Print("Up Input");
                        break;
                    case(InputKey::Down):
                        Print("Down Input");
                        break;
                    case(InputKey::Right):
                        Print("Right Input");
                        break;
                    case(InputKey::Left):
                        Print("Left Input");
                        break;
                    case(InputKey::Quit):
                        Print("Quit Input");
                        EndGame();
                        break;
                    default:
                        Printf("Unknown key pressed, keycode %d", InputVal);
                        break;
                }
            }
        }
    }

 private:
    Input GameInput;

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

