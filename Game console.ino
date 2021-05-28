#include <avr/interrupt.h>
#include <SPI.h>
#include "Ucglib.h"

#define JOYSTICK_OX_PIN A15
#define JOYSTICK_OY_PIN A14
#define JOYSTICK_PUSH_PIN 21

#define JOYSTICK_LEFT_THRESHOLD 900
#define JOYSTICK_RIGHT_THRESHOLD 50 

#define UNCONNECTED_PIN A0

#define TIMEOUT_FOR_DEBOUNCE 50

#define INITIAL_STATE 0
#define CHOOSE_GAME_STATE 1
#define PLAY_GAME_TETRIS 2

#define GAME_UNDEFINED -1
#define GAME_TETRIS 0
#define GAME_TIC_TAC_TOE 1
#define GAME_2048 2

typedef struct
{
  int x;
  int y;
  char pieceType;
  bool fixed;
  int rotation;
} TetrisPiece;

typedef struct
{
  bool tetrisBoard[20][15];
  TetrisPiece tetrisPieces[300];
  int numberOfPieces;
  bool movingPiece;
  bool rotatePiece;
} TetrisGame;

Ucglib_ILI9341_18x240x320_HWSPI lcd(/*cd=*/ 48,/*cs=*/ 49,/*reset=*/ 50);

// used for joystick push button's debouncing
volatile long millisAtPreviousPush = 0;
// used for saving the state of the entire game console
volatile char gameState = INITIAL_STATE;
// used for selection in menu of the game console
volatile char gameSelected = GAME_TETRIS;
// used for sending the game selection from ISR to the showMenu function
volatile char gameToPlay = GAME_UNDEFINED;

#define NUMBER_OF_PIECES_TETRIS 7
#define I_PIECE_TETRIS 0
#define L_PIECE_TETRIS 1
#define J_PIECE_TETRIS 2
#define O_PIECE_TETRIS 3
#define S_PIECE_TETRIS 4
#define T_PIECE_TETRIS 5
#define Z_PIECE_TETRIS 6

#define TETRIS_PIECE_FIXED 0
#define TETRIS_PIECE_MOVE_DOWN 1
#define TETRIS_PIECE_MOVE_LEFT 2
#define TETRIS_PIECE_MOVE_RIGHT 4
#define TETRIS_PIECE_ROTATE 8

volatile TetrisGame tetris;

ISR(INT0_vect)
{
  long millisNow = millis();

  if (millisNow - millisAtPreviousPush > TIMEOUT_FOR_DEBOUNCE)
  {
    millisAtPreviousPush = millisNow;
    pushButtonAction();
  } 
}

void resetConsole()
{
  lcd.clearScreen();
  lcd.setColor(255, 0, 0);
  
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("An error occured!")) / 2, lcd.getHeight() / 2 - 25);
  lcd.print("An error occured!");

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Resetting in a few seconds...")) / 2, lcd.getHeight() / 2 + 25);
  lcd.print("Resetting in a few seconds...");

  delay(2000);

  // reset the entire console
  asm volatile ("jmp 0");
}

void pushButtonAction()
{
  switch (gameState)
  {
    case CHOOSE_GAME_STATE:
    {
      gameToPlay = gameSelected;
      
      break;
    }
    case PLAY_GAME_TETRIS:
    {
      tetris.rotatePiece = true;

      break;
    }
  }
}

void setup()
{
  gameState = INITIAL_STATE;

  pinMode(JOYSTICK_OX_PIN, INPUT);
  pinMode(JOYSTICK_OY_PIN, INPUT);
  
  cli();

  Serial.begin(9600);

  EICRA |= (1 << ISC11);
  EIMSK |= (1 << INT0);

  sei();

  lcd.begin(UCG_FONT_MODE_TRANSPARENT);
  lcd.setFont(ucg_font_ncenR14_tr);
}

void showMenu()
{
  gameSelected = GAME_TETRIS;
  
  lcd.clearScreen();

  // print menu message
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Choose a game:")) / 2, 25);
  lcd.setColor(255, 200, 200);
  lcd.print("Choose a game:");

  // draw frame for selected option
  lcd.setColor(0, 255, 0);
  lcd.drawFrame(27, 97  + gameSelected * 75, 186, 46);
  
  // draw Tetris option box
  lcd.setColor(255, 240, 200);
  lcd.drawBox(30, 100, 180, 40);

  // print Tetris option text
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Tetris")) / 2, 125);
  lcd.setColor(255, 255, 255);
  lcd.print("Tetris");

  // draw Tic-Tac-Toe option box
  lcd.setColor(255, 240, 200);
  lcd.drawBox(30, 175, 180, 40);

  // print Tetris option text
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Tic-Tac-Toe")) / 2, 200);
  lcd.setColor(255, 255, 255);
  lcd.print("Tic-Tac-Toe");

  // draw 2048 option box
  lcd.setColor(255, 240, 200);
  lcd.drawBox(30, 250, 180, 40);

  // print Tetris option text
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("2048")) / 2, 275);
  lcd.setColor(255, 255, 255);
  lcd.print("2048");

  gameState = CHOOSE_GAME_STATE;
  
  int joystickOY;
  
  while(true)
  {
    joystickOY = analogRead(JOYSTICK_OY_PIN);

    if (joystickOY > JOYSTICK_LEFT_THRESHOLD)
    {
      if (gameSelected < 2)
      {
        // clean frame for selected option
        lcd.setColor(0, 0, 0);
        lcd.drawFrame(27, 97  + gameSelected * 75, 186, 46);

        // update selection
        gameSelected++;

        // draw frame for selected option
        lcd.setColor(0, 255, 0);
        lcd.drawFrame(27, 97  + gameSelected * 75, 186, 46);
      }
    }
    else if (joystickOY < JOYSTICK_RIGHT_THRESHOLD)
    {
      if (gameSelected > 0)
      {
        // clean frame for selected option
        lcd.setColor(0, 0, 0);
        lcd.drawFrame(27, 97  + gameSelected * 75, 186, 46);

        // update selection
        gameSelected--;

        // draw frame for selected option
        lcd.setColor(0, 255, 0);
        lcd.drawFrame(27, 97  + gameSelected * 75, 186, 46);
      }
    } 

    if (gameToPlay != GAME_UNDEFINED)
    {
      break;
    }
    
    delay(200);
  }
}

void drawTetrisPiece(int offsetOX, int offsetOY, int rotation, char pieceType, bool colored)
{
  switch(pieceType)
  {
    case I_PIECE_TETRIS:
    {
      if (!colored)
      {
        lcd.setColor(0, 0, 0);
      }
      else
      {
        lcd.setColor(0, 255, 255);
      }
      
      if (rotation == 0 || rotation == 180)
      {
        lcd.drawBox(offsetOX, offsetOY, 64, 16);
      }
      else
      {
        lcd.drawBox(offsetOX, offsetOY, 16, 64);
      }
      
      break;
    }
    case L_PIECE_TETRIS:
    {
      if (!colored)
      {
        lcd.setColor(0, 0, 0);
      }
      else
      {
        lcd.setColor(255, 127, 0);
      }
      
      if (rotation == 0)
      {
        lcd.drawBox(offsetOX + 32, offsetOY, 16, 16);
        lcd.drawBox(offsetOX, offsetOY + 16, 48, 16);
      }
      else if (rotation == 90)
      {
        lcd.drawBox(offsetOX, offsetOY, 16, 48);
        lcd.drawBox(offsetOX + 16, offsetOY + 32, 16, 16);
      }
      else if (rotation == 180)
      {
        lcd.drawBox(offsetOX, offsetOY, 48, 16);
        lcd.drawBox(offsetOX, offsetOY + 16, 16, 16);
      }
      else
      {
        lcd.drawBox(offsetOX, offsetOY, 16, 16);
        lcd.drawBox(offsetOX + 16, offsetOY, 16, 48);
      }
      
      break;
    }
    case J_PIECE_TETRIS:
    {
      if (!colored)
      {
        lcd.setColor(0, 0, 0);
      }
      else
      {
        lcd.setColor(0, 0, 255);
      }
      
      if (rotation == 0)
      {
        lcd.drawBox(offsetOX, offsetOY, 16, 16);
        lcd.drawBox(offsetOX, offsetOY + 16, 48, 16);
      }
      else if (rotation == 90)
      {
        lcd.drawBox(offsetOX, offsetOY, 16, 48);
        lcd.drawBox(offsetOX + 16, offsetOY, 16, 16);
      }
      else if (rotation == 180)
      {
        lcd.drawBox(offsetOX, offsetOY, 48, 16);
        lcd.drawBox(offsetOX + 32, offsetOY + 16, 16, 16);
      }
      else
      {
        lcd.drawBox(offsetOX, offsetOY + 32, 16, 16);
        lcd.drawBox(offsetOX + 16, offsetOY, 16, 48);
      }
      
      break;
    }
    case O_PIECE_TETRIS:
    {
      if (!colored)
      {
        lcd.setColor(0, 0, 0);
      }
      else
      {
        lcd.setColor(255, 255, 0);
      }
      
      lcd.drawBox(offsetOX, offsetOY, 32, 32);
      
      break;
    }
    case S_PIECE_TETRIS:
    {
      if (!colored)
      {
        lcd.setColor(0, 0, 0);
      }
      else
      {
        lcd.setColor(0, 255, 0);
      }
      
      if (rotation == 0 || rotation == 180)
      {
        lcd.drawBox(offsetOX + 16, offsetOY, 32, 16);
        lcd.drawBox(offsetOX, offsetOY + 16, 32, 16);
      }
      else
      {
        lcd.drawBox(offsetOX, offsetOY, 16, 32);
        lcd.drawBox(offsetOX + 16, offsetOY + 16, 16, 32);
      }
      
      break;
    }
    case T_PIECE_TETRIS:
    {
      if (!colored)
      {
        lcd.setColor(0, 0, 0);
      }
      else
      {
        lcd.setColor(128, 0, 128);
      }
      
      if (rotation == 0)
      {
        lcd.drawBox(offsetOX + 16, offsetOY, 16, 16);
        lcd.drawBox(offsetOX, offsetOY + 16, 48, 16);
      }
      else if (rotation == 90)
      {
        lcd.drawBox(offsetOX, offsetOY, 16, 48);
        lcd.drawBox(offsetOX + 16, offsetOY + 16, 16, 16);
      }
      else if (rotation == 180)
      {
        lcd.drawBox(offsetOX, offsetOY, 48, 16);
        lcd.drawBox(offsetOX + 16, offsetOY + 16, 16, 16);
      }
      else
      {
        lcd.drawBox(offsetOX, offsetOY + 16, 16, 16);
        lcd.drawBox(offsetOX + 16, offsetOY, 16, 48);
      }
      
      break;
    }
    case Z_PIECE_TETRIS:
    {
      if (!colored)
      {
        lcd.setColor(0, 0, 0);
      }
      else
      {
        lcd.setColor(255, 0, 0);
      }
      
      if (rotation == 0 || rotation == 180)
      {
        lcd.drawBox(offsetOX, offsetOY, 32, 16);
        lcd.drawBox(offsetOX + 16, offsetOY + 16, 32, 16);
      }
      else
      {
        lcd.drawBox(offsetOX, offsetOY + 16, 16, 32);
        lcd.drawBox(offsetOX + 16, offsetOY, 16, 32);
      }
      
      break;
    }
    default:
    {
      resetConsole();
    }
  }
}

void drawMovingTetrisPiece(int offsetOX, int offsetOY, int rotation, char pieceType, char movingDirection)
{
  switch(movingDirection)
  {
    case TETRIS_PIECE_MOVE_DOWN:
    {
      drawTetrisPiece(offsetOX, offsetOY - 16, rotation, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);

      break;
    }
    case TETRIS_PIECE_MOVE_LEFT:
    {
      drawTetrisPiece(offsetOX + 16, offsetOY, rotation, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);

      break;
    }
    case TETRIS_PIECE_MOVE_RIGHT:
    {
      drawTetrisPiece(offsetOX - 16, offsetOY, rotation, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);

      break;
    }
    case TETRIS_PIECE_MOVE_DOWN | TETRIS_PIECE_MOVE_LEFT:
    {
      drawTetrisPiece(offsetOX + 16, offsetOY - 16, rotation, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);

      break;
    }
    case TETRIS_PIECE_MOVE_DOWN | TETRIS_PIECE_MOVE_RIGHT:
    {
      drawTetrisPiece(offsetOX - 16, offsetOY - 16, rotation, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);

      break;
    }
    case TETRIS_PIECE_ROTATE:
    {
      drawTetrisPiece(offsetOX, offsetOY, (rotation + 270) % 360, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);
      
      break;
    }
    case TETRIS_PIECE_MOVE_DOWN | TETRIS_PIECE_ROTATE:
    {
      drawTetrisPiece(offsetOX, offsetOY - 16, (rotation + 270) % 360, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);

      break;
    }
    case TETRIS_PIECE_MOVE_LEFT | TETRIS_PIECE_ROTATE:
    {
      drawTetrisPiece(offsetOX + 16, offsetOY, (rotation + 270) % 360, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);

      break;
    }
    case TETRIS_PIECE_MOVE_RIGHT | TETRIS_PIECE_ROTATE:
    {
      drawTetrisPiece(offsetOX - 16, offsetOY, (rotation + 270) % 360, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);

      break;
    }
    case TETRIS_PIECE_MOVE_DOWN | TETRIS_PIECE_MOVE_LEFT | TETRIS_PIECE_ROTATE:
    {
      drawTetrisPiece(offsetOX + 16, offsetOY - 16, (rotation + 270) % 360, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);

      break;
    }
    case TETRIS_PIECE_MOVE_DOWN | TETRIS_PIECE_MOVE_RIGHT | TETRIS_PIECE_ROTATE:
    {
      drawTetrisPiece(offsetOX - 16, offsetOY - 16, (rotation + 270) % 360, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);

      break;
    }
    default:
    {
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);
    }
  }
}

void drawTetrisBoard(char lastPieceMove)
{
  int movingPiece = tetris.numberOfPieces - 1;

  drawMovingTetrisPiece(tetris.tetrisPieces[movingPiece].x, tetris.tetrisPieces[movingPiece].y,
                        tetris.tetrisPieces[movingPiece].rotation, tetris.tetrisPieces[movingPiece].pieceType,
                        lastPieceMove);
}

int getDimensionTetrisPiece(char pieceType, int rotation, bool oXDimension)
{
  switch(pieceType)
  {
    case I_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        if (oXDimension)
        {
          return 4;
        }
        else
        {
          return 1;
        }
      }
      else
      {
        if (oXDimension)
        {
          return 1;
        }
        else
        {
          return 4;
        }
      }
    }
    case O_PIECE_TETRIS:
    {
      return 2;
    }
    default:
    {
      if (rotation == 0 || rotation == 180)
      {
        if (oXDimension)
        {
          return 3;
        }
        else
        {
          return 2;
        }
      }
      else
      {
        if (oXDimension)
        {
          return 2;
        }
        else
        {
          return 3;
        }
      }
    }
  }
}

void clearTetrisBoard(void)
{
  int i, j;

  for (i = 0; i < 20; i++)
  {
    for (j = 0; j < 15; j++)
    {
      tetris.tetrisBoard[i][j] = false;
    }
  }
}

void printTetrisBoard(void)
{
  int i, j;

  for (i = 0; i < 20; i++)
  {
    for (j = 0; j < 15; j++)
    {
      if(tetris.tetrisBoard[i][j])
      {
        Serial.print("1 ");
      }
      else
      {
        Serial.print("0 ");
      }
    }
    Serial.print("\n");
  }

  Serial.print("\n\n\n");
}

void tetrisGameOver(bool won)
{
  if (won)
  {
    // TODO: print winning message
  }
  else
  {
    // TODO: print loosing message
  }

  // TODO: restart game
  resetConsole();
}

void generateNewTetrisPiece(void)
{
  if (tetris.movingPiece)
  {
    return;
  }
  
  char pieceType = random(analogRead(UNCONNECTED_PIN)) % NUMBER_OF_PIECES_TETRIS;
  int rotation = 0;
  int x;

  if (pieceType == I_PIECE_TETRIS)
  {
    x = 5;
  }
  else
  {
    x = 6;
  }
  
  tetris.tetrisPieces[tetris.numberOfPieces].pieceType = pieceType;
  tetris.tetrisPieces[tetris.numberOfPieces].fixed = false;
  tetris.tetrisPieces[tetris.numberOfPieces].rotation = rotation;
  tetris.tetrisPieces[tetris.numberOfPieces].y = 0;
  tetris.tetrisPieces[tetris.numberOfPieces].x = 16 * x;

  tetris.numberOfPieces++;
  tetris.movingPiece = true;

  switch(pieceType)
  {
    case I_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        if (tetris.tetrisBoard[0][x] || tetris.tetrisBoard[0][x + 1]
              || tetris.tetrisBoard[0][x + 2] || tetris.tetrisBoard[0][x + 3])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x] = true;
        tetris.tetrisBoard[0][x + 1] = true;
        tetris.tetrisBoard[0][x + 2] = true;
        tetris.tetrisBoard[0][x + 3] = true;
      }
      else
      {
        if (tetris.tetrisBoard[0][x] || tetris.tetrisBoard[1][x]
              || tetris.tetrisBoard[2][x] || tetris.tetrisBoard[3][x])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x] = true;
        tetris.tetrisBoard[1][x] = true;
        tetris.tetrisBoard[2][x] = true;
        tetris.tetrisBoard[3][x] = true;
      }

      break;
    }
    case L_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        if (tetris.tetrisBoard[0][x + 2] || tetris.tetrisBoard[1][x]
              || tetris.tetrisBoard[1][x + 1] || tetris.tetrisBoard[1][x + 2])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x + 2] = true;
        tetris.tetrisBoard[1][x] = true;
        tetris.tetrisBoard[1][x + 1] = true;
        tetris.tetrisBoard[1][x + 2] = true;
      }
      else if (rotation == 90)
      {
        if (tetris.tetrisBoard[0][x] || tetris.tetrisBoard[1][x]
              || tetris.tetrisBoard[2][x] || tetris.tetrisBoard[2][x + 1])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x] = true;
        tetris.tetrisBoard[1][x] = true;
        tetris.tetrisBoard[2][x] = true;
        tetris.tetrisBoard[3][x + 1] = true;
      }
      else if (rotation == 180)
      {
        if (tetris.tetrisBoard[0][x] || tetris.tetrisBoard[0][x + 1]
              || tetris.tetrisBoard[0][x + 2] || tetris.tetrisBoard[1][x])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x] = true;
        tetris.tetrisBoard[0][x + 1] = true;
        tetris.tetrisBoard[0][x + 2] = true;
        tetris.tetrisBoard[1][x] = true;
      }
      else
      {
        if (tetris.tetrisBoard[0][x] || tetris.tetrisBoard[0][x + 1]
              || tetris.tetrisBoard[1][x + 1] || tetris.tetrisBoard[2][x + 1])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x] = true;
        tetris.tetrisBoard[0][x + 1] = true;
        tetris.tetrisBoard[1][x + 1] = true;
        tetris.tetrisBoard[2][x + 1] = true;
      }

      break;
    }
    case J_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        if (tetris.tetrisBoard[0][x] || tetris.tetrisBoard[1][x]
              || tetris.tetrisBoard[1][x + 1] || tetris.tetrisBoard[1][x + 2])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x] = true;
        tetris.tetrisBoard[1][x] = true;
        tetris.tetrisBoard[1][x + 1] = true;
        tetris.tetrisBoard[1][x + 2] = true;
      }
      else if (rotation == 90)
      {
        if (tetris.tetrisBoard[0][x] || tetris.tetrisBoard[0][x + 1]
              || tetris.tetrisBoard[1][x] || tetris.tetrisBoard[2][x])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x] = true;
        tetris.tetrisBoard[0][x + 1] = true;
        tetris.tetrisBoard[1][x] = true;
        tetris.tetrisBoard[2][x] = true;
      }
      else if (rotation == 180)
      {
        if (tetris.tetrisBoard[0][x] || tetris.tetrisBoard[0][x + 1]
              || tetris.tetrisBoard[0][x + 2] || tetris.tetrisBoard[1][x + 2])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x] = true;
        tetris.tetrisBoard[0][x + 1] = true;
        tetris.tetrisBoard[0][x + 2] = true;
        tetris.tetrisBoard[1][x + 2] = true;
      }
      else
      {
        if (tetris.tetrisBoard[0][x + 1] || tetris.tetrisBoard[1][x + 1]
              || tetris.tetrisBoard[2][x] || tetris.tetrisBoard[2][x + 1])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x + 1] = true;
        tetris.tetrisBoard[1][x + 1] = true;
        tetris.tetrisBoard[2][x] = true;
        tetris.tetrisBoard[2][x + 1] = true;
      }

      break;
    }
    case O_PIECE_TETRIS:
    {
      if (tetris.tetrisBoard[0][x] || tetris.tetrisBoard[0][x + 1]
              || tetris.tetrisBoard[1][x] || tetris.tetrisBoard[1][x + 1])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x] = true;
        tetris.tetrisBoard[0][x + 1] = true;
        tetris.tetrisBoard[1][x] = true;
        tetris.tetrisBoard[1][x + 1] = true;

        break;
    }
    case S_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        if (tetris.tetrisBoard[0][x + 1] || tetris.tetrisBoard[0][x + 2]
              || tetris.tetrisBoard[1][x] || tetris.tetrisBoard[1][x + 1])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x + 1] = true;
        tetris.tetrisBoard[0][x + 2] = true;
        tetris.tetrisBoard[1][x] = true;
        tetris.tetrisBoard[1][x + 1] = true;
      }
      else
      {
        if (tetris.tetrisBoard[0][x] || tetris.tetrisBoard[1][x]
              || tetris.tetrisBoard[1][x + 1] || tetris.tetrisBoard[2][x + 1])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x] = true;
        tetris.tetrisBoard[1][x] = true;
        tetris.tetrisBoard[1][x + 1] = true;
        tetris.tetrisBoard[2][x + 1] = true;
      }

      break;
    }
    case T_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        if (tetris.tetrisBoard[0][x + 1] || tetris.tetrisBoard[1][x]
              || tetris.tetrisBoard[1][x + 1] || tetris.tetrisBoard[1][x + 2])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x + 1] = true;
        tetris.tetrisBoard[1][x] = true;
        tetris.tetrisBoard[1][x + 1] = true;
        tetris.tetrisBoard[1][x + 2] = true;
      }
      else if (rotation == 90)
      {
        if (tetris.tetrisBoard[0][x] || tetris.tetrisBoard[1][x]
              || tetris.tetrisBoard[1][x + 1] || tetris.tetrisBoard[2][x])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x] = true;
        tetris.tetrisBoard[1][x] = true;
        tetris.tetrisBoard[1][x + 1] = true;
        tetris.tetrisBoard[2][x] = true;
      }
      else if (rotation == 180)
      {
        if (tetris.tetrisBoard[0][x] || tetris.tetrisBoard[0][x + 1]
              || tetris.tetrisBoard[0][x + 2] || tetris.tetrisBoard[1][x + 1])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x] = true;
        tetris.tetrisBoard[0][x + 1] = true;
        tetris.tetrisBoard[0][x + 2] = true;
        tetris.tetrisBoard[1][x + 1] = true;
      }
      else
      {
        if (tetris.tetrisBoard[0][x + 1] || tetris.tetrisBoard[1][x]
              || tetris.tetrisBoard[1][x + 1] || tetris.tetrisBoard[2][x + 1])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x + 1] = true;
        tetris.tetrisBoard[1][x] = true;
        tetris.tetrisBoard[1][x + 1] = true;
        tetris.tetrisBoard[2][x + 1] = true;
      }

      break;
    }
    case Z_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        if (tetris.tetrisBoard[0][x] || tetris.tetrisBoard[0][x + 1]
              || tetris.tetrisBoard[1][x + 1] || tetris.tetrisBoard[1][x + 2])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x] = true;
        tetris.tetrisBoard[0][x + 1] = true;
        tetris.tetrisBoard[1][x + 1] = true;
        tetris.tetrisBoard[1][x + 2] = true;
      }
      else
      {
        if (tetris.tetrisBoard[0][x + 1] || tetris.tetrisBoard[1][x]
              || tetris.tetrisBoard[1][x + 1] || tetris.tetrisBoard[2][x])
        {
          tetrisGameOver(false);
        }
        
        tetris.tetrisBoard[0][x + 1] = true;
        tetris.tetrisBoard[1][x] = true;
        tetris.tetrisBoard[1][x + 1] = true;
        tetris.tetrisBoard[2][x] = true;
      }

      break;
    }
    default:
    {
      resetConsole();
    }
  }
}

void modifyTetrisPieceFromBoard(int x, int y, int pieceType, int rotation, int newValue)
{
  switch(pieceType)
  {
    case I_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        tetris.tetrisBoard[y][x] = newValue;
        tetris.tetrisBoard[y][x + 1] = newValue;
        tetris.tetrisBoard[y][x + 2] = newValue;
        tetris.tetrisBoard[y][x + 3] = newValue;
      }
      else
      {
        tetris.tetrisBoard[y][x] = newValue;
        tetris.tetrisBoard[y + 1][x] = newValue;
        tetris.tetrisBoard[y + 2][x] = newValue;
        tetris.tetrisBoard[y + 3][x] = newValue;
      }
      
      break;
    }
    case L_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        tetris.tetrisBoard[y][x + 2] = newValue;
        tetris.tetrisBoard[y + 1][x] = newValue;
        tetris.tetrisBoard[y + 1][x + 1] = newValue;
        tetris.tetrisBoard[y + 1][x + 2] = newValue;
      }
      else if (rotation == 90)
      {
        tetris.tetrisBoard[y][x] = newValue;
        tetris.tetrisBoard[y + 1][x] = newValue;
        tetris.tetrisBoard[y + 2][x] = newValue;
        tetris.tetrisBoard[y + 2][x + 1] = newValue;
      }
      else if (rotation == 180)
      {
        tetris.tetrisBoard[y][x] = newValue;
        tetris.tetrisBoard[y][x + 1] = newValue;
        tetris.tetrisBoard[y][x + 2] = newValue;
        tetris.tetrisBoard[y + 1][x] = newValue;
      }
      else
      {
        tetris.tetrisBoard[y][x] = newValue;
        tetris.tetrisBoard[y][x + 1] = newValue;
        tetris.tetrisBoard[y + 1][x + 1] = newValue;
        tetris.tetrisBoard[y + 2][x + 1] = newValue;
      }
      break;
    }
    case J_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        tetris.tetrisBoard[y][x] = newValue;
        tetris.tetrisBoard[y + 1][x] = newValue;
        tetris.tetrisBoard[y + 1][x + 1] = newValue;
        tetris.tetrisBoard[y + 1][x + 2] = newValue;
      }
      else if (rotation == 90)
      {
        tetris.tetrisBoard[y][x] = newValue;
        tetris.tetrisBoard[y][x + 1] = newValue;
        tetris.tetrisBoard[y + 1][x] = newValue;
        tetris.tetrisBoard[y + 2][x] = newValue;
      }
      else if (rotation == 180)
      {
        tetris.tetrisBoard[y][x] = newValue;
        tetris.tetrisBoard[y][x + 1] = newValue;
        tetris.tetrisBoard[y][x + 2] = newValue;
        tetris.tetrisBoard[y + 1][x + 2] = newValue;
      }
      else
      {
        tetris.tetrisBoard[y][x + 1] = newValue;
        tetris.tetrisBoard[y + 1][x + 1] = newValue;
        tetris.tetrisBoard[y + 2][x] = newValue;
        tetris.tetrisBoard[y + 2][x + 1] = newValue;
      }
      
      break;
    }
    case O_PIECE_TETRIS:
    {
      tetris.tetrisBoard[y][x] = newValue;
      tetris.tetrisBoard[y][x + 1] = newValue;
      tetris.tetrisBoard[y + 1][x] = newValue;
      tetris.tetrisBoard[y + 1][x + 1] = newValue;
      
      break;
    }
    case S_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        tetris.tetrisBoard[y][x + 1] = newValue;
        tetris.tetrisBoard[y][x + 2] = newValue;
        tetris.tetrisBoard[y + 1][x] = newValue;
        tetris.tetrisBoard[y + 1][x + 1] = newValue;
      }
      else
      {
        tetris.tetrisBoard[y][x] = newValue;
        tetris.tetrisBoard[y + 1][x] = newValue;
        tetris.tetrisBoard[y + 1][x + 1] = newValue;
        tetris.tetrisBoard[y + 2][x + 1] = newValue;
      }
      
      break;
    }
    case T_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        tetris.tetrisBoard[y][x + 1] = newValue;
        tetris.tetrisBoard[y + 1][x] = newValue;
        tetris.tetrisBoard[y + 1][x + 1] = newValue;
        tetris.tetrisBoard[y + 1][x + 2] = newValue;
      }
      else if (rotation == 90)
      {
        tetris.tetrisBoard[y][x] = newValue;
        tetris.tetrisBoard[y + 1][x] = newValue;
        tetris.tetrisBoard[y + 1][x + 1] = newValue;
        tetris.tetrisBoard[y + 2][x] = newValue;
      }
      else if (rotation == 180)
      {
        tetris.tetrisBoard[y][x] = newValue;
        tetris.tetrisBoard[y][x + 1] = newValue;
        tetris.tetrisBoard[y][x + 2] = newValue;
        tetris.tetrisBoard[y + 1][x + 1] = newValue;
      }
      else
      {
        tetris.tetrisBoard[y][x + 1] = newValue;
        tetris.tetrisBoard[y + 1][x] = newValue;
        tetris.tetrisBoard[y + 1][x + 1] = newValue;
        tetris.tetrisBoard[y + 2][x + 1] = newValue;
      }
      
      break;
    }
    case Z_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        tetris.tetrisBoard[y][x] = newValue;
        tetris.tetrisBoard[y][x + 1] = newValue;
        tetris.tetrisBoard[y + 1][x + 1] = newValue;
        tetris.tetrisBoard[y + 1][x + 2] = newValue;       
      }
      else
      {
        tetris.tetrisBoard[y][x + 1] = newValue;
        tetris.tetrisBoard[y + 1][x] = newValue;
        tetris.tetrisBoard[y + 1][x + 1] = newValue;
        tetris.tetrisBoard[y + 2][x] = newValue;
      }
      
      break;
    }
    default:
    {
      resetConsole();
    }
  }
}

bool checkTetrisPieceOnTable(int x, int y, int pieceType, int rotation)
{
  switch(pieceType)
  {
    case I_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        return tetris.tetrisBoard[y][x] || tetris.tetrisBoard[y][x + 1]
                  || tetris.tetrisBoard[y][x + 2] || tetris.tetrisBoard[y][x + 3];
      }
      else
      {
        return tetris.tetrisBoard[y][x] || tetris.tetrisBoard[y + 1][x]
                  || tetris.tetrisBoard[y + 2][x] || tetris.tetrisBoard[y + 3][x];
      }
      
      break;
    }
    case L_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        return tetris.tetrisBoard[y][x + 2] || tetris.tetrisBoard[y + 1][x]
                || tetris.tetrisBoard[y + 1][x + 1] || tetris.tetrisBoard[y + 1][x + 2];
      }
      else if (rotation == 90)
      {
        return tetris.tetrisBoard[y][x] || tetris.tetrisBoard[y + 1][x]
                || tetris.tetrisBoard[y + 2][x] || tetris.tetrisBoard[y + 2][x + 1];
      }
      else if (rotation == 180)
      {
        return tetris.tetrisBoard[y][x] || tetris.tetrisBoard[y][x + 1]
                || tetris.tetrisBoard[y][x + 2] || tetris.tetrisBoard[y + 1][x];
      }
      else
      {
        return tetris.tetrisBoard[y][x] || tetris.tetrisBoard[y][x + 1]
                || tetris.tetrisBoard[y + 1][x + 1] || tetris.tetrisBoard[y + 2][x + 1];
      }
      break;
    }
    case J_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        return tetris.tetrisBoard[y][x] || tetris.tetrisBoard[y + 1][x]
                || tetris.tetrisBoard[y + 1][x + 1] || tetris.tetrisBoard[y + 1][x + 2];
      }
      else if (rotation == 90)
      {
        return tetris.tetrisBoard[y][x] || tetris.tetrisBoard[y][x + 1]
                || tetris.tetrisBoard[y + 1][x] || tetris.tetrisBoard[y + 2][x];
      }
      else if (rotation == 180)
      {
        return tetris.tetrisBoard[y][x] || tetris.tetrisBoard[y][x + 1]
                || tetris.tetrisBoard[y][x + 2] || tetris.tetrisBoard[y + 1][x + 2];
      }
      else
      {
        return tetris.tetrisBoard[y][x + 1] || tetris.tetrisBoard[y + 1][x + 1]
                || tetris.tetrisBoard[y + 2][x] || tetris.tetrisBoard[y + 2][x + 1];
      }
      
      break;
    }
    case O_PIECE_TETRIS:
    {
      return tetris.tetrisBoard[y][x] || tetris.tetrisBoard[y][x + 1]
              || tetris.tetrisBoard[y + 1][x] || tetris.tetrisBoard[y + 1][x + 1];
      
      break;
    }
    case S_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        return tetris.tetrisBoard[y][x + 1] || tetris.tetrisBoard[y][x + 2]
                || tetris.tetrisBoard[y + 1][x] || tetris.tetrisBoard[y + 1][x + 1];
      }
      else
      {
        return tetris.tetrisBoard[y][x] || tetris.tetrisBoard[y + 1][x]
                || tetris.tetrisBoard[y + 1][x + 1] || tetris.tetrisBoard[y + 2][x + 1];
      }
      
      break;
    }
    case T_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        return tetris.tetrisBoard[y][x + 1] || tetris.tetrisBoard[y + 1][x]
                || tetris.tetrisBoard[y + 1][x + 1] || tetris.tetrisBoard[y + 1][x + 2];
      }
      else if (rotation == 90)
      {
        return tetris.tetrisBoard[y][x] || tetris.tetrisBoard[y + 1][x]
                || tetris.tetrisBoard[y + 1][x + 1] || tetris.tetrisBoard[y + 2][x];
      }
      else if (rotation == 180)
      {
        return tetris.tetrisBoard[y][x] || tetris.tetrisBoard[y][x + 1]
                || tetris.tetrisBoard[y][x + 2] || tetris.tetrisBoard[y + 1][x + 1];
      }
      else
      {
        return tetris.tetrisBoard[y][x + 1] || tetris.tetrisBoard[y + 1][x]
                || tetris.tetrisBoard[y + 1][x + 1] || tetris.tetrisBoard[y + 2][x + 1];
      }
      
      break;
    }
    case Z_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        return tetris.tetrisBoard[y][x] || tetris.tetrisBoard[y][x + 1]
                || tetris.tetrisBoard[y + 1][x + 1] || tetris.tetrisBoard[y + 1][x + 2];       
      }
      else
      {
        return tetris.tetrisBoard[y][x + 1] || tetris.tetrisBoard[y + 1][x]
                || tetris.tetrisBoard[y + 1][x + 1] || tetris.tetrisBoard[y + 2][x];
      }
      
      break;
    }
  }

  resetConsole();
}

char checkAndUpdateTetrisBoardForCollisions(int pieceNo, char moveDirection)
{
  int x = tetris.tetrisPieces[pieceNo].x / 16;
  int y = tetris.tetrisPieces[pieceNo].y / 16;
  int dx = 0;
  int dy = 0;
  int pieceType = tetris.tetrisPieces[pieceNo].pieceType;
  int rotation = tetris.tetrisPieces[pieceNo].rotation;
  bool collision = false;

  if (moveDirection == TETRIS_PIECE_MOVE_DOWN)
  {
    dy = 1;
  }
  else if (moveDirection == TETRIS_PIECE_MOVE_LEFT)
  {
    if (x > 0)
    {
      dx = -1;
    }
    else
    {
      moveDirection = TETRIS_PIECE_FIXED;
    }    
  }
  else if (moveDirection == TETRIS_PIECE_MOVE_RIGHT)
  {
    if (x + getDimensionTetrisPiece(pieceType, rotation, true) < 15)
    {
      dx = 1;
    }
    else
    {
      moveDirection = TETRIS_PIECE_FIXED;
    }
  }

  modifyTetrisPieceFromBoard(x, y, pieceType, rotation, false);

  if (moveDirection == TETRIS_PIECE_ROTATE)
  {
    rotation = (rotation + 90) % 360;

    if (checkTetrisPieceOnTable(x, y, pieceType, rotation))
    {
      rotation = (rotation + 270) % 360;

      modifyTetrisPieceFromBoard(x, y, pieceType, rotation, true);

      collision = true;
    }
    else
    {
      modifyTetrisPieceFromBoard(x, y, pieceType, rotation, true);
      tetris.tetrisPieces[pieceNo].rotation = rotation;  
    }         
  }
  else if (moveDirection != TETRIS_PIECE_FIXED)
  {
    if (checkTetrisPieceOnTable(x + dx, y + dy, pieceType, rotation))
    {  
      modifyTetrisPieceFromBoard(x, y, pieceType, rotation, true);
      
      collision = true;
    }
    else
    {
      modifyTetrisPieceFromBoard(x + dx, y + dy, pieceType, rotation, true); 
    }
  }

  if (collision)
  {   
    moveDirection = TETRIS_PIECE_FIXED;
  }
  else
  {
    tetris.tetrisPieces[pieceNo].x += 16 * dx;
    tetris.tetrisPieces[pieceNo].y += 16 * dy;
  }

  return moveDirection;
}

char passTimeTetrisGame(void)
{
  int rotate = tetris.rotatePiece;
  
  if (tetris.numberOfPieces == 0)
  {
    return TETRIS_PIECE_FIXED;
  }
  
  int joystickOX;
  char moveDirectionJoystick = TETRIS_PIECE_MOVE_DOWN, moveDirection;

  joystickOX = analogRead(JOYSTICK_OX_PIN);

  if (joystickOX < JOYSTICK_RIGHT_THRESHOLD)
  {
    moveDirectionJoystick = TETRIS_PIECE_MOVE_RIGHT;
  }
  else if (joystickOX > JOYSTICK_LEFT_THRESHOLD)
  {
    moveDirectionJoystick = TETRIS_PIECE_MOVE_LEFT;
  }

  moveDirection = moveDirectionJoystick;

  int movingPiece = tetris.numberOfPieces - 1;
  int pieceHeight = 16;

  if (rotate)
  {
    pieceHeight *= getDimensionTetrisPiece(tetris.tetrisPieces[movingPiece].pieceType, tetris.tetrisPieces[movingPiece].rotation, true);
  }
  else
  {
    pieceHeight *= getDimensionTetrisPiece(tetris.tetrisPieces[movingPiece].pieceType, tetris.tetrisPieces[movingPiece].rotation, false);
  }
  
  if (tetris.tetrisPieces[movingPiece].y + pieceHeight == 320)
  {
    if (!rotate)
    {
      tetris.tetrisPieces[tetris.numberOfPieces - 1].fixed = true;
      tetris.movingPiece = false;

      moveDirection = TETRIS_PIECE_FIXED;
    }
    else
    {
      tetris.rotatePiece = false;
      moveDirection = TETRIS_PIECE_FIXED;

      if (moveDirectionJoystick == TETRIS_PIECE_MOVE_LEFT || moveDirectionJoystick == TETRIS_PIECE_MOVE_RIGHT)
      {
        moveDirection |= checkAndUpdateTetrisBoardForCollisions(tetris.numberOfPieces - 1, moveDirectionJoystick);
      }
      
      moveDirection |= checkAndUpdateTetrisBoardForCollisions(tetris.numberOfPieces - 1, TETRIS_PIECE_MOVE_DOWN);
  
      if (moveDirection == TETRIS_PIECE_FIXED)
      {
        tetris.tetrisPieces[tetris.numberOfPieces - 1].fixed = true;
        tetris.movingPiece = false;
      }
    }
  }
  else
  {
    moveDirection = TETRIS_PIECE_FIXED;
    
    if (rotate)
    {
      moveDirection |= checkAndUpdateTetrisBoardForCollisions(tetris.numberOfPieces - 1, TETRIS_PIECE_ROTATE);
            
      tetris.rotatePiece = false;
    }
    
    if (moveDirectionJoystick == TETRIS_PIECE_MOVE_LEFT || moveDirectionJoystick == TETRIS_PIECE_MOVE_RIGHT)
    {
      moveDirection |= checkAndUpdateTetrisBoardForCollisions(tetris.numberOfPieces - 1, moveDirectionJoystick);
    }

    moveDirection |= checkAndUpdateTetrisBoardForCollisions(tetris.numberOfPieces - 1, TETRIS_PIECE_MOVE_DOWN);
    
    if (moveDirection == TETRIS_PIECE_FIXED)
    {
      tetris.tetrisPieces[tetris.numberOfPieces - 1].fixed = true;
      tetris.movingPiece = false;
    }
  }

  return moveDirection;
}

void playTetris()
{
  char lastPieceMove;

  gameState = PLAY_GAME_TETRIS;
  lcd.clearScreen();

  tetris.numberOfPieces = 0;
  tetris.movingPiece = false;
  tetris.rotatePiece = false;
  clearTetrisBoard();

  while(true)
  {
    lastPieceMove = passTimeTetrisGame();
    generateNewTetrisPiece();    
    drawTetrisBoard(lastPieceMove);
    
    //printTetrisBoard();

    delay(200);
  }
}

void playTicTacToe()
{
  lcd.clearScreen();

  // print menu message
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Tic-Tac_Toe")) / 2, 25);
  lcd.setColor(255, 200, 200);
  lcd.print("Tic-Tac_Toe");
}

void play2048()
{
  lcd.clearScreen();

  // print menu message
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("2048")) / 2, 25);
  lcd.setColor(255, 200, 200);
  lcd.print("2048");
}

void playGame()
{
  if (gameToPlay == GAME_TETRIS)
  {
    playTetris();
  }
  else if (gameToPlay == GAME_TIC_TAC_TOE)
  {
    playTicTacToe();
  }
  else if (gameToPlay == GAME_2048)
  {
    play2048();
  }
  else
  {
    resetConsole();
  }
}

void loop()
{
  gameState = INITIAL_STATE;
  gameSelected = GAME_TETRIS;
  gameToPlay = GAME_UNDEFINED;

  playTetris();
  
  showMenu();

  playGame();
}
