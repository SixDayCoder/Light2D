#include "svpng.inc"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define WIDTH                     (512)
#define HEIGHT                    (512)
#define RGB	                      (3)
#define TWO_PI                    (6.28318530718f)
#define LIGHT_COUNT               (64)
#define RAY_MARCHING_MAX_STEP     (10)
#define RAY_MARCHING_MAX_DISTANCE (2.0f)
#define EPSILON                   (1e-6f)

#define CIRCLE_CENTRE_X      (0.75f)
#define CIRCLE_CENTRE_Y      (0.5f)
#define CIRCLE_RADIUS        (0.2f)
#define LIGHT_STRENGTH       (2.0f) 

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

	FILE* fp = fopen("..//..//png//test.png", "wb");
	svpng(fp, WIDTH, HEIGHT, image, 0);
	printf("Svnpng Success\n");
	return 0;
}

float Lighting(float x, float y)
{
	float sum = 0.0f;
	for (int i = 0; i < LIGHT_COUNT; ++i)
	{
		float radians = TWO_PI * (i + (float)rand() / RAND_MAX) / LIGHT_COUNT;   // 抖动采样
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

//计算点(ox,oy)在(dx,dy)方向上接受的光照度
float Trace(float ox, float oy, float dx, float dy)
{
	float t = 0.0f;

	for (int i = 0; i < RAY_MARCHING_MAX_STEP && t < RAY_MARCHING_MAX_DISTANCE; ++i)
	{
		float sdf = CircleSDF(ox + dx * t, oy + dy * t, CIRCLE_CENTRE_X, CIRCLE_CENTRE_Y, CIRCLE_RADIUS);
		if (sdf < EPSILON)
		{
			return  LIGHT_STRENGTH;
		}
		t += sdf;
	}

	return 0.0f;
}


//DOC
//在计算光照时
//无论图像长宽是多少,通过除以宽度和高度，把x,y=>映射到(0,1) X (0, 1)，然后把得到的结果乘255.0f映射到{0,1,...,255}
//相当于做了一次坐标变换,变换到光照空间
//记录发光圆盘在(0.5,0.5),发光半径为0.1,这其实是用光照空间描述的
//好处在于,在光照空间计算Trace时,RAY_MARCHING_MAX_DISTANCE可以设置一个不大的数(大于1就好)来满足步进条件的同时,又能得到比较快的收敛速度
//当然,你也可以把这个计算放在PNG的坐标系下,此时给出的发光圆盘的位置应该就是在PNG的坐标下