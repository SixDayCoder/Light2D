#include "svpng.inc"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define WIDTH        (512)
#define HEIGHT       (512)
#define RGB	         (3)
#define TWO_PI       (6.28318530718f)
#define SAMPLE_COUNT (64)
#define MAX_STEP     (10)
#define MAX_DISTANCE (2.0f)
#define EPSILON      (1e-6f)

#define CIRCLE_CENTRE_X      (0.5f)
#define CIRCLE_CENTRE_Y      (0.5f)
#define CIRCLE_CENTRE_RADIUS (0.1f)

#define LIGHT_NORMAL_STRENGTH (2.0f) //

typedef unsigned char byte;




byte image[WIDTH * HEIGHT * RGB];


float Sample(float x, float y);
float CircleSDF(float x, float y, float cx, float cy, float radius);
float Trace(float ox, float oy, float dx, float dy);

int main()
{
	byte* p = image;

	for (int y = 0; y < HEIGHT; ++y)
	{
		for (int x = 0; x < WIDTH; ++x)
		{
			p[0] = p[1] = p[2] = (int)255.0f;
			p += RGB;
		}
	}

	FILE* fp = fopen("..//..//png//basic.png", "wb");
	svpng(fp, WIDTH, HEIGHT, image, 0);
	printf("Svnpng Success\n");
	return 0;
}

float Sample(float x, float y)
{
	float sum = 0.0f;
	for (int i = 0; i < SAMPLE_COUNT; ++i)
	{
		float radians = TWO_PI * rand() / RAND_MAX;
		sum += Trace(x, y, cosf(radians), sinf(radians));
	}
	return sum / SAMPLE_COUNT;
}

float CircleSDF(float x, float y, float cx, float cy, float radius)
{
	float dx = x - cx;
	float dy = y - cy;
	return sqrtf(dx * dx + dy * dy) - radius;
}

float Trace(float ox, float oy, float dx, float dy)
{
	float delta = 0.0f;

	for (int i = 0; i < MAX_STEP && delta < MAX_DISTANCE; ++i)
	{
		float step = CircleSDF(ox + dx * delta, oy + dy * delta, CIRCLE_CENTRE_X, CIRCLE_CENTRE_Y, CIRCLE_CENTRE_RADIUS);
		if (step < EPSILON)
		{
			return  LIGHT_NORMAL_STRENGTH;
		}

		delta += step;
	}

	return 0.0f;
}
