#include <stdbool.h>  // For bool, true, false
#include <stdlib.h>   // For abs, rand

volatile int pixel_buffer_start;  // global variable
short int Buffer1[240][512];      // 240 rows, 512 (320 + padding) columns
short int Buffer2[240][512];

// GLOBALS
#define BALL_RADIUS 10
#define BALL_COLOR 0x001F
#define BALL_SPEED 2
// fixed point scale
#define SCALE 1000

#define PADDLE_RADIUS 12
#define PADDLE_COLOR 0xFFFFF

/*This is struct for Ball*/
struct Ball {
  // center of ball
  int xc;
  int yc;
  // velocity of ball
  double dxBall;
  double dyBall;
};

/*Declare all three generations of ball (hockey puck)*/
// this is the one that is about to be drawn (back buffer about to swap to front
// buffer) swap 2
struct Ball ball;
// this is the one that has been drawn, (back buffer that has already swapped to
// front buffer) swap 1
struct Ball ballOld;

// this is the previous ball that had been drawn
struct Ball ballOldOld;

/*This is struct for Paddle*/
struct Paddle {
  // center of paddle
  int xc;
  int yc;
  // paddle number
  int paddleNumber;
  // keyPressed
  bool upPressed;
  bool downPressed;
  bool leftPressed;
  bool rightPressed;
};

// arrow keys, paddle 1
struct Paddle paddle1;
struct Paddle paddle1Old;
struct Paddle paddle1OldOld;

// wasd keys, paddle 2
struct Paddle paddle2;
struct Paddle paddle2Old;
struct Paddle paddle2OldOld;

void bounceVector(struct Ball *ball, struct Paddle *p);
bool collidedWPaddle(struct Ball *ball, struct Paddle *p);

bool p1Hit = false;
bool p2Hit = false;

unsigned char keyInput() {
  volatile int *PS2Base = (int *)0xFF200100;
  int PS2Data = *PS2Base;

  // 0x8000 masks the rvalid bit, bit 15, checks if there is unread byte in fif0
  if ((PS2Data & 0x8000) != 0) {
    unsigned char pressedKey = (char)(PS2Data & 0xFF);  // lower 8 bits
    return pressedKey;
  }
  return 0x00;
}

void processKeyInput() {
  unsigned char byte1, byte2, byte3;

  byte1 = keyInput();

  if (byte1 != 0x00) {
    // arrow keys require two bytes of data, make code and data code
    // 0xE0 is make code for arrow keys
    if (byte1 == 0xE0) {
      byte2 = keyInput();

      // 0xF0 is the release code
      if (byte2 == 0xF0) {
        byte3 = keyInput();

        switch (byte3) {
          case 0x75:
            paddle1.upPressed = false;
            break;
          case 0x72:
            paddle1.downPressed = false;
            break;
          case 0x6B:
            paddle1.leftPressed = false;
            break;
          case 0x74:
            paddle1.rightPressed = false;
            break;
        }
      } else {
        switch (byte2) {
          case 0x75:
            paddle1.upPressed = true;
            break;
          case 0x72:
            paddle1.downPressed = true;
            break;
          case 0x6B:
            paddle1.leftPressed = true;
            break;
          case 0x74:
            paddle1.rightPressed = true;
            break;
        }
      }
    }
    // WASD keys, no 0xE0 prefix

    // if released
    else if (byte1 == 0xF0) {
      byte2 = keyInput();
      switch (byte2) {
        case 0x1D:  // W
          paddle2.upPressed = false;
          break;
        case 0x1B:  // S
          paddle2.downPressed = false;
          break;
        case 0x1C:  // A
          paddle2.leftPressed = false;
          break;
        case 0x23:  // D
          paddle2.rightPressed = false;
          break;
      }
    }
    // if pressed
    else {
      switch (byte1) {
        case 0x1D:
          paddle2.upPressed = true;
          break;
        case 0x1B:
          paddle2.downPressed = true;
          break;
        case 0x1C:
          paddle2.leftPressed = true;
          break;
        case 0x23:
          paddle2.rightPressed = true;
          break;
      }
    }
  }
}

void updatePaddlePosition(struct Paddle *paddle, bool pHit) {
  if (pHit) {
    return;
  }
  if (paddle->upPressed && paddle->leftPressed) {
    paddle->yc -= 2;
    paddle->xc -= 2;
  } else if (paddle->upPressed && paddle->rightPressed) {
    paddle->yc -= 2;
    paddle->xc += 2;
  } else if (paddle->downPressed && paddle->leftPressed) {
    paddle->yc += 2;
    paddle->xc -= 2;
  } else if (paddle->downPressed && paddle->rightPressed) {
    paddle->yc += 2;
    paddle->xc += 2;
  } else if (paddle->upPressed) {
    paddle->yc -= 2;
  } else if (paddle->downPressed) {
    paddle->yc += 2;
  } else if (paddle->leftPressed) {
    paddle->xc -= 2;
  } else if (paddle->rightPressed) {
    paddle->xc += 2;
  }
}

void screenLimits(struct Paddle *paddle) {
  if (paddle->yc - PADDLE_RADIUS < 1) {
    paddle->upPressed = false;
  }
  // bottom wall
  if (paddle->yc + PADDLE_RADIUS > 239) {
    paddle->downPressed = false;
  }

  // left wall
  if (paddle->xc - PADDLE_RADIUS < 1) {
    paddle->leftPressed = false;
  }
  // right wall
  if (paddle->xc + PADDLE_RADIUS > 319) {
    paddle->rightPressed = false;
  }

  // middle wall

  // for paddle 1, right side
  if (paddle->paddleNumber == 1 && paddle->xc - PADDLE_RADIUS < 161) {
    paddle->leftPressed = false;
  }
  // for paddle 2, left side
  if (paddle->paddleNumber == 2 && paddle->xc + PADDLE_RADIUS > 159) {
    paddle->rightPressed = false;
  }

  if (collidedWPaddle(&ball, paddle)) {
    // Left wall collision
    if (ball.xc == BALL_RADIUS &&
        paddle->xc - PADDLE_RADIUS < ball.xc + BALL_RADIUS) {
      paddle->leftPressed = false;
    }
    // Right wall collision
    if (ball.xc == (320 - BALL_RADIUS) &&
        paddle->xc + PADDLE_RADIUS > ball.xc - BALL_RADIUS) {
      paddle->rightPressed = false;
    }
    // Top wall collision
    if (ball.yc == BALL_RADIUS &&
        paddle->yc - PADDLE_RADIUS < ball.yc + BALL_RADIUS) {
      paddle->upPressed = false;
    }
    // Bottom wall collision
    if (ball.yc == (240 - BALL_RADIUS) &&
        paddle->yc + PADDLE_RADIUS > ball.yc - BALL_RADIUS) {
      paddle->downPressed = false;
    }
  }
}

void plot_pixel(int x, int y, short int line_color) {
  volatile short int *one_pixel_address;

  one_pixel_address = pixel_buffer_start + (y << 10) + (x << 1);

  *one_pixel_address = line_color;
}

// this one is the initial one to make all that random nonsense at the beginning
// dissappear
void clear_screen() {
  int x, y;
  for (x = 0; x < 320; x++) {
    for (y = 0; y < 240; y++) {
      plot_pixel(x, y, 0);
    }
  }
}

// Function to draw a circle using the Midpoint Circle Algorithm (Algo inspired
// by BASIC256 at https://rosettacode.org/wiki/Bitmap/Midpoint_circle_algorithm)
void draw_circle(int xc, int yc, int r, short int color) {
  int x = 0, y = r;
  // decision variable (will help decide if we move horizontally or diagonally)
  int d = 1 - r;

  while (x <= y) {
    // We exploit the symmetry of a circle and draw 8 points at once.
    // This allows to compute only 1 octant of a circle instead of every point
    plot_pixel(xc + x, yc + y, color);
    plot_pixel(xc - x, yc + y, color);
    plot_pixel(xc + x, yc - y, color);
    plot_pixel(xc - x, yc - y, color);
    plot_pixel(xc + y, yc + x, color);
    plot_pixel(xc - y, yc + x, color);
    plot_pixel(xc + y, yc - x, color);
    plot_pixel(xc - y, yc - x, color);

    // based on decision variable, change the y coordinate
    if (d < 0) {
      d += 2 * x + 3;
    } else {
      // next pixel move diagonally
      d += 2 * (x - y) + 5;
      y--;
    }
    x++;
  }
}

// NEW
void erase_circle(int xc, int yc, int radius) {
  draw_circle(xc, yc, radius, 0x0);
}

// function from class
void wait_for_vsync() {
  volatile int *pixel_ctrl_ptr = (int *)0xff203020;  // base address
  int status;
  *pixel_ctrl_ptr = 1;  // start the synchronization process
  // write 1 into front buffer address register
  status = *(pixel_ctrl_ptr + 3);  // read the status register
  while ((status & 0x01) != 0)     // polling loop waiting for S bit to go to 0
  {
    status = *(pixel_ctrl_ptr + 3);
  }  // polling loop/function exits when status bit goes to 0
}

void drawLine() {
  for (int y = 0; y < 240; y++) {
    plot_pixel(159, y, 0xFFFF);
  }
}

void eraseLine() {
  for (int y = 0; y < 240; y++) {
    plot_pixel(159, y, 0x0);  // Black color
  }
}


/*MILESTONE 2*/
// use the reflection of a vector to calculate bounce vector off paddle
// surface type: wall(0) or paddle(1) already identified in the main code
// bounce vector = ballV + 2(ballV dot Normal)
void bounceVector(struct Ball *ball, struct Paddle *p) {
  // find vector normal to the hit surface relative to the ball
  double nx = ball->xc - p->xc;
  double ny = ball->yc - p->yc;

  // magnitude of n
  double magN = sqrt(nx * nx + ny * ny);

  // unit vector of n
  nx /= magN;
  ny /= magN;

  // handle bounce
  // ball velocity vector should be reversed to point away from paddle centre
  // so that dot product works
  double dotBallN = ball->dxBall * nx + ball->dyBall * ny;
  // reflected vector = v+2(vdotn)n
  ball->dxBall -= 2 * dotBallN * nx;
  ball->dyBall -= 2 * dotBallN * ny;

  ball->xc += nx * (BALL_RADIUS + PADDLE_RADIUS - magN + 1);
  ball->yc += ny * (BALL_RADIUS + PADDLE_RADIUS - magN + 1);
}

bool collidedWPaddle(struct Ball *ball, struct Paddle *p) {
  double deltax = ball->xc - p->xc;
  double deltay = ball->yc - p->yc;
  double currdelta = deltax * deltax + deltay * deltay;  // magnitude ^2
  double conditionDelta = BALL_RADIUS + PADDLE_RADIUS;
  return (currdelta <=
          conditionDelta * conditionDelta);  // magnitude condition ^2
}

/*MILESTONE 2*/

int main(void) {
  volatile int *pixel_ctrl_ptr = (int *)0xFF203020;

  // initialize location and direction of ball
  ball.xc = 300;
  ball.yc = 200;

  // ball.dxBall = ((rand() % 2)*2 - 1) * BALL_SPEED;
  // ball.dyBall = ((rand() % 2)*2 - 1) * BALL_SPEED;

  ball.dxBall = 0 * BALL_SPEED * SCALE;
  ball.dyBall = 1 * BALL_SPEED * SCALE;

  ballOld = ball;
  ballOldOld = ball;

  // initialize locations of paddle1
  paddle1.xc = 300;
  paddle1.yc = 20;
  paddle1.paddleNumber = 1;
  paddle1Old = paddle1;
  paddle1OldOld = paddle1;

  // initialize locations of paddle2
  paddle2.xc = 50;
  paddle2.yc = 50;
  paddle2.paddleNumber = 2;
  paddle2Old = paddle2;
  paddle2OldOld = paddle2;

  /* set front pixel buffer to Buffer 1 */
  // first store the address in the  back buffer
  *(pixel_ctrl_ptr + 1) = (int)&Buffer1;
  /* now, swap the front/back buffers, to set the front buffer location */
  wait_for_vsync();
  /* initialize a pointer to the pixel buffer, used by drawing functions */
  pixel_buffer_start = *pixel_ctrl_ptr;
  clear_screen();  // pixel_buffer_start points to the pixel buffer

  /* set back pixel buffer to Buffer 2 */
  *(pixel_ctrl_ptr + 1) = (int)&Buffer2;
  pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // we draw on the back buffer
  clear_screen();  // pixel_buffer_start points to the pixel buffer

  // main loop
  while (1) {
    // OLD
    // /* Erase any balls and paddles that were drawn in the last iteration */
    // erase();

    // NEW
    // erase ball
    erase_circle(ballOldOld.xc, ballOldOld.yc, BALL_RADIUS);

    // erase paddle1
    erase_circle(paddle1OldOld.xc, paddle1OldOld.yc, PADDLE_RADIUS);
    // erase paddle2
    erase_circle(paddle2OldOld.xc, paddle2OldOld.yc, PADDLE_RADIUS);

    // draw the balls
    draw_circle(ball.xc, ball.yc, BALL_RADIUS, BALL_COLOR);

    // draw paddle1
    draw_circle(paddle1.xc, paddle1.yc, PADDLE_RADIUS, PADDLE_COLOR);
    // draw paddle2
    draw_circle(paddle2.xc, paddle2.yc, PADDLE_RADIUS, PADDLE_COLOR);

    drawLine();

    /*BALL*/
    // Update the locations of the ball
    ballOldOld = ballOld;  //(can do this in C)
    // update old location before changing
    ballOld = ball;

    // update location
    // divide by 100 for fixed point arithmetic
    ball.xc += (int)(ball.dxBall / SCALE);
    ball.yc += (int)(ball.dyBall / SCALE);
    // check boundaries and update velocities
    // CHECK IF BALL HAS HIT WALLS

    /*PADDLE*/
    // Update locations of paddle1
    paddle1OldOld = paddle1Old;
    paddle1Old = paddle1;

    // Update locations of paddle2
    paddle2OldOld = paddle2Old;
    paddle2Old = paddle2;

    // update paddle position
    processKeyInput();

    // check if either paddles are touching game limits
    screenLimits(&paddle1);
    screenLimits(&paddle2);

    p1Hit = collidedWPaddle(&ball, &paddle1);
    p2Hit = collidedWPaddle(&ball, &paddle2);

    if (p1Hit) {
      printf("collision w paddle1");
    }
    if (p2Hit) {
      printf("collision w paddle2");
    }
    // update paddle1
    updatePaddlePosition(&paddle1, p1Hit);

    // update paddle2
    updatePaddlePosition(&paddle2, p2Hit);

    // Top wall collision
    if (ball.yc - BALL_RADIUS < 0) {
      ball.dyBall *= -1;
      ball.yc = (BALL_RADIUS + 1);
    }
    // Bottom wall collision
    if (ball.yc + BALL_RADIUS > 240) {
      ball.dyBall *= -1;
      ball.yc = 240 - (BALL_RADIUS + 1);
    }
    // Left wall collision
    if (ball.xc - BALL_RADIUS < 0) {
      ball.dxBall *= -1;
      ball.xc = (BALL_RADIUS + 1);
    }
    // Right wall collision
    if (ball.xc + BALL_RADIUS > 320) {
      ball.dxBall *= -1;
      ball.xc = 320 - (BALL_RADIUS + 1);
    }

    if (p1Hit) {
      bounceVector(&ball, &paddle1);
    }

    else if (p2Hit) {
      bounceVector(&ball, &paddle2);
    }

    wait_for_vsync();  // swap front and back buffers on VGA vertical sync
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer
  }
}