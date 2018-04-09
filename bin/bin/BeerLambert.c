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

#define REFRACT (1)  //折射
#define TOTAL_REFLECT (0) //全反射

typedef unsigned char byte;
typedef struct { float sdf, emissive, reflectivity, eta, absorption; } TraceResult;

byte image[WIDTH * HEIGHT * RGB];

TraceResult Scene(float x, float y);

TraceResult Union(TraceResult lhs, TraceResult rhs);

TraceResult Intersec(TraceResult lhs, TraceResult rhs);

TraceResult Subtract(TraceResult lhs, TraceResult rhs);

float Lighting(float x, float y);

float CircleSDF(float x, float y, float cx, float cy, float radius);

float PlaneSDF(float x, float y, float px, float py, float nx, float ny);

float SegmentSDF(float x, float y, float ax, float ay, float bx, float by);

float CapsuleSDF(float x, float y, float ax, float ay, float bx, float by, float radius);

float BoxSDF(float x, float y, float ox, float oy, float theta, float sx, float sy);

float TriangleSDF(float x, float y, float ax, float ay, float bx, float by, float cx, float cy);

float Trace(float ox, float oy, float dx, float dy, int depth);

void Reflect(float ix, float iy, float nx, float ny, float* rx, float* ry);

int Refract(float ix, float iy, float nx, float ny, float eta, float *rx, float *ry);

void Gradient(float x, float y, float* nx, float* ny);

float Fresnel(float cosi, float cost, float etai, float etat);//菲涅尔反射方程,计算反射比

float BeerLambert(float a, float d);

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

	FILE* fp = fopen("..//..//png//basic_beer_lambert.png", "wb");
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
	float t = 1e-3f;
	float sign = Scene(ox, oy).sdf > 0.0f ? 1.0f : -1.0f;

	for (int i = 0; i < RAY_MARCHING_MAX_STEP && t < RAY_MARCHING_MAX_DISTANCE; ++i)
	{
		float x = ox + dx * t;
		float y = oy + dy * t;
		TraceResult r = Scene(x, y);
		if (r.sdf * sign  < EPSILON) //因为现在是光线在内外部均有可能,所以在光线步进的时候要考虑符号
		{
			float sum = r.emissive;
			//SDF该点是可反射或者可折射的,并且Trace的递归次数在要求的范围内
			if (depth < RAY_MAX_TRACE_STEP && ((r.reflectivity > 0.0f) || (r.eta > 0.0f)))
			{
				float reflect = r.reflectivity;
				float nx, ny, rx, ry;
				Gradient(x, y, &nx, &ny);//计算法线
										 //如果光线在形状内部我们还要翻转法线
				nx *= sign;
				ny *= sign;
				//追踪折射光线
				if (r.eta > 0.0f)
				{
					float eta = sign < 0.0f ? r.eta : 1.0f / r.eta;
					//在(dx,dy)方向发生了折射
					if (REFRACT == Refract(dx, dy, nx, ny, eta, &rx, &ry))
					{
						//如果发生了折射,使用菲涅尔方程计算真正的反射率,1.0f-reflect就是真正的折射率
						//这里的1.0是表示空气的介质折射率
						//这里的cos为什么取负数要画一下图看看,入射角是锐角,但是我们这个入射方向和法线的直接dot是负角
						float cosi = -(dx * nx + dy * ny);
						float cost = -(rx * nx + ry * ny);
						reflect = sign < 0.0f ? Fresnel(cosi, cost, r.eta, 1.0f) : Fresnel(cosi, cost, 1.0f, r.eta);
						sum += (1.0f - reflect) * Trace(x +  nx * RAY_BIAS, y + ny * RAY_BIAS, rx, ry, depth + 1);
					}
					else
					{
						//否则是全反射,反射率拉满
						reflect = 1.0f;
					}
				}
				//追踪反射光线
				if (r.reflectivity > 0.0f)
				{
					Reflect(dx, dy, nx, ny, &rx, &ry);
					//这里的reflect考虑到了菲涅尔反射
					sum += reflect * Trace(x + nx * RAY_BIAS, y + ny * RAY_BIAS, rx, ry, depth + 1);
				}
			}
			return sum * BeerLambert(r.absorption, t);
		}

		//光线步进考虑光线在形状内还是形状外
		t += r.sdf * sign;
	}

	return 0.0f;
}

void Reflect(float ix, float iy, float nx, float ny, float * rx, float * ry)
{
	float idotn2 = (ix * nx + iy * ny) * 2.0f;
	*rx = ix - idotn2 * nx;
	*ry = iy - idotn2 * ny;
}

int Refract(float ix, float iy, float nx, float ny, float eta, float * rx, float * ry)
{
	//(nx,ny)是单位向量,(rx, ry)是单位向量
	float idotn = ix * nx + iy * ny;
	float k = 1.0f - eta * eta * (1.0f - idotn * idotn);
	if (k < 0.0f)
	{
		return TOTAL_REFLECT;//全反射
	}

	float a = eta * idotn + sqrtf(k);
	*rx = eta * ix - a * nx;
	*ry = eta * iy - a * ny;
	return REFRACT;//折射
}

void Gradient(float x, float y, float * nx, float * ny)
{
	//梯度是偏微分,这里使用近似值,就是在x和y方向上分别步进delta(这里取得的是Epsilon),然后做微分
	*nx = (Scene(x + EPSILON, y).sdf - Scene(x - EPSILON, y).sdf) * (0.5f / EPSILON);
	*ny = (Scene(x, y + EPSILON).sdf - Scene(x, y - EPSILON).sdf) * (0.5f / EPSILON);
}

float Fresnel(float cosi, float cost, float etai, float etat)
{
	float rs = (etat * cosi - etai * cost) / (etat * cosi + etai * cost);
	float rp = (etai * cosi - etat * cost) / (etai * cosi + etat * cost);
	//图形学是考虑光无偏振,所以取入射光的s偏振和p偏振的均值
	return (rs * rs + rp * rp) * 0.5f;
}

float BeerLambert(float a, float d)
{
	return expf(-a * d);
}

TraceResult Scene(float x, float y)
{
	TraceResult a = { CircleSDF(x, y, -0.2f, -0.2f, 0.1f), 10.0f, 0.0f, 0.0f, 0.0f };
	TraceResult b = { BoxSDF(x, y, 0.5f, 0.5f, 0.0f, 0.3f, 0.2f), 0.0f, 0.2f, 1.5f, 4.0f };

	return Union(a, b);
}

TraceResult Union(TraceResult lhs, TraceResult rhs)
{
	return lhs.sdf < rhs.sdf ? lhs : rhs;
}

TraceResult Intersec(TraceResult lhs, TraceResult rhs)
{
	TraceResult r;
	float emissive = lhs.sdf > rhs.sdf ? lhs.emissive : rhs.emissive;
	float sdf = lhs.sdf > rhs.sdf ? lhs.sdf : rhs.sdf;

	r.emissive = emissive;
	r.sdf = sdf;
	return r;
}

TraceResult Subtract(TraceResult lhs, TraceResult rhs)
{
	TraceResult r = lhs;
	r.sdf = lhs.sdf > -rhs.sdf ? lhs.sdf : -rhs.sdf;
	return r;
}
//
//
////DOC:
////比尔兰伯特定律：
//比尔-朗伯定律（Beer-Lambert law）描述电磁波（如可见光）通过物体时，物体吸收部分电磁波，
///而吸收率与物体的厚度（光程距离）、物质的吸光系数及其浓度相关。

///一束单色光照射于一吸收介质表面，在通过一定厚度的介质后，由于介质吸收了一部分光能，透射光的强度就要减弱。
///吸收介质的浓度愈大，介质的厚度愈大，则光强度的减弱愈显著

/// T = expf(-a * c * d)
/// T : 透过某个物体表面的光的比率
/// a : 物质的吸光系数,查表
/// c : 物质的浓度,查表
/// d : 光程距离,光在介质中的传播的距离 d = eta * distance => 空气中eta是1
/// 均质物体的ac是常数
