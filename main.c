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
} Player;

typedef struct {
  uint16_t alive;
  Vector2 position;
  Vector2 velocity;
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
  PORTC = 128 + y / 256;
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

  for(int i = 0; i < 16; i++)
  {
    int32_t x = i * (int32_t)(x2 - x1) / 16 + x1;
    int32_t y = i * (int32_t)(y2 - y1) / 16 + y1;

    if(x > SHRT_MAX || y > SHRT_MAX)
      continue;

    if(x < SHRT_MIN || y < SHRT_MIN)
      continue;

    move_cursor(x, y);

    _delay_us(1);
  }

  move_cursor(x2, y2);
  set_beam(false);
}

void paint_linept(Vector2 from, Vector2 to)
{
  paint_line(from.x, from.y, to.x, to.y);
}

void paint_player(Player * player)
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

#define INPUT_LEFT  (1<<2)
#define INPUT_RIGHT (1<<0)
#define INPUT_ACCEL (1<<1)
#define INPUT_FIRE  (1<<3)

bool is_input_pressed(uint8_t mask)
{
  return (PIND & mask) == 0;
}

Vector2 direction_for_angle(uint8_t angle)
{
  int16_t dx = sine_lut[angle];
  int16_t dy = sine_lut[(angle + 64) & 0xFF];
  return (Vector2) { dx, -dy };
}

int main()
{
  DDRA = 0x01;
	DDRB = 0xFF;
	DDRC = 0xFF;
  DDRD = 0x00;
  PORTD = 0x0F; // 4 bit pullup

  Player player1 = {
    .position = { 0, 0 },
    .angle = 0,
    .velocity = { 0, 0 }
  };

  bool fire_pressed = false;
	while(1)
	{
    paint_player(&player1);

    for(size_t i = 0; i < NUM_SHOTS; i++)
    {
      if(shots[i].alive)
      {
        shots[i].position.x += shots[i].velocity.x;
        shots[i].position.y += shots[i].velocity.y;

        paint_point(shots[i].position.x, shots[i].position.y);

        shots[i].alive -= 1;
      }
    }

    if(is_input_pressed(INPUT_ACCEL)) {
      Vector2 delta = direction_for_angle(player1.angle);

      const int dx = delta.x / 32;
      const int dy = delta.y / 32;

      if(abs(player1.velocity.x + dx) < 512)
        player1.velocity.x += dx;
      if(abs(player1.velocity.y + dy) < 512)
        player1.velocity.y += dy;

    } else {
      if(player1.velocity.x > 0)
        player1.velocity.x -= 1;
      else if(player1.velocity.x < 0)
        player1.velocity.x += 1;
      if(player1.velocity.y > 0)
        player1.velocity.y -= 1;
      else if(player1.velocity.y < 0)
        player1.velocity.y += 1;
    }

    if(is_input_pressed(INPUT_LEFT)) {
      player1.angle -= 1;
    }

    if(is_input_pressed(INPUT_RIGHT)) {
      player1.angle += 1;
    }

    if(is_input_pressed(INPUT_FIRE)) {
      if(!fire_pressed) {
        Shot * shot = alloc_shot();
        if(shot != NULL) {
          shot->position = player1.position;
          shot->velocity = direction_for_angle(player1.angle);
          shot->velocity.x *= 2;
          shot->velocity.y *= 2;
          shot->alive = 256; // 1000 frames alive
        }
      }
      fire_pressed = true;
    } else {
      fire_pressed = false;
    }

    player1.position.x += player1.velocity.x;
    player1.position.y += player1.velocity.y;
	}

	return 0;
}






int8_t const sine_lut[256] = 
{
  0,
  3,
  6,
  9,
  12,
  16,
  19,
  22,
  25,
  28,
  31,
  34,
  37,
  40,
  43,
  46,
  49,
  51,
  54,
  57,
  60,
  63,
  65,
  68,
  71,
  73,
  76,
  78,
  81,
  83,
  85,
  88,
  90,
  92,
  94,
  96,
  98,
  100,
  102,
  104,
  106,
  107,
  109,
  111,
  112,
  113,
  115,
  116,
  117,
  118,
  120,
  121,
  122,
  122,
  123,
  124,
  125,
  125,
  126,
  126,
  126,
  127,
  127,
  127,
  127,
  127,
  127,
  127,
  126,
  126,
  126,
  125,
  125,
  124,
  123,
  122,
  122,
  121,
  120,
  118,
  117,
  116,
  115,
  113,
  112,
  111,
  109,
  107,
  106,
  104,
  102,
  100,
  98,
  96,
  94,
  92,
  90,
  88,
  85,
  83,
  81,
  78,
  76,
  73,
  71,
  68,
  65,
  63,
  60,
  57,
  54,
  51,
  49,
  46,
  43,
  40,
  37,
  34,
  31,
  28,
  25,
  22,
  19,
  16,
  12,
  9,
  6,
  3,
  0,
  -3,
  -6,
  -9,
  -12,
  -16,
  -19,
  -22,
  -25,
  -28,
  -31,
  -34,
  -37,
  -40,
  -43,
  -46,
  -49,
  -51,
  -54,
  -57,
  -60,
  -63,
  -65,
  -68,
  -71,
  -73,
  -76,
  -78,
  -81,
  -83,
  -85,
  -88,
  -90,
  -92,
  -94,
  -96,
  -98,
  -100,
  -102,
  -104,
  -106,
  -107,
  -109,
  -111,
  -112,
  -113,
  -115,
  -116,
  -117,
  -118,
  -120,
  -121,
  -122,
  -122,
  -123,
  -124,
  -125,
  -125,
  -126,
  -126,
  -126,
  -127,
  -127,
  -127,
  -127,
  -127,
  -127,
  -127,
  -126,
  -126,
  -126,
  -125,
  -125,
  -124,
  -123,
  -122,
  -122,
  -121,
  -120,
  -118,
  -117,
  -116,
  -115,
  -113,
  -112,
  -111,
  -109,
  -107,
  -106,
  -104,
  -102,
  -100,
  -98,
  -96,
  -94,
  -92,
  -90,
  -88,
  -85,
  -83,
  -81,
  -78,
  -76,
  -73,
  -71,
  -68,
  -65,
  -63,
  -60,
  -57,
  -54,
  -51,
  -49,
  -46,
  -43,
  -40,
  -37,
  -34,
  -31,
  -28,
  -25,
  -22,
  -19,
  -16,
  -12,
  -9,
  -6,
  -3,
};