#include <avr/interrupt.h>
#include <SPI.h>
#include "Ucglib.h"

#define BUZZER_PIN 7

#define RGB_LED_RIGHT_R_PIN A5
#define RGB_LED_RIGHT_G_PIN A6
#define RGB_LED_RIGHT_B_PIN A7

#define RGB_LED_LEFT_R_PIN A4
#define RGB_LED_LEFT_G_PIN A3
#define RGB_LED_LEFT_B_PIN A2

#define PUSH_BUTTON_BLUE_PIN 19
#define PUSH_BUTTON_RED_PIN 20
#define TIMEOUT_FOR_DEBOUNCE_PUSH_BUTTON_BLUE 150
#define TIMEOUT_FOR_DEBOUNCE_PUSH_BUTTON_RED 150

#define JOYSTICK_OX_PIN A14
#define JOYSTICK_OY_PIN A15
#define JOYSTICK_PUSH_PIN 21

#define JOYSTICK_LEFT_THRESHOLD 900
#define JOYSTICK_RIGHT_THRESHOLD 50
#define JOYSTICK_DOWN_THRESHOLD 900
#define JOYSTICK_UP_THRESHOLD 50
#define TIMEOUT_FOR_DEBOUNCE_PUSH_JOYSTICK 100

#define UNCONNECTED_PIN A0

#define LED_LEFT 0
#define LED_RIGHT 1

#define INITIAL_STATE 0
#define CHOOSE_GAME_STATE 1
#define PLAY_GAME_TETRIS 2
#define GAME_OVER_TETRIS 3
#define PLAY_GAME_2048 4
#define GAME_OVER_THE_2048 5
#define PLAY_GAME_TIC_TAC_TOE 6
#define GAME_OVER_TIC_TAC_TOE 7

#define GAME_UNDEFINED -1
#define GAME_TETRIS 0
#define GAME_TIC_TAC_TOE 1
#define GAME_2048 2

Ucglib_ILI9341_18x240x320_HWSPI lcd(/*cd=*/ 48,/*cs=*/ 49,/*reset=*/ 50);

// used for joystick push button's debouncing
volatile long millisAtPreviousPushJoystick = 0;
// used for push button_blue's debouncing
volatile long millisAtPreviousPushButtonBlue = 0;
// used for push button_red's debouncing
volatile long millisAtPreviousPushButtonRed = 0;
// used for saving the state of the entire game console
volatile char gameState = INITIAL_STATE;
// used for selection in menu of the game console
volatile char gameSelected = GAME_TETRIS;
// used for sending the game selection from ISR to the showMenu function
volatile char gameToPlay = GAME_UNDEFINED;

// Tetris related constants
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

// 2048 related constants
#define BOARD_SIZE_2048 5

#define GAME_DELAY_2048 100

#define PLAYER_MOVE_UNDEFINED_2048 -1

#define DRAW_FROM_UP_2048_BOARD 0
#define DRAW_FROM_DOWN_2048_BOARD 1
#define DRAW_FROM_LEFT_2048_BOARD 2
#define DRAW_FROM_RIGHT_2048_BOARD 3

#define THE_2048_PLAY_GAME 0
#define THE_2048_PAUSE_GAME 1
#define THE_2048_EXIT_GAME 2

// Tic-Tac-Toe related constants
#define BOARD_SIZE_TIC_TAC_TOE 3

#define GAME_DELAY_TIC_TAC_TOE 100

#define UNDEFINED_PLAYER_TIC_TAC_TOE 0
#define X_PLAYER_TIC_TAC_TOE 1
#define O_PLAYER_TIC_TAC_TOE 2

#define PLAYER_MOVE_UNDEFINED_TIC_TAC_TOE -1

#define PLAYER_MOVE_UP_TIC_TAC_TOE 0
#define PLAYER_MOVE_DOWN_TIC_TAC_TOE 1
#define PLAYER_MOVE_LEFT_TIC_TAC_TOE 2
#define PLAYER_MOVE_RIGHT_TIC_TAC_TOE 3

#define PLAYER_WON_TIC_TAC_TOE 0
#define TIE_TIC_TAC_TOE 1
#define PLAYER_LOST_TIC_TAC_TOE 2

#define TIC_TAC_TOE_PLAY_GAME 0
#define TIC_TAC_TOE_PAUSE_GAME 1
#define TIC_TAC_TOE_EXIT_GAME 2

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
  char selectorOX;
  char selectorOY;
  bool nextMoveSelected;
  char board[BOARD_SIZE_TIC_TAC_TOE][BOARD_SIZE_TIC_TAC_TOE];
  char gamePaused;
} TicTacToeGame;

typedef struct
{
  char gamePaused;
  unsigned int emptyCells;
  unsigned long score;
  unsigned int board[BOARD_SIZE_2048][BOARD_SIZE_2048];
  bool updates[BOARD_SIZE_2048][BOARD_SIZE_2048];
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

  if (millisNow - millisAtPreviousPushButtonBlue > TIMEOUT_FOR_DEBOUNCE_PUSH_BUTTON_BLUE)
  {
    millisAtPreviousPushButtonBlue = millisNow;
    pushButtonBlueAction();
  } 
}

ISR(INT2_vect)
{
  long millisNow = millis();

  if (millisNow - millisAtPreviousPushButtonRed > TIMEOUT_FOR_DEBOUNCE_PUSH_BUTTON_RED)
  {
    millisAtPreviousPushButtonRed = millisNow;
    pushButtonRedAction();
  } 
}

void setup()
{
  gameState = INITIAL_STATE;
  
  pinMode(PUSH_BUTTON_BLUE_PIN, INPUT_PULLUP);
  pinMode(PUSH_BUTTON_RED_PIN, INPUT_PULLUP);
  
  pinMode(JOYSTICK_PUSH_PIN, INPUT);
  pinMode(JOYSTICK_OX_PIN, INPUT);
  pinMode(JOYSTICK_OY_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  
  pinMode(RGB_LED_RIGHT_R_PIN, OUTPUT);
  pinMode(RGB_LED_RIGHT_G_PIN, OUTPUT);
  pinMode(RGB_LED_RIGHT_B_PIN, OUTPUT);
  pinMode(RGB_LED_LEFT_R_PIN, OUTPUT);
  pinMode(RGB_LED_LEFT_G_PIN, OUTPUT);
  pinMode(RGB_LED_LEFT_B_PIN, OUTPUT);
  
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
  setLed(0, 0, 0, LED_LEFT);
  setLed(0, 0, 0, LED_RIGHT);
  
  gameState = INITIAL_STATE;
  gameSelected = GAME_TETRIS;
  gameToPlay = GAME_UNDEFINED;
  
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

void setLed(int r, int g, int b, char led)
{
  if (led == LED_LEFT)
  {
    analogWrite(RGB_LED_LEFT_R_PIN, r);
    analogWrite(RGB_LED_LEFT_G_PIN, g);
    analogWrite(RGB_LED_LEFT_B_PIN, b);
  }
  else if (led == LED_RIGHT)
  {
    analogWrite(RGB_LED_RIGHT_R_PIN, r);
    analogWrite(RGB_LED_RIGHT_G_PIN, g);
    analogWrite(RGB_LED_RIGHT_B_PIN, b);
  }
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

      break;
    }
    case GAME_OVER_THE_2048:
    {
      gameState = INITIAL_STATE;

      break;
    }
    case PLAY_GAME_TIC_TAC_TOE:
    {
      game.ticTacToe.nextMoveSelected = true;

      break;
    }
    case GAME_OVER_TIC_TAC_TOE:
    {
      gameState = INITIAL_STATE;

      break;
    }
  }
}

void pushButtonBlueAction()
{
  switch (gameState)
  {
    case PLAY_GAME_TETRIS:
    {
      if (game.tetris.gamePaused == TETRIS_PAUSE_GAME)
      {
        // reset the entire console
        asm volatile ("jmp 0");
      }

      break;
    }
    case PLAY_GAME_2048:
    {
      if (game.the2048.gamePaused == THE_2048_PAUSE_GAME)
      {
        // reset the entire console
        asm volatile ("jmp 0");
      }

      break;
    }
    case PLAY_GAME_TIC_TAC_TOE:
    {
      if (game.ticTacToe.gamePaused == TIC_TAC_TOE_PAUSE_GAME)
      {
        // reset the entire console
        asm volatile ("jmp 0");
      }

      break;
    }
  }
}

void pushButtonRedAction()
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

      break;
    }
    case PLAY_GAME_2048:
    {
      if (game.the2048.gamePaused == THE_2048_PLAY_GAME)
      {
        game.the2048.gamePaused = THE_2048_PAUSE_GAME;
      }
      else if (game.the2048.gamePaused == THE_2048_PAUSE_GAME)
      {
        game.the2048.gamePaused = THE_2048_PLAY_GAME;
      }

      break;
    }
    case PLAY_GAME_TIC_TAC_TOE:
    {
      if (game.ticTacToe.gamePaused == TIC_TAC_TOE_PLAY_GAME)
      {
        game.ticTacToe.gamePaused = TIC_TAC_TOE_PAUSE_GAME;
      }
      else if (game.ticTacToe.gamePaused == TIC_TAC_TOE_PAUSE_GAME)
      {
        game.ticTacToe.gamePaused = TIC_TAC_TOE_PLAY_GAME;
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
  lcd.setColor(0, 0, 0);
  lcd.print("Tetris");

  // draw Tic-Tac-Toe option box
  lcd.setColor(255, 240, 200);
  lcd.drawBox(30, 175, 180, 40);

  // print Tetris option text
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Tic-Tac-Toe")) / 2, 200);
  lcd.setColor(0, 0, 0);
  lcd.print("Tic-Tac-Toe");

  // draw 2048 option box
  lcd.setColor(255, 240, 200);
  lcd.drawBox(30, 250, 180, 40);

  // print Tetris option text
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("2048")) / 2, 275);
  lcd.setColor(0, 0, 0);
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

// Games' implementations

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
    checkTetrisGamePause();
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

void checkTetrisGamePause()
{
  if (game.tetris.gamePaused == TETRIS_PLAY_GAME)
  {
    return;
  }
  else if (game.tetris.gamePaused == TETRIS_PAUSE_GAME)
  {
    pauseTetris();
  } 
}

void pauseTetris()
{
  char score[15];
  sprintf(score, "%lu", game.tetris.score);
  
  lcd.clearScreen();

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Game paused!")) / 2, 100);
  lcd.setColor(255, 200, 200);
  lcd.print("Game paused!");

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Your score:")) / 2, 150);
  lcd.print("Your score:");

  lcd.setColor(0, 255, 0);
  lcd.drawRFrame((lcd.getWidth() - lcd.getStrWidth(score) - 25) / 2, 170, lcd.getStrWidth(score) + 25, 40, 8);
  
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth(score)) / 2, 195);
  lcd.setColor(255, 0, 0);
  lcd.print(score);

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Press Red to resume...")) / 2, 275);
  lcd.setColor(255, 0, 0);
  lcd.print("Press Red to resume...");

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Press Blue to exit...")) / 2, 300);
  lcd.setColor(0, 0, 255);
  lcd.print("Press Blue to exit...");

  while(game.tetris.gamePaused == TETRIS_PAUSE_GAME);

  resumeTetris();
}

void resumeTetris()
{
  game.tetris.rotateMovingPieceDetected = false;
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
      tone(BUZZER_PIN, 500, 50);
      delay(50);
      
      drawTetrisPiece(offsetOX, offsetOY, (rotation + 270) % 360, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);
      
      break;
    }
    case TETRIS_PIECE_MOVE_DOWN | TETRIS_PIECE_ROTATE:
    {
      tone(BUZZER_PIN, 500, 50);
      delay(50);
      
      drawTetrisPiece(offsetOX, offsetOY - 16, (rotation + 270) % 360, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);

      break;
    }
    case TETRIS_PIECE_MOVE_LEFT | TETRIS_PIECE_ROTATE:
    {
      tone(BUZZER_PIN, 500, 50);
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
      tone(BUZZER_PIN, 500, 50);
      delay(50);
      
      drawTetrisPiece(offsetOX + 16, offsetOY - 16, (rotation + 270) % 360, pieceType, false);
      drawTetrisPiece(offsetOX, offsetOY, rotation, pieceType, true);

      break;
    }
    case TETRIS_PIECE_MOVE_DOWN | TETRIS_PIECE_MOVE_RIGHT | TETRIS_PIECE_ROTATE:
    {
      tone(BUZZER_PIN, 500, 50);
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

  while(gameState == GAME_OVER_TETRIS)
  {
    setLed(0, 255, 255, LED_LEFT);
    setLed(0, 255, 255, LED_RIGHT);

    delay(1);

    setLed(0, 0, 0, LED_LEFT);
    setLed(0, 0, 0, LED_RIGHT);

    delay(20);
  }

  // reset the entire console
  asm volatile ("jmp 0");
}

// Tic-Tac_Toe game implementation

void playTicTacToe()
{
  lcd.clearScreen();
  
  initTicTacToeGame();

  while(true)
  {
    waitAndUpdatePlayerMoveTicTacToeGame();

    checkIfPlayerWonAndMove();
  }
}

void initTicTacToeGame()
{  
  for (int i = 0; i < BOARD_SIZE_TIC_TAC_TOE; i++)
  {
    for (int j = 0; j < BOARD_SIZE_TIC_TAC_TOE; j++)
    {
      game.ticTacToe.board[i][j] = UNDEFINED_PLAYER_TIC_TAC_TOE;
    }
  }

  game.ticTacToe.gamePaused = TIC_TAC_TOE_PLAY_GAME;
  game.ticTacToe.selectorOX = 0;
  game.ticTacToe.selectorOY = 0;
  game.ticTacToe.nextMoveSelected = false;

  drawTicTacToeBoard();

  gameState = PLAY_GAME_TIC_TAC_TOE;
}

void drawTicTacToeBoard()
{
  int tileSize = lcd.getWidth() / BOARD_SIZE_TIC_TAC_TOE;
  int oYOffset = (lcd.getHeight() - (BOARD_SIZE_TIC_TAC_TOE * tileSize)) / 2;
  int oXOffset = (lcd.getWidth() - (BOARD_SIZE_TIC_TAC_TOE * tileSize)) / 2;
  int i, j;
  
  for (i = 0; i < BOARD_SIZE_TIC_TAC_TOE; i++)
  {
    for (j = 0; j < BOARD_SIZE_TIC_TAC_TOE; j++)
    {
      lcd.setColor(255, 200, 200);
      lcd.drawRFrame(oXOffset + j * tileSize, oYOffset + i * tileSize, tileSize, tileSize, 4);
      
      lcd.setColor(0, 0, 0);
      lcd.drawRBox(oXOffset + j * tileSize + 1, oYOffset + i * tileSize + 1, tileSize - 2, tileSize - 2, 4);

      if (game.ticTacToe.board[i][j] == X_PLAYER_TIC_TAC_TOE)
      {
        lcd.setColor(0, 255, 0);
        lcd.drawLine(oXOffset + j * tileSize + 0.35 * (tileSize / 2), oYOffset + i * tileSize + 0.35 * (tileSize / 2), oXOffset + j * tileSize + 1.65 * (tileSize / 2), oYOffset + i * tileSize + 1.65 * (tileSize / 2));
        lcd.drawLine(oXOffset + j * tileSize + 1.65 * (tileSize / 2), oYOffset + i * tileSize + 0.35 * (tileSize / 2), oXOffset + j * tileSize + 0.35 * (tileSize / 2), oYOffset + i * tileSize + 1.65 * (tileSize / 2));
      }
      else if (game.ticTacToe.board[i][j] == O_PLAYER_TIC_TAC_TOE)
      {
        lcd.setColor(255, 0, 0);
        lcd.drawCircle(oXOffset + j * tileSize + tileSize / 2, oYOffset + i * tileSize + tileSize / 2, 0.65 * (tileSize / 2), UCG_DRAW_ALL);
      }      
    }
  }

  drawCursorTicTacToeGame(false);
}

void drawCursorTicTacToeGame(bool removeCursor)
{
  int tileSize = lcd.getWidth() / BOARD_SIZE_TIC_TAC_TOE;
  int oYOffset = (lcd.getHeight() - (BOARD_SIZE_TIC_TAC_TOE * tileSize)) / 2;
  int oXOffset = (lcd.getWidth() - (BOARD_SIZE_TIC_TAC_TOE * tileSize)) / 2;
  int i, j;

  i = game.ticTacToe.selectorOX;
  j = game.ticTacToe.selectorOY;

  if (!removeCursor)
  {
    lcd.setColor(0, 255, 255);
  }
  else
  {
    lcd.setColor(0, 0, 0);
  }
  
  lcd.drawRFrame(oXOffset + i * tileSize + 4, oYOffset + j * tileSize + 4, tileSize - 8, tileSize - 8, 4);
}

void waitAndUpdatePlayerMoveTicTacToeGame()
{
  int tileSize = lcd.getWidth() / BOARD_SIZE_TIC_TAC_TOE;
  int oYOffset = (lcd.getHeight() - (BOARD_SIZE_TIC_TAC_TOE * tileSize)) / 2;
  int oXOffset = (lcd.getWidth() - (BOARD_SIZE_TIC_TAC_TOE * tileSize)) / 2;
  int i, j;
  int joystickOX = analogRead(JOYSTICK_OX_PIN);
  int joystickOY = analogRead(JOYSTICK_OY_PIN);
  char moveDirectionJoystick;
  
  while(true)
  {
    checkTicTacToeGamePause();
    
    if (game.ticTacToe.nextMoveSelected)
    {
      game.ticTacToe.nextMoveSelected = false;

      if (game.ticTacToe.board[game.ticTacToe.selectorOY][game.ticTacToe.selectorOX] == UNDEFINED_PLAYER_TIC_TAC_TOE)
      {
        tone(BUZZER_PIN, 500, 50);
        delay(50);
        
        game.ticTacToe.board[game.ticTacToe.selectorOY][game.ticTacToe.selectorOX] = X_PLAYER_TIC_TAC_TOE;

        j = game.ticTacToe.selectorOX;
        i = game.ticTacToe.selectorOY;

        lcd.setColor(0, 255, 0);
        lcd.drawLine(oXOffset + j * tileSize + 0.35 * (tileSize / 2), oYOffset + i * tileSize + 0.35 * (tileSize / 2), oXOffset + j * tileSize + 1.65 * (tileSize / 2), oYOffset + i * tileSize + 1.65 * (tileSize / 2));
        lcd.drawLine(oXOffset + j * tileSize + 1.65 * (tileSize / 2), oYOffset + i * tileSize + 0.35 * (tileSize / 2), oXOffset + j * tileSize + 0.35 * (tileSize / 2), oYOffset + i * tileSize + 1.65 * (tileSize / 2));
      
        break;
      }
    }
    
    moveDirectionJoystick = PLAYER_MOVE_UNDEFINED_TIC_TAC_TOE;
    
    if (joystickOX < JOYSTICK_RIGHT_THRESHOLD)
    {
      moveDirectionJoystick = PLAYER_MOVE_RIGHT_TIC_TAC_TOE;
    }
    else if (joystickOX > JOYSTICK_LEFT_THRESHOLD)
    {
      moveDirectionJoystick = PLAYER_MOVE_LEFT_TIC_TAC_TOE;
    }

    if (joystickOY < JOYSTICK_UP_THRESHOLD)
    {
      if (moveDirectionJoystick == PLAYER_MOVE_UNDEFINED_TIC_TAC_TOE)
      {
        moveDirectionJoystick = PLAYER_MOVE_UP_TIC_TAC_TOE;
      }
      else
      {
        moveDirectionJoystick = PLAYER_MOVE_UNDEFINED_TIC_TAC_TOE;
      }
    }
    else if (joystickOY > JOYSTICK_DOWN_THRESHOLD)
    {
      if (moveDirectionJoystick == PLAYER_MOVE_UNDEFINED_TIC_TAC_TOE)
      {
        moveDirectionJoystick = PLAYER_MOVE_DOWN_TIC_TAC_TOE;
      }
      else
      {
        moveDirectionJoystick = PLAYER_MOVE_UNDEFINED_TIC_TAC_TOE;
      }
    }

    switch(moveDirectionJoystick)
    {
      case PLAYER_MOVE_UP_TIC_TAC_TOE:
      {
        if (game.ticTacToe.selectorOY > 0)
        {
          drawCursorTicTacToeGame(true);
          game.ticTacToe.selectorOY--;
          drawCursorTicTacToeGame(false);
        }

        break;
      }
      case PLAYER_MOVE_DOWN_TIC_TAC_TOE:
      {
        if (game.ticTacToe.selectorOY < BOARD_SIZE_TIC_TAC_TOE - 1)
        {
          drawCursorTicTacToeGame(true);
          game.ticTacToe.selectorOY++;
          drawCursorTicTacToeGame(false);
        }

        break;
      }
      case PLAYER_MOVE_LEFT_TIC_TAC_TOE:
      {
        if (game.ticTacToe.selectorOX > 0)
        {
          drawCursorTicTacToeGame(true);
          game.ticTacToe.selectorOX--;
          drawCursorTicTacToeGame(false);
        }

        break;
      }
      case PLAYER_MOVE_RIGHT_TIC_TAC_TOE:
      {
        if (game.ticTacToe.selectorOX < BOARD_SIZE_TIC_TAC_TOE - 1)
        {
          drawCursorTicTacToeGame(true);
          game.ticTacToe.selectorOX++;
          drawCursorTicTacToeGame(false);
        }

        break;
      }
    }

    delay(GAME_DELAY_TIC_TAC_TOE);

    joystickOX = analogRead(JOYSTICK_OX_PIN);
    joystickOY = analogRead(JOYSTICK_OY_PIN);
  }
}

void checkTicTacToeGamePause()
{
  if (game.ticTacToe.gamePaused == TIC_TAC_TOE_PLAY_GAME)
  {
    return;
  }
  else if (game.ticTacToe.gamePaused == TIC_TAC_TOE_PAUSE_GAME)
  {
    pauseTicTacToe();
  }
}

void pauseTicTacToe()
{
  lcd.clearScreen();

  lcd.setFont(ucg_font_ncenR14_tr);
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Game paused!")) / 2, 100);
  lcd.setColor(255, 200, 200);
  lcd.print("Game paused!");

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Press Red to resume...")) / 2, 275);
  lcd.setColor(255, 0, 0);
  lcd.print("Press Red to resume...");

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Press Blue to exit...")) / 2, 300);
  lcd.setColor(0, 0, 255);
  lcd.print("Press Blue to exit...");

  while(game.ticTacToe.gamePaused == TIC_TAC_TOE_PAUSE_GAME);

  resumeTicTacToe();
}

void resumeTicTacToe()
{
  lcd.clearScreen();
  
  drawTicTacToeBoard();
}

void checkIfPlayerWonAndMove()
{
  if (checkForGameOverTicTacToe(X_PLAYER_TIC_TAC_TOE))
  {
    ticTacToeGameOver(PLAYER_WON_TIC_TAC_TOE);
  }

  generateNextMovement();
}

bool checkForGameOverTicTacToe(char player)
{
  int i, j;

  for (i = 0; i < BOARD_SIZE_TIC_TAC_TOE; i++)
  {
    j = 0;

    while (j < BOARD_SIZE_TIC_TAC_TOE && game.ticTacToe.board[i][j] == player)
    {
      j++;
    }

    if (j == BOARD_SIZE_TIC_TAC_TOE)
    {
      return true;
    }

    j = 0;

    while (j < BOARD_SIZE_TIC_TAC_TOE && game.ticTacToe.board[j][i] == player)
    {
      j++;
    }

    if (j == BOARD_SIZE_TIC_TAC_TOE)
    {
      return true;
    }
  }

  j = 0;

  while (j < BOARD_SIZE_TIC_TAC_TOE && game.ticTacToe.board[j][j] == player)
  {
    j++;
  }

  if (j == BOARD_SIZE_TIC_TAC_TOE)
  {
    return true;
  }

  j = 0;

  while (j < BOARD_SIZE_TIC_TAC_TOE && game.ticTacToe.board[j][BOARD_SIZE_TIC_TAC_TOE - j - 1] == player)
  {
    j++;
  }

  if (j == BOARD_SIZE_TIC_TAC_TOE)
  {
    return true;
  }

  return false;
}

void generateNextMovement()
{
  int tileSize = lcd.getWidth() / BOARD_SIZE_TIC_TAC_TOE;
  int oYOffset = (lcd.getHeight() - (BOARD_SIZE_TIC_TAC_TOE * tileSize)) / 2;
  int oXOffset = (lcd.getWidth() - (BOARD_SIZE_TIC_TAC_TOE * tileSize)) / 2;
  bool moved = false;
  bool gameOver = true;
  int freeCells;

  // try to win
  for (int i = 0; i < BOARD_SIZE_TIC_TAC_TOE; i++)
  {
    for (int j = 0; j < BOARD_SIZE_TIC_TAC_TOE; j++)
    {
      if (game.ticTacToe.board[i][j] == UNDEFINED_PLAYER_TIC_TAC_TOE)
      {
        game.ticTacToe.board[i][j] = O_PLAYER_TIC_TAC_TOE;

        if(checkForGameOverTicTacToe(O_PLAYER_TIC_TAC_TOE))
        {
          lcd.setColor(255, 0, 0);
          lcd.drawCircle(oXOffset + j * tileSize + tileSize / 2, oYOffset + i * tileSize + tileSize / 2, 0.65 * (tileSize / 2), UCG_DRAW_ALL);

          ticTacToeGameOver(PLAYER_LOST_TIC_TAC_TOE);
        }
        else
        {
          game.ticTacToe.board[i][j] = UNDEFINED_PLAYER_TIC_TAC_TOE;
        }
      }
    }
  }

  // try to defend
  for (int i = 0; !moved && i < BOARD_SIZE_TIC_TAC_TOE; i++)
  {
    for (int j = 0; j < BOARD_SIZE_TIC_TAC_TOE; j++)
    {
      if (game.ticTacToe.board[i][j] == UNDEFINED_PLAYER_TIC_TAC_TOE)
      {
        game.ticTacToe.board[i][j] = X_PLAYER_TIC_TAC_TOE;

        if(checkForGameOverTicTacToe(X_PLAYER_TIC_TAC_TOE))
        {
          game.ticTacToe.board[i][j] = O_PLAYER_TIC_TAC_TOE;
          moved = true;
          
          lcd.setColor(255, 0, 0);
          lcd.drawCircle(oXOffset + j * tileSize + tileSize / 2, oYOffset + i * tileSize + tileSize / 2, 0.65 * (tileSize / 2), UCG_DRAW_ALL);

          break;
        }
        else
        {
          game.ticTacToe.board[i][j] = UNDEFINED_PLAYER_TIC_TAC_TOE;
        }
      }
    }
  }

  freeCells = 0;

  for (int i = 0; i < BOARD_SIZE_TIC_TAC_TOE; i++)
  {
    for (int j = 0; j < BOARD_SIZE_TIC_TAC_TOE; j++)
    {
      if (game.ticTacToe.board[i][j] == UNDEFINED_PLAYER_TIC_TAC_TOE)
      {
        freeCells++;
      }
    }
  }

  if (freeCells > 1)
  {
    gameOver = false;
  }

  freeCells = random(analogRead(UNCONNECTED_PIN)) % freeCells;

  for (int i = 0; !moved && i < BOARD_SIZE_TIC_TAC_TOE; i++)
  {
    for (int j = 0; j < BOARD_SIZE_TIC_TAC_TOE; j++)
    {
      if (game.ticTacToe.board[i][j] == UNDEFINED_PLAYER_TIC_TAC_TOE)
      {
        if (freeCells == 0)
        {
          game.ticTacToe.board[i][j] = O_PLAYER_TIC_TAC_TOE;
          moved = true;
          
          lcd.setColor(255, 0, 0);
          lcd.drawCircle(oXOffset + j * tileSize + tileSize / 2, oYOffset + i * tileSize + tileSize / 2, 0.65 * (tileSize / 2), UCG_DRAW_ALL);

          break;
        }
        else
        {
          freeCells--;
        }
      }
    }
  }

  
  if (gameOver)
  {
    ticTacToeGameOver(TIE_TIC_TAC_TOE);
  }
}

void ticTacToeGameOver(char result)
{
  int tileSize = lcd.getWidth() / BOARD_SIZE_TIC_TAC_TOE;
  int oYOffset = (lcd.getHeight() - (BOARD_SIZE_TIC_TAC_TOE * tileSize)) / 2;
  gameState = GAME_OVER_TIC_TAC_TOE;
  
  lcd.setFont(ucg_font_ncenR14_tr);
  lcd.setPrintPos(0, 16);

  switch(result)
  {
    case PLAYER_WON_TIC_TAC_TOE:
    {
      lcd.setColor(0, 255, 0);
      lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("You won!")) / 2, oYOffset - 14);
      lcd.print("You won!");

      break;
    }
    case TIE_TIC_TAC_TOE:
    {
      lcd.setColor(255, 128, 0);
      lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Nobody won!")) / 2, oYOffset - 14);
      lcd.print("Nobody won!");

      break;
    }
    case PLAYER_LOST_TIC_TAC_TOE:
    {
      lcd.setColor(255, 0, 0);
      lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("You lost!")) / 2, oYOffset - 14);
      lcd.print("You lost!");

      break;
    }
  }

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Press joystick to continue...")) / 2, lcd.getHeight() - 14);
  lcd.setColor(255, 200, 200);
  lcd.print("Press joystick to continue...");

  while(gameState == GAME_OVER_TIC_TAC_TOE)
  {
    if (result == PLAYER_WON_TIC_TAC_TOE)
    {
      setLed(0, 255, 0, LED_LEFT);
      setLed(0, 255, 0, LED_RIGHT);
  
      delay(1);
  
      setLed(0, 0, 0, LED_LEFT);
      setLed(0, 0, 0, LED_RIGHT);
  
      delay(20);
    }
    else if (result == TIE_TIC_TAC_TOE)
    {
      setLed(255, 255, 0, LED_LEFT);
      setLed(255, 255, 0, LED_RIGHT);
  
      delay(1);
  
      setLed(0, 0, 0, LED_LEFT);
      setLed(0, 0, 0, LED_RIGHT);
  
      delay(20);
    }
    else if (result == PLAYER_LOST_TIC_TAC_TOE)
    {
      setLed(255, 0, 0, LED_LEFT);
      setLed(255, 0, 0, LED_RIGHT);
  
      delay(1);
  
      setLed(0, 0, 0, LED_LEFT);
      setLed(0, 0, 0, LED_RIGHT);
  
      delay(20);
    }
  }

  // reset the entire console
  asm volatile ("jmp 0");
}

// 2048 game implementation

void play2048()
{
  char playerMove;

  lcd.clearScreen();

  init2048Game();

  while(true)
  {
    playerMove = waitAndUpdatePlayerMove2048Game();

    draw2048Board(playerMove); 

    generateNewTile2048Game();
    
    if (!newMoveIsPossible2048Game())
    {
      // board is full and no other movement is possible; game is over
      the2048GameOver();
    }

    draw2048Board(playerMove); 
  }
}

void init2048Game()
{
  game.the2048.gamePaused = THE_2048_PLAY_GAME;
  game.the2048.score = 0;
  game.the2048.emptyCells = BOARD_SIZE_2048 * BOARD_SIZE_2048;

  for (int i = 0; i < BOARD_SIZE_2048; i++)
  {
    for (int j = 0; j < BOARD_SIZE_2048; j++)
    {
      game.the2048.board[i][j] = 0;
      game.the2048.updates[i][j] = true;
    }
  }

  generateNewTile2048Game();

  print2048Score();

  draw2048Board(DRAW_FROM_UP_2048_BOARD);

  gameState = PLAY_GAME_2048;
}

void generateNewTile2048Game()
{
  if (game.the2048.emptyCells == 0)
  {
    return;
  }
  
  unsigned int tilePosition = random(analogRead(UNCONNECTED_PIN)) % game.the2048.emptyCells;
  unsigned int tileValue;

  game.the2048.emptyCells--;

  if (random(analogRead(UNCONNECTED_PIN)) % 2 == 0)
  {
    tileValue = 2;
  }
  else
  {
    tileValue = 4;
  }

  for (int i = 0; i < BOARD_SIZE_2048; i++)
  {
    for (int j = 0; j < BOARD_SIZE_2048; j++)
    {
      if (game.the2048.board[i][j] == 0)
      {
        if (tilePosition > 0)
        {
          tilePosition--;
        }
        else
        {
          game.the2048.board[i][j] = tileValue;
          game.the2048.updates[i][j] = true;
          return;
        }
      }
    }
  }
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

void setTileColor2048(unsigned int tileValue)
{
  switch(tileValue)
  {
    case 0:
    {
      lcd.setColor(0, 0, 0);
      return;   
    }
    case 2:
    {
      lcd.setColor(255, 241, 0);
      return;   
    }
    case 4:
    {
      lcd.setColor(255, 140, 0);
      return;   
    }
    case 8:
    {
      lcd.setColor(232, 17, 35);
      return;   
    }
    case 16:
    {
      lcd.setColor(236, 0, 140);
      return;   
    }
    case 32:
    {
      lcd.setColor(104, 33, 122);
      return;   
    }
    case 64:
    {
      lcd.setColor(0, 24, 143);
      return;   
    }
    case 128:
    {
      lcd.setColor(0, 188, 242);
      return;   
    }
    case 256:
    {
      lcd.setColor(0, 178, 148);
      return;   
    }
    case 512:
    {
      lcd.setColor(0, 158, 73);
      return;   
    }
    case 1024:
    {
      lcd.setColor(186, 216, 10);
      return;   
    }
    default:
    {
      lcd.setColor(160, 121, 203);
      return;   
    }
  }
}

void draw2048Board(char drawDirection)
{
  int i, j;
  int tileSize = lcd.getWidth() / BOARD_SIZE_2048;
  int oYOffset = (lcd.getHeight() - (BOARD_SIZE_2048 * tileSize));
  int oXOffset = (lcd.getWidth() - (BOARD_SIZE_2048 * tileSize)) / 2;
  char tileValue[10] = {0};

  print2048Score();
  
  switch(drawDirection)
  {
    case DRAW_FROM_UP_2048_BOARD:
    {
      for (i = 0; i < BOARD_SIZE_2048; i++)
      {
        for (j = 0; j < BOARD_SIZE_2048; j++)
        {
          if (!game.the2048.updates[i][j])
          {
            continue;
          }

          game.the2048.updates[i][j] = false;
          
          lcd.setColor(255, 255, 255);
          lcd.drawRFrame(oXOffset + j * tileSize, oYOffset + i * tileSize, tileSize, tileSize, 4);
          
          setTileColor2048(game.the2048.board[i][j]);
          lcd.drawRBox(oXOffset + j * tileSize + 1, oYOffset + i * tileSize + 1, tileSize - 2, tileSize - 2, 4);
          
          if (game.the2048.board[i][j] > 0)
          {
            sprintf(tileValue, "%u", game.the2048.board[i][j]);

            lcd.setFont(ucg_font_cu12_hf);
            lcd.setPrintPos(oXOffset + j * tileSize + (tileSize - lcd.getStrWidth(tileValue)) / 2, oYOffset + i * tileSize + tileSize / 2 + 5);
            lcd.setColor(255, 255, 255);
            lcd.print(tileValue);
          }
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
          if (!game.the2048.updates[i][j])
          {
            continue;
          }

          game.the2048.updates[i][j] = false;
         
          lcd.setColor(255, 255, 255);          
          lcd.drawRFrame(oXOffset + j * tileSize, oYOffset + i * tileSize, tileSize, tileSize, 4);
          setTileColor2048(game.the2048.board[i][j]);
          lcd.drawRBox(oXOffset + j * tileSize + 1, oYOffset + i * tileSize + 1, tileSize - 2, tileSize - 2, 4);

          if (game.the2048.board[i][j] > 0)
          {
            sprintf(tileValue, "%u", game.the2048.board[i][j]);

            lcd.setFont(ucg_font_cu12_hf);
            lcd.setPrintPos(oXOffset + j * tileSize + (tileSize - lcd.getStrWidth(tileValue)) / 2, oYOffset + i * tileSize + tileSize / 2 + 5);
            lcd.setColor(255, 255, 255);
            lcd.print(tileValue);
          }
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
          if (!game.the2048.updates[j][i])
          {
            continue;
          }

          game.the2048.updates[j][i] = false;

          lcd.setColor(255, 255, 255);
          lcd.drawRFrame(oXOffset + i * tileSize, oYOffset + j * tileSize, tileSize, tileSize, 4);
          setTileColor2048(game.the2048.board[j][i]);
          lcd.drawRBox(oXOffset + i * tileSize + 1, oYOffset + j * tileSize + 1, tileSize - 2, tileSize - 2, 4);

          if (game.the2048.board[j][i] > 0)
          {
            sprintf(tileValue, "%u", game.the2048.board[j][i]);

            lcd.setFont(ucg_font_cu12_hf);
            lcd.setPrintPos(oXOffset + i * tileSize + (tileSize - lcd.getStrWidth(tileValue)) / 2, oYOffset + j * tileSize + tileSize / 2 + 5);
            lcd.setColor(255, 255, 255);
            lcd.print(tileValue);
          }
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
          if (!game.the2048.updates[j][i])
          {
            continue;
          }

          game.the2048.updates[j][i] = false;

          lcd.setColor(255, 255, 255);        
          lcd.drawRFrame(oXOffset + i * tileSize, oYOffset + j * tileSize, tileSize, tileSize, 4);
          setTileColor2048(game.the2048.board[j][i]);
          lcd.drawRBox(oXOffset + i * tileSize + 1, oYOffset + j * tileSize + 1, tileSize - 2, tileSize - 2, 4);

          if (game.the2048.board[j][i] > 0)
          {
            sprintf(tileValue, "%u", game.the2048.board[j][i]);

            lcd.setFont(ucg_font_cu12_hf);
            lcd.setPrintPos(oXOffset + i * tileSize + (tileSize - lcd.getStrWidth(tileValue)) / 2, oYOffset + j * tileSize + tileSize / 2 + 5);
            lcd.setColor(255, 255, 255);
            lcd.print(tileValue);
          }
        }
      }
      
      break;
    }
  }
}

char waitAndUpdatePlayerMove2048Game()
{
  // waits until player moves in a possible direction
  // returns direction of move
  int joystickOX = analogRead(JOYSTICK_OX_PIN);
  int joystickOY = analogRead(JOYSTICK_OY_PIN);
  char moveDirectionJoystick;
  
  while(true)
  {
    check2048GamePause();
    
    moveDirectionJoystick = PLAYER_MOVE_UNDEFINED_2048;
    
    if (joystickOX < JOYSTICK_RIGHT_THRESHOLD)
    {
      moveDirectionJoystick = DRAW_FROM_LEFT_2048_BOARD;
    }
    else if (joystickOX > JOYSTICK_LEFT_THRESHOLD)
    {
      moveDirectionJoystick = DRAW_FROM_RIGHT_2048_BOARD;
    }

    if (joystickOY < JOYSTICK_UP_THRESHOLD)
    {
      if (moveDirectionJoystick == PLAYER_MOVE_UNDEFINED_2048)
      {
        moveDirectionJoystick = DRAW_FROM_DOWN_2048_BOARD;
      }
      else
      {
        moveDirectionJoystick = PLAYER_MOVE_UNDEFINED_2048;
      }
    }
    else if (joystickOY > JOYSTICK_DOWN_THRESHOLD)
    {
      if (moveDirectionJoystick == PLAYER_MOVE_UNDEFINED_2048)
      {
        moveDirectionJoystick = DRAW_FROM_UP_2048_BOARD;
      }
      else
      {
        moveDirectionJoystick = PLAYER_MOVE_UNDEFINED_2048;
      }
    }

    if (moveDirectionJoystick != PLAYER_MOVE_UNDEFINED_2048 && moveIfPossible2048Game(moveDirectionJoystick))
    {
      tone(BUZZER_PIN, 500, 50);
      delay(50);
      
      return moveDirectionJoystick;
    }

    delay(GAME_DELAY_2048);

    joystickOX = analogRead(JOYSTICK_OX_PIN);
    joystickOY = analogRead(JOYSTICK_OY_PIN);
  }
}

void check2048GamePause()
{
  if (game.the2048.gamePaused == THE_2048_PLAY_GAME)
  {
    return;
  }
  else if (game.the2048.gamePaused == THE_2048_PAUSE_GAME)
  {
    pause2048();
  }
}

void pause2048()
{
  char score[15];
  sprintf(score, "%lu", game.the2048.score);
  
  lcd.clearScreen();

  lcd.setFont(ucg_font_ncenR14_tr);
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Game paused!")) / 2, 100);
  lcd.setColor(255, 200, 200);
  lcd.print("Game paused!");

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Your score:")) / 2, 150);
  lcd.print("Your score:");

  lcd.setColor(0, 255, 0);
  lcd.drawRFrame((lcd.getWidth() - lcd.getStrWidth(score) - 25) / 2, 170, lcd.getStrWidth(score) + 25, 40, 8);
  
  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth(score)) / 2, 195);
  lcd.setColor(255, 0, 0);
  lcd.print(score);

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Press Red to resume...")) / 2, 275);
  lcd.setColor(255, 0, 0);
  lcd.print("Press Red to resume...");

  lcd.setPrintPos((lcd.getWidth() - lcd.getStrWidth("Press Blue to exit...")) / 2, 300);
  lcd.setColor(0, 0, 255);
  lcd.print("Press Blue to exit...");

  while(game.the2048.gamePaused == THE_2048_PAUSE_GAME);

  resume2048();
}

void resume2048()
{
  for (int i = 0; i < BOARD_SIZE_2048; i++)
  {
    for (int j = 0; j < BOARD_SIZE_2048; j++)
    {
      game.the2048.updates[i][j] = true;
    }
  }

  lcd.clearScreen();
  
  draw2048Board(DRAW_FROM_UP_2048_BOARD);
}

bool moveIfPossible2048Game(int moveDirection)
{
  bool movedDone = false;
  int i, k;

  switch (moveDirection)
  {
      case DRAW_FROM_UP_2048_BOARD:
      {
        for (int j = 0; j < BOARD_SIZE_2048; j++)
        {
          k = BOARD_SIZE_2048 - 1;
          i = BOARD_SIZE_2048 - 1;

          while(i >= 0)
          {
            while(i >= 0 && game.the2048.board[i][j] == 0)
            {
              i--;
            }

            if (i >= 0 && game.the2048.board[i][j] != 0)
            {
              game.the2048.board[k][j] = game.the2048.board[i][j];
              
              if (i < k)
              {
                movedDone = true;
                game.the2048.board[i][j] = 0;
                
                game.the2048.updates[i][j] = true;
                game.the2048.updates[k][j] = true;
              }

              k--;
            }

            i--;
          }

          for (i = BOARD_SIZE_2048 - 1; i > 0 && game.the2048.board[i - 1][j] > 0; i--)
          {
            if (game.the2048.board[i][j] == game.the2048.board[i - 1][j])
            {
              game.the2048.score += game.the2048.board[i][j];
              
              game.the2048.board[i][j] *= 2;
              game.the2048.board[i - 1][j] = 0;

              game.the2048.updates[i][j] = true;
              game.the2048.updates[i - 1][j] = true;
              
              game.the2048.emptyCells++;

              movedDone = true;
              i--;
            }
          }
          
          k = BOARD_SIZE_2048 - 1;
          i = BOARD_SIZE_2048 - 1;

          while(i >= 0)
          {
            while(i >= 0 && game.the2048.board[i][j] == 0)
            {
              i--;
            }

            if (i >= 0 && game.the2048.board[i][j] != 0)
            {
              game.the2048.board[k][j] = game.the2048.board[i][j];
              
              if (i < k)
              {
                movedDone = true;
                game.the2048.board[i][j] = 0;
                
                game.the2048.updates[i][j] = true;
                game.the2048.updates[k][j] = true;
              }

              k--;
            }

            i--;
          }
        }

        break;
      }
      case DRAW_FROM_DOWN_2048_BOARD:
      {
        for (int j = 0; j < BOARD_SIZE_2048; j++)
        {
          k = 0;
          i = 0;

          while(i < BOARD_SIZE_2048)
          {
            while(i < BOARD_SIZE_2048 && game.the2048.board[i][j] == 0)
            {
              i++;
            }

            if (i < BOARD_SIZE_2048 && game.the2048.board[i][j] != 0)
            {
              game.the2048.board[k][j] = game.the2048.board[i][j];
              
              if (i > k)
              {
                movedDone = true;
                game.the2048.board[i][j] = 0;

                game.the2048.updates[i][j] = true;
                game.the2048.updates[k][j] = true;
              }

              k++;
            }

            i++;
          }

          for (i = 0; i < BOARD_SIZE_2048 && game.the2048.board[i + 1][j] > 0; i++)
          {
            if (game.the2048.board[i][j] == game.the2048.board[i + 1][j])
            {
              game.the2048.score += game.the2048.board[i][j];
              
              game.the2048.board[i][j] *= 2;
              game.the2048.board[i + 1][j] = 0;

              game.the2048.updates[i][j] = true;
              game.the2048.updates[i + 1][j] = true;
                
              game.the2048.emptyCells++;

              movedDone = true;
              i++;
            }
          }
          
          k = 0;
          i = 0;

          while(i < BOARD_SIZE_2048)
          {
            while(i < BOARD_SIZE_2048 && game.the2048.board[i][j] == 0)
            {
              i++;
            }

            if (i < BOARD_SIZE_2048 && game.the2048.board[i][j] != 0)
            {
              game.the2048.board[k][j] = game.the2048.board[i][j];
              
              if (i > k)
              {
                movedDone = true;
                game.the2048.board[i][j] = 0;

                game.the2048.updates[i][j] = true;
                game.the2048.updates[k][j] = true;
              }

              k++;
            }

            i++;
          }
        }

        break;
      }
      case DRAW_FROM_LEFT_2048_BOARD:
      {
        for (int j = 0; j < BOARD_SIZE_2048; j++)
        {
          k = BOARD_SIZE_2048 - 1;
          i = BOARD_SIZE_2048 - 1;

          while(i >= 0)
          {
            while(i >= 0 && game.the2048.board[j][i] == 0)
            {
              i--;
            }

            if (i >= 0 && game.the2048.board[j][i] != 0)
            {
              game.the2048.board[j][k] = game.the2048.board[j][i];
              
              if (i < k)
              {
                movedDone = true;
                game.the2048.board[j][i] = 0;

                game.the2048.updates[j][i] = true;
                game.the2048.updates[j][k] = true;
              }

              k--;
            }

            i--;
          }

          for (i = BOARD_SIZE_2048 - 1; i > 0 && game.the2048.board[j][i - 1] > 0; i--)
          {
            if (game.the2048.board[j][i] == game.the2048.board[j][i - 1])
            {
              game.the2048.score += game.the2048.board[j][i];
              
              game.the2048.board[j][i] *= 2;
              game.the2048.board[j][i - 1] = 0;

              game.the2048.updates[j][i] = true;
              game.the2048.updates[j][i - 1] = true;
                
              game.the2048.emptyCells++;

              movedDone = true;
              i--;
            }
          }
          
          k = BOARD_SIZE_2048 - 1;
          i = BOARD_SIZE_2048 - 1;

          while(i >= 0)
          {
            while(i >= 0 && game.the2048.board[j][i] == 0)
            {
              i--;
            }

            if (i >= 0 && game.the2048.board[j][i] != 0)
            {
              game.the2048.board[j][k] = game.the2048.board[j][i];
              
              if (i < k)
              {
                movedDone = true;
                game.the2048.board[j][i] = 0;

                game.the2048.updates[j][i] = true;
                game.the2048.updates[j][k] = true;
              }

              k--;
            }

            i--;
          }
        }

        break;
      }
      case DRAW_FROM_RIGHT_2048_BOARD:
      {
        for (int j = 0; j < BOARD_SIZE_2048; j++)
        {
          k = 0;
          i = 0;

          while(i < BOARD_SIZE_2048)
          {
            while(i < BOARD_SIZE_2048 && game.the2048.board[j][i] == 0)
            {
              i++;
            }

            if (i < BOARD_SIZE_2048 && game.the2048.board[j][i] != 0)
            {
              game.the2048.board[j][k] = game.the2048.board[j][i];
              
              if (i > k)
              {
                movedDone = true;
                game.the2048.board[j][i] = 0;
                
                game.the2048.updates[j][i] = true;
                game.the2048.updates[j][k] = true;
              }

              k++;
            }

            i++;
          }

          for (i = 0; i < BOARD_SIZE_2048 && game.the2048.board[j][i + 1] > 0; i++)
          {
            if (game.the2048.board[j][i] == game.the2048.board[j][i + 1])
            {
              game.the2048.score += game.the2048.board[j][i];
              
              game.the2048.board[j][i] *= 2;
              game.the2048.board[j][i + 1] = 0;

              game.the2048.updates[j][i] = true;
              game.the2048.updates[j][i + 1] = true;
                
              game.the2048.emptyCells++;

              movedDone = true;
              i++;
            }
          }
          
          k = 0;
          i = 0;

          while(i < BOARD_SIZE_2048)
          {
            while(i < BOARD_SIZE_2048 && game.the2048.board[j][i] == 0)
            {
              i++;
            }

            if (i < BOARD_SIZE_2048 && game.the2048.board[j][i] != 0)
            {
              game.the2048.board[j][k] = game.the2048.board[j][i];
              
              if (i > k)
              {
                movedDone = true;
                game.the2048.board[j][i] = 0;

                game.the2048.updates[j][i] = true;
                game.the2048.updates[j][k] = true;
              }

              k++;
            }

            i++;
          }
        }

        break;
      }
  }

  return movedDone;
}

bool newMoveIsPossible2048Game()
{
  if (game.the2048.emptyCells > 0)
  {
    return true;
  }
  
  for (int i = 0; i < BOARD_SIZE_2048; i++)
  {
    for (int j = 1; j < BOARD_SIZE_2048; j++)
    {
      if (game.the2048.board[i][j] == game.the2048.board[i][j - 1])
      {
        return true;
      }

      if (game.the2048.board[j][i] == game.the2048.board[j - 1][i])
      {
        return true;
      }
    }
  }

  return false;
}

void the2048GameOver()
{
  char score[15];
  sprintf(score, "%lu", game.the2048.score);
  
  lcd.clearScreen();

  gameState = GAME_OVER_THE_2048;
  
  lcd.clearScreen();

  lcd.setFont(ucg_font_ncenR14_tr);
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

  while(gameState == GAME_OVER_THE_2048)
  {
    setLed(0, 255, 255, LED_LEFT);
    setLed(0, 255, 255, LED_RIGHT);

    delay(1);

    setLed(0, 0, 0, LED_LEFT);
    setLed(0, 0, 0, LED_RIGHT);

    delay(20);
  }

  // reset the entire console
  asm volatile ("jmp 0");
}
