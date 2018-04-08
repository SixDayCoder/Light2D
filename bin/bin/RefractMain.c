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


#define RAY_MARCHING_MAX_STEP     (32)
#define RAY_MARCHING_MAX_DISTANCE (3.0f)
#define RAY_MAX_TRACE_STEP    (2)
#define RAY_BIAS (1e-4f)

#define COLOR_WHITE (0.0f)
#define COLOR_BLACK (255.0f)

#define REFRACT (1)  //����
#define TOTAL_REFLECT (0) //ȫ����

typedef unsigned char byte;
typedef struct { float sdf, emissive, reflectivity, eta; } TraceResult;

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

	FILE* fp = fopen("..//..//png//basic_refract.png", "wb");
	svpng(fp, WIDTH, HEIGHT, image, 0);
	printf("Svnpng Success\n");
	return 0;
}

float Lighting(float x, float y)
{
	float sum = 0.0f;
	for (int i = 0; i < LIGHT_COUNT; ++i)
	{
		float radians = TWO_PI * (i + (float)rand() / RAND_MAX) / LIGHT_COUNT;   // ��������
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

float Trace(float ox, float oy, float dx, float dy, int depth)
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
			float sum = r.emissive;
			//SDF�õ��ǿɷ�����߿������,����Trace�ĵݹ������Ҫ��ķ�Χ��
			if (depth < RAY_MAX_TRACE_STEP && ( (r.reflectivity > 0.0f) || (r.eta > 0.0f) ) )
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
						sum += (1.0f - reflect) * Trace(x - nx * RAY_BIAS, y - ny * RAY_BIAS, rx, ry, depth + 1);
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
					sum += reflect * Trace(x + nx * RAY_BIAS, y + ny * RAY_BIAS, rx, ry, depth + 1);

				}

			}
			return sum;
		}

		//���߲������ǹ�������״�ڻ�����״��
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

TraceResult Scene(float x, float y)
{
	x = fabsf(x - 0.5f) + 0.5f;

	TraceResult a = { CapsuleSDF(x, y, 0.75f, 0.25f, 0.75f, 0.75f, 0.05f), 0.0f, 0.2f, 1.5f };

	TraceResult b = { CapsuleSDF(x, y, 0.75f, 0.25f, 0.50f, 0.75f, 0.05f), 0.0f, 0.2f, 1.5f };

	y = fabsf(y - 0.5f) + 0.5f;

	TraceResult c = { CircleSDF(x, y, 1.05f, 1.05f, 0.05f), 5.0f, 0.0f, 0.0f };

	return Union(a, Union(b, c));
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
////���䶨��:˹��������
////n1 * sin(theta1) = n2 * sin(theta2),���n1 > n2,������Ƕȵ����ٽ��,�ᷢ��ȫ����, 
//
////����ͷ����Trace
////��֮ǰ������ʱ,������һ��(x,y)�������Ǽ������ķ��������������״���ⲿ��,
////ʵ���ϼ�������֮��,��������䶼���ܷ�������״���ڲ�,��������Ӧ���ȸ���SDF�ж�(x,y)������״���ڲ������ⲿ
//
////������ߴ��ⲿ�����ڲ�,��������ʱ���� 1.0f / eta,������eta
////�����eta(yita) = ����������ڵĽ��ʵ������� / ����������ڵĽ��ʵ�������
