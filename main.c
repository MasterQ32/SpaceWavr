#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

extern int8_t const sine_lut[256];

typedef struct 
{
  int16_t x;
  int16_t y;
} Vector2;

typedef struct 
{
  Vector2 position;
  uint8_t angle;

  Vector2 velocity;

  bool fire_pressed;
} Player;

typedef struct {
  uint16_t alive;
  Vector2 position;
  Vector2 velocity;
  Player * owner;
} Shot;


#define NUM_SHOTS 16
Shot shots[NUM_SHOTS];

Shot * alloc_shot()
{
  for(size_t i = 0; i < NUM_SHOTS; i++) {
    if(shots[i].alive) 
      continue;
    return &shots[i];
  }
  return NULL;
}

inline int16_t mult_sine(int16_t value, uint8_t angle)
{
  return ((int32_t)value * sine_lut[angle]) / 128;
}

void set_beam(bool on)
{
  if(on) {
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      PORTA &= ~0x01; // beam on
  }
  else {
      PORTA |= 0x01; // beam off
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
  }
}

void move_cursor(int16_t x, int16_t y)
{
  PORTB = 128 + x / 256;
  PORTD = 128 + y / 256;
}

void paint_point(int16_t x, int16_t y)
{
  set_beam(false);
  move_cursor(x, y);
  set_beam(true);
  _delay_us(15);
  set_beam(false);
}

void paint_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
  set_beam(false);
  move_cursor(x1, y1);
  set_beam(true);

  const int divs = 16;
  for(int i = 0; i < divs; i++)
  {
    int32_t x = i * (int32_t)(x2 - x1) / divs + x1;
    int32_t y = i * (int32_t)(y2 - y1) / divs + y1;

    if(x > SHRT_MAX || y > SHRT_MAX)
      continue;

    if(x < SHRT_MIN || y < SHRT_MIN)
      continue;

    move_cursor(x, y);
  }

  move_cursor(x2, y2);
  set_beam(false);
}

void paint_linept(Vector2 from, Vector2 to)
{
  paint_line(from.x, from.y, to.x, to.y);
}

void paint_player(Player const * player)
{
  const int size_x = 8 * 256;
  const int size_y = 12 * 256;

  Vector2 corners[3] = {
    { -size_x, -size_y },
    {  size_x, -size_y },
    {       0,  size_y }
  };

  for(int i = 0; i < 3; i++) {

    Vector2 cpy = corners[i];

    corners[i].x = mult_sine(cpy.x, player->angle + 64) + mult_sine(cpy.y, player->angle);
    corners[i].y = mult_sine(cpy.x, player->angle) - mult_sine(cpy.y, player->angle + 64);

    corners[i].x += player->position.x;
    corners[i].y += player->position.y;
  }

  for(int i = 0; i < 3; i++) {
    paint_linept(corners[i], corners[(i + 1) % 3]);
  }
}

#define INPUT1_LEFT  (1<<6)
#define INPUT1_RIGHT (1<<7)
#define INPUT1_ACCEL (1<<5)
#define INPUT1_FIRE  (1<<4)

#define INPUT2_LEFT  (1<<2)
#define INPUT2_RIGHT (1<<3)
#define INPUT2_ACCEL (1<<1)
#define INPUT2_FIRE  (1<<0)

bool is_input_pressed(uint8_t mask)
{
  return (PINC & mask) == 0;
}

Vector2 direction_for_angle(uint8_t angle)
{
  int16_t dx = sine_lut[angle];
  int16_t dy = sine_lut[(angle + 64) & 0xFF];
  return (Vector2) { dx, -dy };
}

static void paint_digit(int16_t x, int16_t y, unsigned char num)
{
  if(num > 0xF)
    return;
  static const uint8_t bitmasks[] = {
    // o--0--o
    // |     |
    // 1     2
    // |     |
    // o--3--o
    // |     |
    // 4     5
    // |     |
    // o--6--o
    0x77, // 0
    0x24, // 1
    0x5D, // 2
    0x6D, // 3
    0x2E, // 4
    0x6B, // 5
    0x7A, // 6
    0x25, // 7
    0x7F, // 8
    0x6F, // 9
    0x3F, // A
    0x7A, // B
    0x53, // C
    0x7C, // D
    0x5B, // E
    0x1B, // F
  };


  const int16_t size = 256 * 8;
  const Vector2 dots[] = 
  {
    // 0---1
    // |   |
    // 2---3
    // |   |
    // 4---5
    { x, y, },
    { x + size, y, },
    { x, y - size, },
    { x + size, y - size, },
    { x, y - 2 * size, },
    { x + size, y - 2 * size, },
  };

  const uint8_t mask = bitmasks[num];
  if(mask & 0x01) paint_line(dots[0].x, dots[0].y, dots[1].x, dots[1].y);
  if(mask & 0x02) paint_line(dots[0].x, dots[0].y, dots[2].x, dots[2].y);
  if(mask & 0x04) paint_line(dots[1].x, dots[1].y, dots[3].x, dots[3].y);
  if(mask & 0x08) paint_line(dots[2].x, dots[2].y, dots[3].x, dots[3].y);
  if(mask & 0x10) paint_line(dots[2].x, dots[2].y, dots[4].x, dots[4].y);
  if(mask & 0x20) paint_line(dots[3].x, dots[3].y, dots[5].x, dots[5].y);
  if(mask & 0x40) paint_line(dots[4].x, dots[4].y, dots[5].x, dots[5].y);
}

typedef struct 
{
  unsigned char value;
  unsigned char left_char;
  unsigned char right_char;
} Score;

void refresh_score(Score * score)
{
  score->left_char = score->value / 10;
  score->right_char = score->value % 10;
}

void paint_score(int16_t x, int16_t y, Score const * score)
{
  paint_digit(x + 0,        y, score->left_char);
  paint_digit(x + 12 * 256, y, score->right_char);
}

enum { LEFT, RIGHT, FIRE, ACCELERATE };
static void update_player(Player * player, uint8_t const inputs[])
{
  if(is_input_pressed(inputs[ACCELERATE])) {
    Vector2 delta = direction_for_angle(player->angle);

    const int dx = delta.x / 32;
    const int dy = delta.y / 32;

    if(abs(player->velocity.x + dx) < 512)
      player->velocity.x += dx;
    if(abs(player->velocity.y + dy) < 512)
      player->velocity.y += dy;

  } else {
    if(player->velocity.x > 0)
      player->velocity.x -= 1;
    else if(player->velocity.x < 0)
      player->velocity.x += 1;
    if(player->velocity.y > 0)
      player->velocity.y -= 1;
    else if(player->velocity.y < 0)
      player->velocity.y += 1;
  }

  if(is_input_pressed(inputs[LEFT])) {
    player->angle -= 1;
  }

  if(is_input_pressed(inputs[RIGHT])) {
    player->angle += 1;
  }

  if(is_input_pressed(inputs[FIRE])) {
    if(!player->fire_pressed) {
      Shot * shot = alloc_shot();
      if(shot != NULL) {
        shot->position = player->position;
        shot->velocity = direction_for_angle(player->angle);
        shot->velocity.x *= 2;
        shot->velocity.y *= 2;
        shot->alive = 256; // 1000 frames alive
        shot->owner = player;
      }
    }
    player->fire_pressed = true;
  } else {
    player->fire_pressed = false;
  }

  player->position.x += player->velocity.x;
  player->position.y += player->velocity.y;
}


  Score p1Score = { 0 };
  Score p2Score = { 0 };

Player player1, player2;

static bool collider_test(int x0, int y0, int r0, int x1, int y1, int r1)
{
  int32_t dx = (int32_t)x0 - (int32_t)x1;
  int32_t dy = (int32_t)y0 - (int32_t)y1;
  int16_t r = r0 + r1;
  if(dx < 0) dx = -dx;
  if(dy < 0) dy = -dy;
  return (dx <= r) && (dy <= r);
}

static void paint_playfield()
{
  paint_player(&player1);
  paint_player(&player2);

  paint_score( 104 * 256, 126 * 256, &p1Score);
  paint_score(-127 * 256, 126 * 256, &p2Score);

  for(size_t i = 0; i < NUM_SHOTS; i++)
  {
    if(shots[i].alive)
    paint_point(shots[i].position.x, shots[i].position.y);
  }
}

static void explosions(int mask)
{
  for(int i = 0; i < 400; i++)
  {
    paint_playfield();

    if(mask & 1)
    {
      paint_line(
        player1.position.x + (rand() % 0x1FFF) - 0x1000,
        player1.position.y + (rand() % 0x1FFF) - 0x1000,
        player1.position.x + (rand() % 0x1FFF) - 0x1000,
        player1.position.y + (rand() % 0x1FFF) - 0x1000
      );
    }

    if(mask & 2)
    {
      paint_line(
        player2.position.x + (rand() % 0x1FFF) - 0x1000,
        player2.position.y + (rand() % 0x1FFF) - 0x1000,
        player2.position.x + (rand() % 0x1FFF) - 0x1000,
        player2.position.y + (rand() % 0x1FFF) - 0x1000
      );
    }
  }
}

static int run_game()
{
  player1 =(Player) {
    .position = { -rand(), 2 * rand() },
    .angle = 64,
    .velocity = { 0, 0 },
    .fire_pressed = false,
  };
  player2 = (Player) {
    .position = { rand(), 2 * rand() },
    .angle = -64,
    .velocity = { 0, 0 },
    .fire_pressed = false,
  };
  for(size_t i = 0; i < NUM_SHOTS; i++)
  {
    shots[i].alive = 0;
  }

	while(1)
	{
    paint_playfield();

    if(collider_test(player1.position.x, player1.position.y, 8*256, player2.position.x, player2.position.y, 8*256))
    {
      explosions(3);
      return 0;
    }

    for(size_t i = 0; i < NUM_SHOTS; i++)
    {
      if(shots[i].alive)
      {
        shots[i].position.x += shots[i].velocity.x;
        shots[i].position.y += shots[i].velocity.y;

        Player const * target;
        if(shots[i].owner == &player1)
          target = &player2;
        else
          target = &player1;

        shots[i].alive -= 1;
        
        if(collider_test(shots[i].position.x, shots[i].position.y, 256, target->position.x, target->position.y, 11 * 256))
        {
          if(target == &player1) {
            explosions(1);
            return 1;
          }
          if(target == &player2){
            explosions(2);
            return 2;
          }
        }
      }
    }

    update_player(&player1, (uint8_t[]) {
      INPUT1_LEFT,
      INPUT1_RIGHT,
      INPUT1_FIRE,
      INPUT1_ACCEL
    });

    update_player(&player2, (uint8_t[]) {
      INPUT2_LEFT,
      INPUT2_RIGHT,
      INPUT2_FIRE,
      INPUT2_ACCEL
    });

	}
} 

int main()
{
  DDRA = 0x01; 
	DDRB = 0xFF;
	DDRC = 0x00;
  DDRD = 0xFF;
  PORTC = 0xFF; // all input = pullup

  refresh_score(&p1Score);
  refresh_score(&p2Score);

  while(1)
  {
    switch(run_game())
    {
      case 1: 
        p1Score.value += 1;
        refresh_score(&p1Score);
        break;
      case 2: 
        p2Score.value += 1;
        refresh_score(&p2Score);
        break;
    }
  }

	return 0;
}
