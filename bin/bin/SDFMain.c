#include "svpng.inc"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define EPSILON                   (1e-6f)
#define WIDTH                     (512)
#define HEIGHT                    (512)
#define RGB	                      (3)
#define TWO_PI                    (6.28318530718f)
#define LIGHT_COUNT               (64)


#define RAY_MARCHING_MAX_STEP     (10)
#define RAY_MARCHING_MAX_DISTANCE (2.0f)

#define COLOR_WHITE (0.0f)
#define COLOR_BLACK (255.0f)

typedef unsigned char byte;
typedef struct { float sdf, emissive; } TraceResult;

byte image[WIDTH * HEIGHT * RGB];

TraceResult Scene(float x, float y);

TraceResult UnionResult(TraceResult lhs, TraceResult rhs);

TraceResult IntersecResult(TraceResult lhs, TraceResult rhs);

TraceResult SubtractResult(TraceResult lhs, TraceResult rhs);

float Lighting(float x, float y);

float CircleSDF(float x, float y, float cx, float cy, float radius);

float PlaneSDF(float x, float y, float px, float py, float nx, float ny);

float SegmentSDF(float x, float y, float ax, float ay, float bx, float by);

float CapsuleSDF(float x, float y, float ax, float ay, float bx, float by, float radius);

float BoxSDF(float x, float y, float ox, float oy, float theta, float sx, float sy);

float TriangleSDF(float x, float y, float ax, float ay, float bx, float by, float cx, float cy);

float Trace(float ox, float oy, float dx, float dy);


int main()
{
	byte* p = image;

	for (int y = 0; y < HEIGHT; ++y)
	{
		for (int x = 0; x < WIDTH; ++x)
		{
			float color = Lighting((float)x / WIDTH, (float)y / HEIGHT) * COLOR_BLACK;
			p[0] = p[1] = p[2] = (int)(fminf(color, COLOR_BLACK));
			p += RGB;
		}
	}

	FILE* fp = fopen("..//..//png//basic_rounded_triangle.png", "wb");
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

float PlaneSDF(float x, float y, float px, float py, float nx, float ny)
{
	return (x - px) * nx + (y - py) * ny;
}

float SegmentSDF(float x, float y, float ax, float ay, float bx, float by)
{
	float vx = x - ax,  vy = y - ay;
	float ux = bx - ax, uy = by - ay;
	float dot = vx * ux + vy * uy;
	float t = fmaxf(fminf( dot / (ux * ux + uy * uy), 1.0f), 0.0f);
	float dx = vx - ux * t, dy = vy - uy * t;

	return sqrtf(dx * dx + dy * dy);
}

float CapsuleSDF(float x, float y, float ax, float ay, float bx, float by, float radius)
{
	return SegmentSDF(x, y, ax, ay, bx, by) - radius;
}

float BoxSDF(float x, float y, float ox, float oy, float theta, float sx, float sy)
{
	float costheta = cosf(theta);
	float sintheta = sinf(theta);

	//坐标变换,变换到Box的局部坐标系下 ： 旋转+平移
	float dx = fabsf((x - ox) * costheta + (y - oy) * sintheta) - sx;
	float dy = fabsf((y - oy) * costheta - (x - ox) * sintheta) - sy;

	float ax = fmaxf(dx, 0.0f);
	float ay = fmaxf(dy, 0.0f);

	return fminf(fmaxf(dx, dy), 0.0f) + sqrtf(ax * ax + ay * ay);
}

float TriangleSDF(float x, float y, float ax, float ay, float bx, float by, float cx, float cy)
{
	float d = fminf(fminf( SegmentSDF(x, y, ax, ay, bx, by), SegmentSDF(x, y, bx, by, cx, cy)), 
		                   SegmentSDF(x, y, cx, cy, ax, ay));

	return  (bx - ax) * (y - ay) > (by - ay) * (x - ax) &&
			(cx - bx) * (y - by) > (cy - by) * (x - bx) &&
			(ax - cx) * (y - cy) > (ay - cy) * (x - cx) ? -d : d;
}

float Trace(float ox, float oy, float dx, float dy)
{
	float t = 0.0f;

	for (int i = 0; i < RAY_MARCHING_MAX_STEP && t < RAY_MARCHING_MAX_DISTANCE; ++i)
	{
		TraceResult r = Scene(ox + dx * t, oy + dy * t);
		if (r.sdf < EPSILON)
		{
			return  r.emissive;
		}
		t += r.sdf;
	}

	return 0.0f;
}

TraceResult Scene(float x, float y)
{
	//TraceResult r = { PlaneSDF(x, y, 0.5f, 0.5f, 0.0f, 1.0f), 0.8f };
	//TraceResult r = { CapsuleSDF(x, y, 0.4f, 0.4f, 0.6f, 0.6f, 0.1f), 1.0f };
	//TraceResult r = { BoxSDF(x, y, 0.5f, 0.5f, TWO_PI / 16.0f, 0.3f, 0.1f) - 0.1f , 1.0f };
	TraceResult r = { TriangleSDF(x, y, 0.5f, 0.2f, 0.8f, 0.8f, 0.3f, 0.6f) - 0.1f, 1.0f };

	return r;
}

TraceResult UnionResult(TraceResult lhs, TraceResult rhs)
{
	return lhs.sdf < rhs.sdf ? lhs : rhs;
}

TraceResult IntersecResult(TraceResult lhs, TraceResult rhs)
{
	TraceResult r;
	float emissive = lhs.sdf > rhs.sdf ? lhs.emissive : rhs.emissive;
	float sdf = lhs.sdf > rhs.sdf ? lhs.sdf : rhs.sdf;

	r.emissive = emissive;
	r.sdf = sdf;
	return r;
}

TraceResult SubtractResult(TraceResult lhs, TraceResult rhs)
{
	TraceResult r = lhs;
	r.sdf = lhs.sdf > -rhs.sdf ? lhs.sdf : -rhs.sdf;
	return r;
}


//DOC

//1.计算Plane的SDF
//这里的Plane只的是一个二维的超平面,在二维空间的表现是一条直线,将二维空间划分为两部分
//点X(x,y)到Plane的SDF定义为
//SDF(x) = Dot( (X-P), Normal ) P是Plane上一点,Normal是Plane的单位法向量  => 直线的点法式

//2.计算Capsule的SDF
//首先我们可以用一组数据a,b,r来描述一个胶囊体
//a,b是胶囊体的body部分的两端,r是胶囊体圆弧的半径
//点X(x,y)到Capsule的SDF定义为
//SDF(X) = Dist(x, segment(a,b)) - radius
//关键是计算X到线段ab的距离

//3.计算矩形的SDF
//首先我们用一组数据o,theta,s来表示
//o是矩形原点
//theta是旋转角(正向旋转)
//s是矩形长对角线的一半 由s可知长宽分别为s * cos(theta)和s * sin(theta)
//首先是将世界坐标系的点p变换到矩形Box所在的坐标系
//显然可以由一次旋转+位移的操作来完成

//4.计算三角形的SDF
//三角形的SDF分为两步
//1.点P到三条线段的最短距离,可以用SegmentSDF判断
//2.P是在三角形的内部还是外部,如果是在内部那么返回-d否则返回d
//重点是判断在三角形的内部还是外部
