#include "svpng.inc"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define WIDTH        (512)
#define HEIGHT       (512)
#define RGB	         (3)
#define TWO_PI       (6.28318530718f)
#define LIGHT_COUNT (64)
#define MAX_STEP     (10)
#define MAX_DISTANCE (2.0f)
#define EPSILON      (1e-6f)

#define CIRCLE_CENTRE_X      (0.5f)
#define CIRCLE_CENTRE_Y      (0.5f)
#define CIRCLE_CENTRE_RADIUS (0.1f)

#define LIGHT_NORMAL_STRENGTH (2.0f) //

#define COLOR_WHITE (0.0f)
#define COLOR_BLACK (255.0f)

typedef unsigned char byte;
byte image[WIDTH * HEIGHT * RGB];


//测定坐标为(x,y)的点被光照射的颜色值
float Lighting(float x, float y);


//任意点(x,y)距离发光圆盘Circle(cx,cy,radius)的符号距离场
//1.SDF < 0 点在场景形状内,距离最近的形状边界距离为SDF
//2.SDF = 0 点在场景形状内
//3.SDF > 0 点在场景形状外
float CircleSDF(float x, float y, float cx, float cy, float radius); 


//简单光线追踪
float Trace(float ox, float oy, float dx, float dy);

int main()
{
	byte* p = image;

	for (int y = 0; y < HEIGHT; ++y)
	{
		for (int x = 0; x < WIDTH; ++x)
		{
			float color = Lighting( (float)x / WIDTH, (float)y / HEIGHT) * COLOR_BLACK;
			p[0] = p[1] = p[2] = (int)(fminf(color, COLOR_BLACK));
			p += RGB;
		}
	}

	FILE* fp = fopen("..//..//png//basic_light_stratified.png", "wb");
	svpng(fp, WIDTH, HEIGHT, image, 0);
	printf("Svnpng Success\n");
	return 0;
}

float Lighting(float x, float y)
{
	float sum = 0.0f;
	for (int i = 0; i < LIGHT_COUNT; ++i)
	{
		float radians = TWO_PI * i / LIGHT_COUNT;
		sum += Trace(x, y, cosf(radians), sinf(radians));
	}
	return sum / LIGHT_COUNT;
}

float CircleSDF(float x, float y, float cx, float cy, float radius)
{
	float dx = x - cx;
	float dy = y - cy;
	return sqrtf(dx * dx + dy * dy) - radius;
}

float Trace(float ox, float oy, float dx, float dy)
{
	float t = 0.0f;

	for (int i = 0; i < MAX_STEP && t < MAX_DISTANCE; ++i)
	{
		float sdf = CircleSDF(ox + dx * t, oy + dy * t, CIRCLE_CENTRE_X, CIRCLE_CENTRE_Y, CIRCLE_CENTRE_RADIUS);
		if (sdf < EPSILON)
		{
			return  LIGHT_NORMAL_STRENGTH;
		}

		t += sdf;
	}

	return 0.0f;
}
