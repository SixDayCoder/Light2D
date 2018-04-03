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


#define RAY_MARCHING_MAX_STEP     (64)
#define RAY_MARCHING_MAX_DISTANCE (5.0f)
#define RAY_MAX_TRACE_STEP    (3)
#define RAY_BIAS (1e-4f)

#define COLOR_WHITE (0.0f)
#define COLOR_BLACK (255.0f)

typedef unsigned char byte;
typedef struct { float sdf, emissive, reflectivity; } TraceResult;

byte image[WIDTH * HEIGHT * RGB];

TraceResult Scene(float x, float y);

TraceResult Union(TraceResult lhs, TraceResult rhs);

TraceResult IntersecResult(TraceResult lhs, TraceResult rhs);

TraceResult SubtractResult(TraceResult lhs, TraceResult rhs);

float Lighting(float x, float y);

float CircleSDF(float x, float y, float cx, float cy, float radius);

float PlaneSDF(float x, float y, float px, float py, float nx, float ny);

float SegmentSDF(float x, float y, float ax, float ay, float bx, float by);

float CapsuleSDF(float x, float y, float ax, float ay, float bx, float by, float radius);

float BoxSDF(float x, float y, float ox, float oy, float theta, float sx, float sy);

float TriangleSDF(float x, float y, float ax, float ay, float bx, float by, float cx, float cy);

float Trace(float ox, float oy, float dx, float dy, int depth);

void Reflect(float ix, float iy, float nx, float ny, float* rx, float* ry);

void Gradient(float x, float y, float* nx, float* ny);

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

	FILE* fp = fopen("..//..//png//basic_reflect.png", "wb");
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
		sum += Trace(x, y, cosf(radians), sinf(radians), 0);
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
	float vx = x - ax, vy = y - ay;
	float ux = bx - ax, uy = by - ay;
	float dot = vx * ux + vy * uy;
	float t = fmaxf(fminf(dot / (ux * ux + uy * uy), 1.0f), 0.0f);
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
	float d = fminf(fminf(SegmentSDF(x, y, ax, ay, bx, by), SegmentSDF(x, y, bx, by, cx, cy)),
		            SegmentSDF(x, y, cx, cy, ax, ay));

	return  (bx - ax) * (y - ay) > (by - ay) * (x - ax) &&
		    (cx - bx) * (y - by) > (cy - by) * (x - bx) &&
		    (ax - cx) * (y - cy) > (ay - cy) * (x - cx) ? -d : d;
}

float Trace(float ox, float oy, float dx, float dy, int depth)
{
	float t = 0.0f;

	for (int i = 0; i < RAY_MARCHING_MAX_STEP && t < RAY_MARCHING_MAX_DISTANCE; ++i)
	{
		float x = ox + dx * t;
		float y = oy + dy * t;
		TraceResult r = Scene(x, y);
		if (r.sdf < EPSILON)
		{
			float sum = r.emissive;
			//SDF该点是可反射的,并且Trace的递归次数在要求的范围内
			if (depth < RAY_MAX_TRACE_STEP && r.reflectivity > 0.0f)
			{
				float nx, ny, rx, ry;
				Gradient(x, y, &nx, &ny);//计算法线
				Reflect(dx, dy, nx, ny, &rx, &ry);//计算反射向量
				sum += r.reflectivity * Trace(x + nx * RAY_BIAS, y + ny * RAY_BIAS, rx, ry, depth + 1);
			}
			return sum;
		}
		t += r.sdf;
	}

	return 0.0f;
}

void Reflect(float ix, float iy, float nx, float ny, float * rx, float * ry)
{
	float idotn2 = (ix * nx + iy * ny) * 2.0f;
	*rx = ix - idotn2 * nx;
	*ry = iy - idotn2 * ny;
}

void Gradient(float x, float y, float * nx, float * ny)
{
	//梯度是偏微分,这里使用近似值,就是在x和y方向上分别步进delta(这里取得的是Epsilon),然后做微分
	*nx = (Scene( x + EPSILON, y).sdf - Scene(x - EPSILON, y).sdf ) * (0.5f / EPSILON);
	*ny = (Scene( x, y + EPSILON).sdf - Scene(x, y - EPSILON).sdf ) * (0.5f / EPSILON);
}

TraceResult Scene(float x, float y)
{
	TraceResult a = { CircleSDF(x, y, 0.4f,  0.2f, 0.1f), 2.0f, 0.0f };
	TraceResult b = { BoxSDF(x, y, 0.5f,  0.8f, TWO_PI / 16.0f, 0.1f, 0.1f), 0.0f, 0.9f };
	TraceResult c = { BoxSDF(x, y, 0.8f,  0.5f, TWO_PI / 16.0f, 0.1f, 0.1f), 0.0f, 0.9f };
	return Union(Union(a, b), c);

}

TraceResult Union(TraceResult lhs, TraceResult rhs)
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
//重点:如何通过SDF获取边界法线
//结论:SDF变化最大的方向是法线方向
//简易推导:
//(1)一个纯量场(全部都是标量,体现在这里就是SDF),它变化最大的方向是它的梯度
//(2)SDF的梯度是单位矢量
//使用微分法计算出某点的梯度=>  
//nx = SDF(x + delta) - SDF(x - delta) / 2delta => SDF对x的偏导
//ny = SDF(y + delta) - SDF(y - delta) / 2delta => SDF对y的偏导

//如何追踪反射的光线
//trace(x,y)到SDF的边界的时候,计算该边界的gradient,然后这个gradient就是边界法向量,由此可以计算出反射向量
//再对反射向量的方向进行trace


//
//另外，由于我们要向反射方向追踪，
//如果用原来的相交点(x,y)，追踪时就为立即遇到SDF的边界而停止，
//所以我们稍微把相交点往法线方向偏移RAY_BIAS的距离，只要RAY_BIAS > RAY_MARCHING_EPSILON
//就可以避免这个问题。但太大的话也会造成误差。