#include <avr/interrupt.h>
#include <SPI.h>
#include "Ucglib.h"

#define BUZZER_PIN 9

#define RGB_LED_R_PIN A8
#define RGB_LED_G_PIN A9
#define RGB_LED_B_PIN A10

#define PUSH_BUTTON_0_PIN 19
#define TIMEOUT_FOR_DEBOUNCE_PUSH_BUTTON_0 500

#define PUSH_BUTTON_1_PIN 20
#define TIMEOUT_FOR_DEBOUNCE_PUSH_BUTTON_1 500

#define JOYSTICK_OX_PIN A15
#define JOYSTICK_OY_PIN A14
#define JOYSTICK_PUSH_PIN 21

#define JOYSTICK_LEFT_THRESHOLD 900
#define JOYSTICK_RIGHT_THRESHOLD 50
#define JOYSTICK_DOWN_THRESHOLD 900
#define JOYSTICK_UP_THRESHOLD 50
#define TIMEOUT_FOR_DEBOUNCE_PUSH_JOYSTICK 50

#define UNCONNECTED_PIN A0

#define INITIAL_STATE 0
#define CHOOSE_GAME_STATE 1
#define PLAY_GAME_TETRIS 2
#define GAME_OVER_TETRIS 3

#define GAME_UNDEFINED -1
#define GAME_TETRIS 0
#define GAME_TIC_TAC_TOE 1
#define GAME_2048 2

Ucglib_ILI9341_18x240x320_HWSPI lcd(/*cd=*/ 48,/*cs=*/ 49,/*reset=*/ 50);

// used for joystick push button's debouncing
volatile long millisAtPreviousPushJoystick = 0;
// used for push button_0's debouncing
volatile long millisAtPreviousPushButton0 = 0;
// used for push button_1's debouncing
volatile long millisAtPreviousPushButton1 = 0;
// used for saving the state of the entire game console
volatile char gameState = INITIAL_STATE;
// used for selection in menu of the game console
volatile char gameSelected = GAME_TETRIS;
// used for sending the game selection from ISR to the showMenu function
volatile char gameToPlay = GAME_UNDEFINED;

#define MAXIMUM_GAME_DELAY_TETRIS 250

#define NUMBER_OF_PIECES_TETRIS 7
#define NO_PIECE_TETRIS -1
#define I_PIECE_TETRIS 0
#define L_PIECE_TETRIS 1
#define J_PIECE_TETRIS 2
#define O_PIECE_TETRIS 3
#define S_PIECE_TETRIS 4
#define T_PIECE_TETRIS 5
#define Z_PIECE_TETRIS 6
#define WALL_PIECE_TETRIS 7

#define TETRIS_PIECE_FIXED 0
#define TETRIS_PIECE_MOVE_DOWN 1
#define TETRIS_PIECE_MOVE_LEFT 2
#define TETRIS_PIECE_MOVE_RIGHT 4
#define TETRIS_PIECE_ROTATE 8

#define TETRIS_PLAY_GAME 0
#define TETRIS_PAUSE_GAME 1
#define TETRIS_EXIT_GAME 2

#define BOARD_SIZE_2048 6

#define DRAW_FROM_UP_2048_BOARD 0
#define DRAW_FROM_DOWN_2048_BOARD 1
#define DRAW_FROM_LEFT_2048_BOARD 2
#define DRAW_FROM_RIGHT_2048_BOARD 3

typedef struct
{
  int x;
  int y;
  int rotation;
  char pieceType;
  bool enabled;
} TetrisPiece;

typedef struct
{
  int gameDelay;
  unsigned long score;
  char tetrisBoardPieces[20][16];
  TetrisPiece movingPiece;
  bool rotateMovingPieceDetected;
  char gamePaused;
} TetrisGame;

typedef struct
{
  unsigned long score;
  int ticTacToeName;
} TicTacToeGame;

typedef struct
{
  unsigned long score;
  int board[BOARD_SIZE_2048][BOARD_SIZE_2048];
} The2048Game;

typedef union
{
  TetrisGame tetris;
  TicTacToeGame ticTacToe;
  The2048Game the2048;
} Game;

volatile Game game;

// Game console initialization

ISR(INT0_vect)
{
  long millisNow = millis();

  if (millisNow - millisAtPreviousPushJoystick > TIMEOUT_FOR_DEBOUNCE_PUSH_JOYSTICK)
  {
    millisAtPreviousPushJoystick = millisNow;
    joystickPushButtonAction();
  } 
}

ISR(INT1_vect)
{
  long millisNow = millis();

  if (millisNow - millisAtPreviousPushButton0 > TIMEOUT_FOR_DEBOUNCE_PUSH_BUTTON_0)
  {
    millisAtPreviousPushButton0 = millisNow;
    pushButton0Action();
  } 
}

ISR(INT2_vect)
{
  long millisNow = millis();

  if (millisNow - millisAtPreviousPushButton1 > TIMEOUT_FOR_DEBOUNCE_PUSH_BUTTON_1)
  {
    millisAtPreviousPushButton1 = millisNow;
    pushButton1Action();
  } 
}

void setup()
{
  gameState = INITIAL_STATE;

  pinMode(JOYSTICK_OX_PIN, INPUT);
  pinMode(JOYSTICK_OY_PIN, INPUT);

  pinMode(RGB_LED_R_PIN, OUTPUT);
  pinMode(RGB_LED_G_PIN, OUTPUT);
  pinMode(RGB_LED_B_PIN, OUTPUT);
  
  cli();

  Serial.begin(9600);

  EICRA |= (1 << ISC11);
  
  EIMSK |= (1 << INT0);
  EIMSK |= (1 << INT1);
  EIMSK |= (1 << INT2);

  sei();

  lcd.begin(UCG_FONT_MODE_TRANSPARENT);
  lcd.setFont(ucg_font_ncenR14_tr);
}

void loop()
{
  analogWrite(RGB_LED_R_PIN, 230);
  analogWrite(RGB_LED_G_PIN, 230);
  analogWrite(RGB_LED_B_PIN, 0);
  
  gameState = INITIAL_STATE;
  gameSelected = GAME_TETRIS;
  gameToPlay = GAME_UNDEFINED;
  
  //play2048();

  showMenu();

  playGame();
}

// General functionalities of the game console

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

void joystickPushButtonAction()
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
      game.tetris.rotateMovingPieceDetected = true;

      break;
    }
    case GAME_OVER_TETRIS:
    {
      gameState = INITIAL_STATE;
    }
  }
}

void pushButton0Action()
{
  switch (gameState)
  {
    case PLAY_GAME_TETRIS:
    {
      if (game.tetris.gamePaused == TETRIS_PLAY_GAME)
      {
        game.tetris.gamePaused = TETRIS_PAUSE_GAME;
      }
      else if (game.tetris.gamePaused == TETRIS_PAUSE_GAME)
      {
        game.tetris.gamePaused = TETRIS_PLAY_GAME;
      }
      else if (game.tetris.gamePaused == TETRIS_EXIT_GAME)
      {
        // reset the entire console
        asm volatile ("jmp 0");
      }

      break;
    }
  }
}

void pushButton1Action()
{
  switch (gameState)
  {
    case PLAY_GAME_TETRIS:
    {
      if (game.tetris.gamePaused == TETRIS_PLAY_GAME)
      {
        game.tetris.gamePaused = TETRIS_EXIT_GAME;
      }
      else if (game.tetris.gamePaused == TETRIS_EXIT_GAME)
      {
        game.tetris.gamePaused = TETRIS_PLAY_GAME;
      }

      break;
    }
  }
}

void showMenu()
{
  gameSelected = GAME_TETRIS;
  
  lcd.clearScreen();

  // print menu message
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Use joystick to select game:")) / 2, 50);
  lcd.setColor(255, 200, 200);
  lcd.print("Use joystick to select game:");

  // draw frame for selected option
  lcd.setColor(0, 255, 0);
  lcd.drawFrame(27, 97 + gameSelected * 75, 186, 46);
  
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

    if (joystickOY > JOYSTICK_DOWN_THRESHOLD)
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
    else if (joystickOY < JOYSTICK_UP_THRESHOLD)
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

// Games implementations

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

// Tetris game implementation

void playTetris()
{
  char lastPieceMove;

  initTetrisGame();

  while(true)
  {
    checkGamePause();
    lastPieceMove = passTimeTetrisGame();
    removeFullLinesFromTetrisBoard();
    generateNewTetrisPiece();    
    drawMovingTetrisPiece(game.tetris.movingPiece.x, game.tetris.movingPiece.y, game.tetris.movingPiece.rotation, game.tetris.movingPiece.pieceType, lastPieceMove);
    printTetrisScore();
    
    delay(game.tetris.gameDelay);
  }
}

void initTetrisGame()
{
  int i, j;

  lcd.clearScreen();

  game.tetris.gamePaused = TETRIS_PLAY_GAME;
  game.tetris.movingPiece.enabled = false;
  game.tetris.rotateMovingPieceDetected = false;
  game.tetris.score = 0;
  game.tetris.gameDelay = MAXIMUM_GAME_DELAY_TETRIS;

  for (i = 0; i < 20; i++)
  {
    for (j = 0; j < 15; j++)
    {
      game.tetris.tetrisBoardPieces[i][j] = NO_PIECE_TETRIS;
    }

    game.tetris.tetrisBoardPieces[i][j] = WALL_PIECE_TETRIS;
  }
  
  printTetrisScore();

  gameState = PLAY_GAME_TETRIS;

  // generating the first piece

  generateNewTetrisPiece();
  drawMovingTetrisPiece(game.tetris.movingPiece.x, game.tetris.movingPiece.y, game.tetris.movingPiece.rotation, game.tetris.movingPiece.pieceType, TETRIS_PIECE_FIXED);
  printTetrisScore();
  
  delay(game.tetris.gameDelay);
}

void checkGamePause()
{
  if (game.tetris.gamePaused == TETRIS_PLAY_GAME)
  {
    return;
  }
  else if (game.tetris.gamePaused == TETRIS_PAUSE_GAME)
  {
    pauseTetris();
  }
  else if (game.tetris.gamePaused == TETRIS_EXIT_GAME)
  {
    exitTetris();
  }  
}

void exitTetris()
{
  lcd.clearScreen();

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Do you want to exit?")) / 2, 150);
  lcd.setColor(255, 200, 200);
  lcd.print("Do you want to exit?");

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Press Pause to resume")) / 2, 275);
  lcd.setColor(255, 200, 200);
  lcd.print("Press Pause to resume");

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Press Exit to exit game...")) / 2, 300);
  lcd.setColor(255, 200, 200);
  lcd.print("Press Exit to exit game...");

  while(game.tetris.gamePaused == TETRIS_EXIT_GAME);

  resumeTetris();
}

void pauseTetris()
{
  lcd.clearScreen();

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Game paused!")) / 2, 150);
  lcd.setColor(255, 200, 200);
  lcd.print("Game paused!");

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Press joystick to resume...")) / 2, 275);
  lcd.setColor(255, 200, 200);
  lcd.print("Press Pause to resume...");

  while(game.tetris.gamePaused == TETRIS_PAUSE_GAME);

  resumeTetris();
}

void resumeTetris()
{
  printTetrisNonMovingBoard();
  printTetrisScore();
  drawMovingTetrisPiece(game.tetris.movingPiece.x, game.tetris.movingPiece.y, game.tetris.movingPiece.rotation, game.tetris.movingPiece.pieceType, TETRIS_PIECE_FIXED);
}

char passTimeTetrisGame()
{
  int rotate = game.tetris.rotateMovingPieceDetected;
  int joystickOX;
  int joystickOY;
  char moveDirectionJoystick = TETRIS_PIECE_MOVE_DOWN;
  char moveDirection;

  joystickOX = analogRead(JOYSTICK_OX_PIN);
  joystickOY = analogRead(JOYSTICK_OY_PIN);

  if (joystickOX < JOYSTICK_RIGHT_THRESHOLD)
  {
    moveDirectionJoystick = TETRIS_PIECE_MOVE_RIGHT;
  }
  else if (joystickOX > JOYSTICK_LEFT_THRESHOLD)
  {
    moveDirectionJoystick = TETRIS_PIECE_MOVE_LEFT;
  }

  if (joystickOY < JOYSTICK_UP_THRESHOLD)
  {
    game.tetris.gameDelay += 50;

    if (game.tetris.gameDelay > MAXIMUM_GAME_DELAY_TETRIS)
    {
      game.tetris.gameDelay = MAXIMUM_GAME_DELAY_TETRIS;
    }
  }
  else if (joystickOY > JOYSTICK_DOWN_THRESHOLD)
  {
    game.tetris.gameDelay -= 50;

    if (game.tetris.gameDelay < 0)
    {
      game.tetris.gameDelay = 0;
    }
  }

  moveDirection = moveDirectionJoystick;

  int pieceHeight = 16;
  int pieceWidth = 16;

  if (rotate)
  {
    pieceHeight *= getDimensionTetrisPiece(game.tetris.movingPiece.pieceType, game.tetris.movingPiece.rotation, true);
    pieceWidth *= getDimensionTetrisPiece(game.tetris.movingPiece.pieceType, game.tetris.movingPiece.rotation, false);
  }
  else
  {
    pieceHeight *= getDimensionTetrisPiece(game.tetris.movingPiece.pieceType, game.tetris.movingPiece.rotation, false);
    pieceWidth *= getDimensionTetrisPiece(game.tetris.movingPiece.pieceType, game.tetris.movingPiece.rotation, false);
  }
  
  if (game.tetris.movingPiece.y + pieceHeight == 320)
  {
    if (!rotate)
    {
      game.tetris.rotateMovingPieceDetected = false;
      moveDirection = TETRIS_PIECE_FIXED;

      if (moveDirectionJoystick == TETRIS_PIECE_MOVE_LEFT || moveDirectionJoystick == TETRIS_PIECE_MOVE_RIGHT)
      {
        moveDirection |= checkAndUpdateTetrisBoardForCollisions(moveDirectionJoystick);
      }
        
      if (moveDirection == TETRIS_PIECE_FIXED)
      {
        game.tetris.movingPiece.enabled = false;
      }
    }
    else
    {
      game.tetris.rotateMovingPieceDetected = false;
      moveDirection = TETRIS_PIECE_FIXED;

      if (moveDirectionJoystick == TETRIS_PIECE_MOVE_LEFT || moveDirectionJoystick == TETRIS_PIECE_MOVE_RIGHT)
      {
        moveDirection |= checkAndUpdateTetrisBoardForCollisions(moveDirectionJoystick);
      }
      
      moveDirection |= checkAndUpdateTetrisBoardForCollisions(TETRIS_PIECE_MOVE_DOWN);
  
      if (moveDirection == TETRIS_PIECE_FIXED)
      {
        game.tetris.movingPiece.enabled = false;
      }
    }
  }
  else
  {
    moveDirection = TETRIS_PIECE_FIXED;
    
    if (rotate)
    {
      moveDirection |= checkAndUpdateTetrisBoardForCollisions(TETRIS_PIECE_ROTATE);
            
      game.tetris.rotateMovingPieceDetected = false;
    }
    
    if (moveDirectionJoystick == TETRIS_PIECE_MOVE_LEFT || moveDirectionJoystick == TETRIS_PIECE_MOVE_RIGHT)
    {
      moveDirection |= checkAndUpdateTetrisBoardForCollisions(moveDirectionJoystick);
    }

    moveDirection |= checkAndUpdateTetrisBoardForCollisions(TETRIS_PIECE_MOVE_DOWN);
    
    if (moveDirection == TETRIS_PIECE_FIXED)
    {
      game.tetris.movingPiece.enabled = false;
    }
  }

  return moveDirection;
}

int getDimensionTetrisPiece(char pieceType, int rotation, bool oXDimension)
{
  int oXDim, oYDim, aux;
  
  if (pieceType == I_PIECE_TETRIS)
  {
    oXDim = 4;
    oYDim = 1;
  }
  else if (pieceType == O_PIECE_TETRIS)
  {
    oXDim = 2;
    oYDim = 2;
  }
  else
  {
    oXDim = 3;
    oYDim = 2;
  }

  if (rotation == 90 || rotation == 270)
  {
    aux = oXDim;
    oXDim = oYDim;
    oYDim = aux;
  }

  if (oXDimension)
  {
    return oXDim;
  }
  else
  {
    return oYDim;
  }
}

char checkAndUpdateTetrisBoardForCollisions(char moveDirection)
{
  int x = game.tetris.movingPiece.x / 16;
  int y = game.tetris.movingPiece.y / 16;
  int dx = 0;
  int dy = 0;
  int pieceType = game.tetris.movingPiece.pieceType;
  int rotation = game.tetris.movingPiece.rotation;
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
      return TETRIS_PIECE_FIXED;
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
      return TETRIS_PIECE_FIXED;
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
      game.tetris.movingPiece.rotation = rotation;  
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
    return
    
    TETRIS_PIECE_FIXED;
  }
  else
  {
    game.tetris.movingPiece.x += 16 * dx;
    game.tetris.movingPiece.y += 16 * dy;
  }

  return moveDirection;
}

bool checkTetrisPieceOnTable(int x, int y, int pieceType, int rotation)
{
  switch(pieceType)
  {
    case I_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        return game.tetris.tetrisBoardPieces[y][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y][x + 1] != NO_PIECE_TETRIS
                  || game.tetris.tetrisBoardPieces[y][x + 2] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y][x + 3] != NO_PIECE_TETRIS;
      }
      else
      {
        return game.tetris.tetrisBoardPieces[y][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x] != NO_PIECE_TETRIS
                  || game.tetris.tetrisBoardPieces[y + 2][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 3][x] != NO_PIECE_TETRIS;
      }
      
      break;
    }
    case L_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        return game.tetris.tetrisBoardPieces[y][x + 2] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y + 1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x + 2] != NO_PIECE_TETRIS;
      }
      else if (rotation == 90)
      {
        return game.tetris.tetrisBoardPieces[y][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y + 2][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 2][x + 1] != NO_PIECE_TETRIS;
      }
      else if (rotation == 180)
      {
        return game.tetris.tetrisBoardPieces[y][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y][x + 1] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y][x + 2] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x] != NO_PIECE_TETRIS;
      }
      else
      {
        return game.tetris.tetrisBoardPieces[y][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y][x + 1] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y + 1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 2][x + 1] != NO_PIECE_TETRIS;
      }
      break;
    }
    case J_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        return game.tetris.tetrisBoardPieces[y][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y + 1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x + 2] != NO_PIECE_TETRIS;
      }
      else if (rotation == 90)
      {
        return game.tetris.tetrisBoardPieces[y][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y][x + 1] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y + 1][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 2][x] != NO_PIECE_TETRIS;
      }
      else if (rotation == 180)
      {
        return game.tetris.tetrisBoardPieces[y][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y][x + 1] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y][x + 2] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x + 2] != NO_PIECE_TETRIS;
      }
      else
      {
        return game.tetris.tetrisBoardPieces[y][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x + 1] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y + 2][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 2][x + 1] != NO_PIECE_TETRIS;
      }
      
      break;
    }
    case O_PIECE_TETRIS:
    {
      return game.tetris.tetrisBoardPieces[y][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y][x + 1] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[y + 1][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x + 1] != NO_PIECE_TETRIS;
      
      break;
    }
    case S_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        return game.tetris.tetrisBoardPieces[y][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y][x + 2] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y + 1][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x + 1] != NO_PIECE_TETRIS;
      }
      else
      {
        return game.tetris.tetrisBoardPieces[y][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y + 1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 2][x + 1] != NO_PIECE_TETRIS;
      }
      
      break;
    }
    case T_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        return game.tetris.tetrisBoardPieces[y][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y + 1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x + 2] != NO_PIECE_TETRIS;
      }
      else if (rotation == 90)
      {
        return game.tetris.tetrisBoardPieces[y][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y + 1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 2][x] != NO_PIECE_TETRIS;
      }
      else if (rotation == 180)
      {
        return game.tetris.tetrisBoardPieces[y][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y][x + 1] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y][x + 2] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x + 1] != NO_PIECE_TETRIS;
      }
      else
      {
        return game.tetris.tetrisBoardPieces[y][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y + 1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 2][x + 1] != NO_PIECE_TETRIS;
      }
      
      break;
    }
    case Z_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        return game.tetris.tetrisBoardPieces[y][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y][x + 1] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y + 1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x + 2] != NO_PIECE_TETRIS;       
      }
      else
      {
        return game.tetris.tetrisBoardPieces[y][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 1][x] != NO_PIECE_TETRIS
                || game.tetris.tetrisBoardPieces[y + 1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[y + 2][x] != NO_PIECE_TETRIS;
      }
      
      break;
    }
  }

  resetConsole();
}

void modifyTetrisPieceFromBoard(int x, int y, int pieceType, int rotation, int newValue)
{
  int tetrisBoardPiece = NO_PIECE_TETRIS;

  if (newValue)
  {
    tetrisBoardPiece = pieceType;
  }
  
  switch(pieceType)
  {
    case I_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        game.tetris.tetrisBoardPieces[y][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y][x + 2] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y][x + 3] = tetrisBoardPiece;
      }
      else
      {
        game.tetris.tetrisBoardPieces[y][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 2][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 3][x] = tetrisBoardPiece;
      }
      
      break;
    }
    case L_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        game.tetris.tetrisBoardPieces[y][x + 2] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 2] = tetrisBoardPiece;
      }
      else if (rotation == 90)
      {
        game.tetris.tetrisBoardPieces[y][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 2][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 2][x + 1] = tetrisBoardPiece;
      }
      else if (rotation == 180)
      {
        game.tetris.tetrisBoardPieces[y][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y][x + 2] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x] = tetrisBoardPiece;
      }
      else
      {
        game.tetris.tetrisBoardPieces[y][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 2][x + 1] = tetrisBoardPiece;
      }
      break;
    }
    case J_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        game.tetris.tetrisBoardPieces[y][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 2] = tetrisBoardPiece;
      }
      else if (rotation == 90)
      {
        game.tetris.tetrisBoardPieces[y][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 2][x] = tetrisBoardPiece;
      }
      else if (rotation == 180)
      {
        game.tetris.tetrisBoardPieces[y][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y][x + 2] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 2] = tetrisBoardPiece;
      }
      else
      {
        game.tetris.tetrisBoardPieces[y][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 2][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 2][x + 1] = tetrisBoardPiece;
      }
      
      break;
    }
    case O_PIECE_TETRIS:
    {
      game.tetris.tetrisBoardPieces[y][x] = tetrisBoardPiece;
      game.tetris.tetrisBoardPieces[y][x + 1] = tetrisBoardPiece;
      game.tetris.tetrisBoardPieces[y + 1][x] = tetrisBoardPiece;
      game.tetris.tetrisBoardPieces[y + 1][x + 1] = tetrisBoardPiece;
      
      break;
    }
    case S_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        game.tetris.tetrisBoardPieces[y][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y][x + 2] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 1] = tetrisBoardPiece;
      }
      else
      {
        game.tetris.tetrisBoardPieces[y][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 2][x + 1] = tetrisBoardPiece;
      }
      
      break;
    }
    case T_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        game.tetris.tetrisBoardPieces[y][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 2] = tetrisBoardPiece;
      }
      else if (rotation == 90)
      {
        game.tetris.tetrisBoardPieces[y][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 2][x] = tetrisBoardPiece;
      }
      else if (rotation == 180)
      {
        game.tetris.tetrisBoardPieces[y][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y][x + 2] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 1] = tetrisBoardPiece;
      }
      else
      {
        game.tetris.tetrisBoardPieces[y][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 2][x + 1] = tetrisBoardPiece;
      }
      
      break;
    }
    case Z_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        game.tetris.tetrisBoardPieces[y][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 2] = tetrisBoardPiece;
      }
      else
      {
        game.tetris.tetrisBoardPieces[y][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 1][x + 1] = tetrisBoardPiece;
        game.tetris.tetrisBoardPieces[y + 2][x] = tetrisBoardPiece;
      }
      
      break;
    }
    default:
    {
      resetConsole();
    }
  }
}

void removeFullLinesFromTetrisBoard()
{
  if (game.tetris.movingPiece.enabled)
  {
    return;
  }
  
  int i, j;
  bool fullLine;

  for (i = 19; i >= 0; i--)
  {
    fullLine = true;

    for (j = 0 ; j < 15; j++)
    {
      if (game.tetris.tetrisBoardPieces[i][j] == NO_PIECE_TETRIS)
      {
        fullLine = false;
        break;
      }
    }

    if (fullLine)
    {
      removeLineFromTetrisBoard(i);
      i++;
    }
  }
}

void removeLineFromTetrisBoard(int lineNo)
{
  int i, j;

  for (i = lineNo - 1; i >= 0; i--)
  {
    for (j = 0; j < 15; j++)
    {
      game.tetris.tetrisBoardPieces[i + 1][j] = game.tetris.tetrisBoardPieces[i][j];
    }
  }

  for (j = 0; j < 15; j++)
  {
    game.tetris.tetrisBoardPieces[0][j] = NO_PIECE_TETRIS;
  }

  printTetrisNonMovingBoard();

  game.tetris.score += 10;

  printTetrisScore();
}

void printTetrisNonMovingBoard()
{
  int i, j;
  
  lcd.clearScreen();

  for (i = 19; i >= 0; i--)
  {
    for (j = 0; j < 15; j++)
    {
      if(game.tetris.tetrisBoardPieces[i][j] != NO_PIECE_TETRIS)
      {
        switch(game.tetris.tetrisBoardPieces[i][j])
        {
          case I_PIECE_TETRIS:
          {
            lcd.setColor(0, 255, 255);
            break;
          }
          case L_PIECE_TETRIS:
          {
            lcd.setColor(255, 127, 0);
            break;
          }
          case J_PIECE_TETRIS:
          {
            lcd.setColor(0, 0, 255);
            break;
          }
          case O_PIECE_TETRIS:
          {
            lcd.setColor(255, 255, 0);
            break;
          }
          case S_PIECE_TETRIS:
          {
            lcd.setColor(0, 255, 0);
            break;
          }
          case T_PIECE_TETRIS:
          {
            lcd.setColor(128, 0, 128);
            break;
          }
          case Z_PIECE_TETRIS:
          {
            lcd.setColor(255, 0, 0);
            break;
          }
          default:
          {
            lcd.setColor(0, 0, 0);
          }
        }

        lcd.drawBox(16 * j, 16 * i, 16, 16);
      }
    }
  }
}

void printTetrisScore()
{
  static unsigned long lastScore = 0;

  lcd.setPrintPos(0, 16);
  lcd.setColor(160, 160, 200);
  lcd.print("Score: ");
  
  if (lastScore != game.tetris.score)
  {
    lcd.setPrintPos(60, 16);
    lcd.setColor(0, 0, 0);
    lcd.print(String(lastScore));
  }

  lcd.setPrintPos(60, 16);
  lcd.setColor(160, 160, 200);
  lcd.print(String(game.tetris.score));

  lastScore = game.tetris.score;
}

void generateNewTetrisPiece()
{
  if (game.tetris.movingPiece.enabled)
  {
    return;
  }
  
  char pieceType = random(analogRead(UNCONNECTED_PIN)) % NUMBER_OF_PIECES_TETRIS;
  int rotation = 0;
  int x;

  game.tetris.gameDelay = MAXIMUM_GAME_DELAY_TETRIS;

  if (pieceType == I_PIECE_TETRIS)
  {
    x = 5;
  }
  else
  {
    x = 6;
  }
  
  game.tetris.movingPiece.pieceType = pieceType;
  game.tetris.movingPiece.rotation = rotation;
  game.tetris.movingPiece.y = 0;
  game.tetris.movingPiece.x = 16 * x;
  game.tetris.movingPiece.enabled = true;
  
  game.tetris.score++;  

  switch(pieceType)
  {
    case I_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        if (game.tetris.tetrisBoardPieces[0][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[0][x + 1] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[0][x + 2] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[0][x + 3] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x] = I_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[0][x + 1] = I_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[0][x + 2] = I_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[0][x + 3] = I_PIECE_TETRIS;
      }
      else
      {
        if (game.tetris.tetrisBoardPieces[0][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[2][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[3][x] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x] = I_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x] = I_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[2][x] = I_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[3][x] = I_PIECE_TETRIS;
      }

      break;
    }
    case L_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        if (game.tetris.tetrisBoardPieces[0][x + 2] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x + 2] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x + 2] = L_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x] = L_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 1] = L_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 2] = L_PIECE_TETRIS;
      }
      else if (rotation == 90)
      {
        if (game.tetris.tetrisBoardPieces[0][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[2][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[2][x + 1] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x] = L_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x] = L_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[2][x] = L_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[3][x + 1] = L_PIECE_TETRIS;
      }
      else if (rotation == 180)
      {
        if (game.tetris.tetrisBoardPieces[0][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[0][x + 1] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[0][x + 2] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x] = L_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[0][x + 1] = L_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[0][x + 2] = L_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x] = L_PIECE_TETRIS;
      }
      else
      {
        if (game.tetris.tetrisBoardPieces[0][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[0][x + 1] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[2][x + 1] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x] = L_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[0][x + 1] = L_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 1] = L_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[2][x + 1] = L_PIECE_TETRIS;
      }

      break;
    }
    case J_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        if (game.tetris.tetrisBoardPieces[0][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x + 2] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x] = J_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x] = J_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 1] = J_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 2] = J_PIECE_TETRIS;
      }
      else if (rotation == 90)
      {
        if (game.tetris.tetrisBoardPieces[0][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[0][x + 1] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[1][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[2][x] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x] = J_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[0][x + 1] = J_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x] = J_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[2][x] = J_PIECE_TETRIS;
      }
      else if (rotation == 180)
      {
        if (game.tetris.tetrisBoardPieces[0][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[0][x + 1] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[0][x + 2] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x + 2] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x] = J_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[0][x + 1] = J_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[0][x + 2] = J_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 2] = J_PIECE_TETRIS;
      }
      else
      {
        if (game.tetris.tetrisBoardPieces[0][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x + 1] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[2][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[2][x + 1] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x + 1] = J_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 1] = J_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[2][x] = J_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[2][x + 1] = J_PIECE_TETRIS;
      }

      break;
    }
    case O_PIECE_TETRIS:
    {
      if (game.tetris.tetrisBoardPieces[0][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[0][x + 1] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[1][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x + 1] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x] = O_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[0][x + 1] = O_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x] = O_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 1] = O_PIECE_TETRIS;

        break;
    }
    case S_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        if (game.tetris.tetrisBoardPieces[0][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[0][x + 2] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[1][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x + 1] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x + 1] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[0][x + 2] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 1] = S_PIECE_TETRIS;
      }
      else
      {
        if (game.tetris.tetrisBoardPieces[0][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[2][x + 1] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 1] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[2][x + 1] = S_PIECE_TETRIS;
      }

      break;
    }
    case T_PIECE_TETRIS:
    {
      if (rotation == 0)
      {
        if (game.tetris.tetrisBoardPieces[0][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x + 2] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x + 1] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 1] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 2] = S_PIECE_TETRIS;
      }
      else if (rotation == 90)
      {
        if (game.tetris.tetrisBoardPieces[0][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[2][x] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 1] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[2][x] = S_PIECE_TETRIS;
      }
      else if (rotation == 180)
      {
        if (game.tetris.tetrisBoardPieces[0][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[0][x + 1] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[0][x + 2] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x + 1] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[0][x + 1] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[0][x + 2] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 1] = S_PIECE_TETRIS;
      }
      else
      {
        if (game.tetris.tetrisBoardPieces[0][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[2][x + 1] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x + 1] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 1] = S_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[2][x + 1] = S_PIECE_TETRIS;
      }

      break;
    }
    case Z_PIECE_TETRIS:
    {
      if (rotation == 0 || rotation == 180)
      {
        if (game.tetris.tetrisBoardPieces[0][x] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[0][x + 1] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x + 2] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x] = Z_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[0][x + 1] = Z_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 1] = Z_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 2] = Z_PIECE_TETRIS;
      }
      else
      {
        if (game.tetris.tetrisBoardPieces[0][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[1][x] != NO_PIECE_TETRIS
              || game.tetris.tetrisBoardPieces[1][x + 1] != NO_PIECE_TETRIS || game.tetris.tetrisBoardPieces[2][x] != NO_PIECE_TETRIS)
        {
          tetrisGameOver();
        }
        
        game.tetris.tetrisBoardPieces[0][x + 1] = Z_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x] = Z_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[1][x + 1] = Z_PIECE_TETRIS;
        game.tetris.tetrisBoardPieces[2][x] = Z_PIECE_TETRIS;
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
      tone(9, 500, 50);
      delay(50);
      
      drawTetrisPiece(offsetOX, offsetOY, (rotation + 270) % 360, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);
      
      break;
    }
    case TETRIS_PIECE_MOVE_DOWN | TETRIS_PIECE_ROTATE:
    {
      tone(9, 500, 50);
      delay(50);
      
      drawTetrisPiece(offsetOX, offsetOY - 16, (rotation + 270) % 360, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);

      break;
    }
    case TETRIS_PIECE_MOVE_LEFT | TETRIS_PIECE_ROTATE:
    {
      tone(9, 500, 50);
      delay(50);
      
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
      tone(9, 500, 50);
      delay(50);
      
      drawTetrisPiece(offsetOX + 16, offsetOY - 16, (rotation + 270) % 360, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);

      break;
    }
    case TETRIS_PIECE_MOVE_DOWN | TETRIS_PIECE_MOVE_RIGHT | TETRIS_PIECE_ROTATE:
    {
      tone(9, 500, 50);
      delay(50);
      
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

void tetrisGameOver()
{
  char score[15];
  sprintf(score, "%lu", game.tetris.score);
  
  lcd.clearScreen();

  gameState = GAME_OVER_TETRIS;
  
  lcd.clearScreen();

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Game Over!")) / 2, 70);
  lcd.setColor(255, 200, 200);
  lcd.print("Game Over!");

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Your score:")) / 2, 150);
  lcd.print("Your score:");

  lcd.setColor(0, 255, 0);
  lcd.drawRFrame((lcd.getWidth() - lcd.getStrWidth(score) - 25) / 2, 170, lcd.getStrWidth(score) + 25, 40, 8);
  
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth(score)) / 2, 195);
  lcd.setColor(255, 0, 0);
  lcd.print(score);

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Press joystick to continue...")) / 2, 275);
  lcd.setColor(255, 200, 200);
  lcd.print("Press joystick to continue...");

  while(gameState == GAME_OVER_TETRIS);

  // reset the entire console
  asm volatile ("jmp 0");
}

// Tic-Tac_Toe game implementation

void playTicTacToe()
{
  lcd.clearScreen();

  // print menu message
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Tic-Tac_Toe")) / 2, 25);
  lcd.setColor(255, 200, 200);
  lcd.print("Tic-Tac_Toe");
}

// 2048 game implementation

void play2048()
{
  lcd.clearScreen();

  init2048Game();

  while(true)
  {
    ;
  }
}

void init2048Game()
{
  game.the2048.score = 0;

  print2048Score();

  draw2048Board(DRAW_FROM_UP_2048_BOARD);
  delay(500);

  lcd.clearScreen();

  draw2048Board(DRAW_FROM_DOWN_2048_BOARD);
  delay(500);

  lcd.clearScreen();

  draw2048Board(DRAW_FROM_LEFT_2048_BOARD);
  delay(500);

  lcd.clearScreen();

  draw2048Board(DRAW_FROM_RIGHT_2048_BOARD);
  delay(500);
}

void print2048Score()
{
  static unsigned long lastScore = 0;

  lcd.setFont(ucg_font_ncenR14_tr);
  lcd.setPrintPos(0, 16);
  lcd.setColor(160, 160, 200);
  lcd.print("Score: ");
  
  if (lastScore != game.the2048.score)
  {
    lcd.setPrintPos(60, 16);
    lcd.setColor(0, 0, 0);
    lcd.print(String(lastScore));
  }

  lcd.setPrintPos(60, 16);
  lcd.setColor(160, 160, 200);
  lcd.print(String(game.the2048.score));

  lastScore = game.the2048.score;
}

void draw2048Board(char drawDirection)
{
  int i, j;
  int tileSize = lcd.getWidth() / BOARD_SIZE_2048;
  int oYOffset = (lcd.getHeight() - (BOARD_SIZE_2048 * tileSize));
  int oXOffset = (lcd.getWidth() - (BOARD_SIZE_2048 * tileSize)) / 2;

  lcd.setColor(0, 255, 255);
  lcd.setFont(ucg_font_amstrad_cpc_8f);
  
  switch(drawDirection)
  {
    case DRAW_FROM_UP_2048_BOARD:
    {
      for (i = 0; i < BOARD_SIZE_2048; i++)
      {
        for (j = 0; j < BOARD_SIZE_2048; j++)
        {
          game.the2048.board[i][j] = 0;
          
          lcd.drawRFrame(oXOffset + j * tileSize, oYOffset + i * tileSize, tileSize, tileSize, 4);
        }
      }
      
      break;
    }
    case DRAW_FROM_DOWN_2048_BOARD:
    {
      for (i = BOARD_SIZE_2048 - 1; i >= 0; i--)
      {
        for (j = 0; j < BOARD_SIZE_2048; j++)
        {
          game.the2048.board[i][j] = 0;
          
          lcd.drawRFrame(oXOffset + j * tileSize, oYOffset + i * tileSize, tileSize, tileSize, 4);
        }
      }
      
      break;
    }
    case DRAW_FROM_LEFT_2048_BOARD:
    {
      for (i = 0; i < BOARD_SIZE_2048; i++)
      {
        for (j = 0; j < BOARD_SIZE_2048; j++)
        {
          game.the2048.board[i][j] = 0;
          
          lcd.drawRFrame(oXOffset + i * tileSize, oYOffset + j * tileSize, tileSize, tileSize, 4);
        }
      }
      
      break;
    }
    case DRAW_FROM_RIGHT_2048_BOARD:
    {
      for (i = BOARD_SIZE_2048 - 1; i >= 0; i--)
      {
        for (j = 0; j < BOARD_SIZE_2048; j++)
        {
          game.the2048.board[i][j] = 0;
          
          lcd.drawRFrame(oXOffset + i * tileSize, oYOffset + j * tileSize, tileSize, tileSize, 4);
        }
      }
      
      break;
    }
  }
}
