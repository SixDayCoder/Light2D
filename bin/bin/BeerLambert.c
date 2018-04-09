#include "svpng.inc"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define EPSILON                   (1e-6f)
#define WIDTH                     (512)
#define HEIGHT                    (512)
#define RGB	                      (3)
#define TWO_PI                    (6.28318530718f)
#define LIGHT_COUNT               (256)


#define RAY_MARCHING_MAX_STEP     (64)
#define RAY_MARCHING_MAX_DISTANCE (5.0f)
#define RAY_MAX_TRACE_STEP    (5)
#define RAY_BIAS (1e-4f)

#define COLOR_WHITE (0.0f)

#define REFRACT (1)  //����
#define TOTAL_REFLECT (0) //ȫ����

#define COLOR_BLACK {0.0f, 0.0f, 0.0f}

typedef unsigned char byte;
typedef struct { float r, g, b; } Color;
typedef struct 
{ 
	float sdf, reflectivity, eta;
	Color emissive, absorption;
}  TraceResult;


Color ColorAdd(Color lhs, Color rhs)
{
	Color c = { lhs.r + rhs.r, lhs.g + rhs.g, lhs.b + rhs.b };
	return c;
}

Color ColorMultiply(Color lhs, Color rhs)
{
	Color c = { lhs.r * rhs.r, lhs.g * rhs.g, lhs.b * rhs.b };
	return c;
}

Color ColorScale(Color c, float scale)
{
	c.r *= scale;
	c.g *= scale;
	c.b *= scale;

	return c;
}

byte image[WIDTH * HEIGHT * RGB];

TraceResult Scene(float x, float y);

TraceResult Union(TraceResult lhs, TraceResult rhs);

TraceResult Intersec(TraceResult lhs, TraceResult rhs);

TraceResult Subtract(TraceResult lhs, TraceResult rhs);

Color Sample(float x, float y);

float CircleSDF(float x, float y, float cx, float cy, float radius);

float PlaneSDF(float x, float y, float px, float py, float nx, float ny);

float SegmentSDF(float x, float y, float ax, float ay, float bx, float by);

float CapsuleSDF(float x, float y, float ax, float ay, float bx, float by, float radius);

float BoxSDF(float x, float y, float ox, float oy, float theta, float sx, float sy);

float TriangleSDF(float x, float y, float ax, float ay, float bx, float by, float cx, float cy);

float NgonSDF(float x, float y, float cx, float cy, float r, float n);

Color Trace(float ox, float oy, float dx, float dy, int depth);

void Reflect(float ix, float iy, float nx, float ny, float* rx, float* ry);

int Refract(float ix, float iy, float nx, float ny, float eta, float *rx, float *ry);

void Gradient(float x, float y, float* nx, float* ny);

float Fresnel(float cosi, float cost, float etai, float etat);//���������䷽��,���㷴���

Color BeerLambert(Color a, float d);

int main()
{
	byte* p = image;

	for (int y = 0; y < HEIGHT; ++y)
	{
		for (int x = 0; x < WIDTH; ++x)
		{
			Color c = Sample((float)x / WIDTH, (float)y / HEIGHT);
			p[0] = (int)(fminf(c.r * 255.0f, 255.0f));
			p[1] = (int)(fminf(c.g * 255.0f, 255.0f));
			p[2] = (int)(fminf(c.b * 255.0f, 255.0f));
			p += RGB;
		}
	}

	FILE* fp = fopen("..//..//png//basic_beer_lambert_color.png", "wb");
	svpng(fp, WIDTH, HEIGHT, image, 0);
	printf("Svnpng Success\n");
	return 0;
}

Color Sample(float x, float y)
{
	Color sum = COLOR_BLACK;
	for (int i = 0; i < LIGHT_COUNT; ++i)
	{
		float radians = TWO_PI * (i + (float)rand() / RAND_MAX) / LIGHT_COUNT;   // ��������
		sum = ColorAdd(sum, Trace(x, y, cosf(radians), sinf(radians), 0));
	}
	return ColorScale(sum, 1.0f / LIGHT_COUNT);
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

	//����任,�任��Box�ľֲ�����ϵ�� �� ��ת+ƽ��
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

float NgonSDF(float x, float y, float cx, float cy, float r, float n)
{
	float ux = x - cx, uy = y - cy, a = TWO_PI / n;
	float t = fmodf(atan2f(uy, ux) + TWO_PI, a), s = sqrtf(ux * ux + uy * uy);
	return PlaneSDF(s * cosf(t), s * sinf(t), r, 0.0f, cosf(a * 0.5f), sinf(a * 0.5f));

}

Color Trace(float ox, float oy, float dx, float dy, int depth)
{
	float t = 1e-3f;
	float sign = Scene(ox, oy).sdf > 0.0f ? 1.0f : -1.0f;

	for (int i = 0; i < RAY_MARCHING_MAX_STEP && t < RAY_MARCHING_MAX_DISTANCE; ++i)
	{
		float x = ox + dx * t;
		float y = oy + dy * t;
		TraceResult r = Scene(x, y);
		if (r.sdf * sign  < EPSILON) //��Ϊ�����ǹ��������ⲿ���п���,�����ڹ��߲�����ʱ��Ҫ���Ƿ���
		{
			Color sum = r.emissive;
			//SDF�õ��ǿɷ�����߿������,����Trace�ĵݹ������Ҫ��ķ�Χ��
			if (depth < RAY_MAX_TRACE_STEP && ((r.reflectivity > 0.0f) || (r.eta > 0.0f)))
			{
				float reflect = r.reflectivity;
				float nx, ny, rx, ry;
				Gradient(x, y, &nx, &ny);//���㷨��
										 //�����������״�ڲ����ǻ�Ҫ��ת����
				nx *= sign;
				ny *= sign;
				//׷���������
				if (r.eta > 0.0f)
				{
					float eta = sign < 0.0f ? r.eta : 1.0f / r.eta;
					//��(dx,dy)������������
					if (REFRACT == Refract(dx, dy, nx, ny, eta, &rx, &ry))
					{
						//�������������,ʹ�÷��������̼��������ķ�����,1.0f-reflect����������������
						//�����1.0�Ǳ�ʾ�����Ľ���������
						//�����cosΪʲôȡ����Ҫ��һ��ͼ����,����������,��������������䷽��ͷ��ߵ�ֱ��dot�Ǹ���
						float cosi = -(dx * nx + dy * ny);
						float cost = -(rx * nx + ry * ny);
						reflect = sign < 0.0f ? Fresnel(cosi, cost, r.eta, 1.0f) : Fresnel(cosi, cost, 1.0f, r.eta);
						Color trace = Trace(x +  nx * RAY_BIAS, y + ny * RAY_BIAS, rx, ry, depth + 1);
						sum = ColorAdd(sum, ColorScale(trace, 1.0f - reflect));
					}
					else
					{
						//������ȫ����,����������
						reflect = 1.0f;
					}
				}
				//׷�ٷ������
				if (r.reflectivity > 0.0f)
				{
					Reflect(dx, dy, nx, ny, &rx, &ry);
					//�����reflect���ǵ��˷���������
					Color trace = Trace(x + nx * RAY_BIAS, y + ny * RAY_BIAS, rx, ry, depth + 1);
					sum = ColorAdd(sum, ColorScale(trace, reflect));
				}
			}
			return ColorMultiply(sum, BeerLambert(r.absorption, t));
		}

		//���߲������ǹ�������״�ڻ�����״��
		t += r.sdf * sign;
	}

	Color black = COLOR_BLACK;
	return black;
}

void Reflect(float ix, float iy, float nx, float ny, float * rx, float * ry)
{
	float idotn2 = (ix * nx + iy * ny) * 2.0f;
	*rx = ix - idotn2 * nx;
	*ry = iy - idotn2 * ny;
}

int Refract(float ix, float iy, float nx, float ny, float eta, float * rx, float * ry)
{
	//(nx,ny)�ǵ�λ����,(rx, ry)�ǵ�λ����
	float idotn = ix * nx + iy * ny;
	float k = 1.0f - eta * eta * (1.0f - idotn * idotn);
	if (k < 0.0f)
	{
		return TOTAL_REFLECT;//ȫ����
	}

	float a = eta * idotn + sqrtf(k);
	*rx = eta * ix - a * nx;
	*ry = eta * iy - a * ny;
	return REFRACT;//����
}

void Gradient(float x, float y, float * nx, float * ny)
{
	//�ݶ���ƫ΢��,����ʹ�ý���ֵ,������x��y�����Ϸֱ𲽽�delta(����ȡ�õ���Epsilon),Ȼ����΢��
	*nx = (Scene(x + EPSILON, y).sdf - Scene(x - EPSILON, y).sdf) * (0.5f / EPSILON);
	*ny = (Scene(x, y + EPSILON).sdf - Scene(x, y - EPSILON).sdf) * (0.5f / EPSILON);
}

float Fresnel(float cosi, float cost, float etai, float etat)
{
	float rs = (etat * cosi - etai * cost) / (etat * cosi + etai * cost);
	float rp = (etai * cosi - etat * cost) / (etai * cosi + etat * cost);
	//ͼ��ѧ�ǿ��ǹ���ƫ��,����ȡ������sƫ���pƫ��ľ�ֵ
	return (rs * rs + rp * rp) * 0.5f;
}

Color BeerLambert(Color a, float d)
{
	Color c = { expf(-a.r * d), expf(-a.g * d), expf(-a.b * d) };
	return c;
}

TraceResult Scene(float x, float y)
{
	TraceResult a = { CircleSDF(x, y, 0.5f, -0.2f, 0.1f), 0.0f, 0.0f,{ 10.0f, 10.0f, 10.0f }, COLOR_BLACK };
	//b��absorption��rgb��(4,4,1),��ʾ��������rg,�����ʾ��������ɫ����ɫ
	TraceResult b = { NgonSDF(x, y, 0.5f, 0.5f, 0.25f, 5.0f), 0.0f, 1.5f, COLOR_BLACK,  { 4.0f, 4.0f, 1.0f } };


	return Union(a, b);
}

TraceResult Union(TraceResult lhs, TraceResult rhs)
{
	return lhs.sdf < rhs.sdf ? lhs : rhs;
}

TraceResult Intersec(TraceResult lhs, TraceResult rhs)
{
	TraceResult r = lhs;
	Color emissive = lhs.sdf > rhs.sdf ? lhs.emissive : rhs.emissive;
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
////�ȶ������ض��ɣ�
//�ȶ�-�ʲ����ɣ�Beer-Lambert law��������Ų�����ɼ��⣩ͨ������ʱ���������ղ��ֵ�Ų���
///��������������ĺ�ȣ���̾��룩�����ʵ�����ϵ������Ũ����ء�

///һ����ɫ��������һ���ս��ʱ��棬��ͨ��һ����ȵĽ��ʺ����ڽ���������һ���ֹ��ܣ�͸����ǿ�Ⱦ�Ҫ������
///���ս��ʵ�Ũ�����󣬽��ʵĺ���������ǿ�ȵļ���������

/// T = expf(-a * c * d)
/// T : ͸��ĳ���������Ĺ�ı���
/// a : ���ʵ�����ϵ��,���
/// c : ���ʵ�Ũ��,���
/// d : ��̾���,���ڽ����еĴ����ľ��� d = eta * distance => ������eta��1
/// ���������ac�ǳ���