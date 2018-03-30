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
typedef struct{ float sdf, emissive;} TraceResult;

byte image[WIDTH * HEIGHT * RGB];

//һ�����������ж��CSG
TraceResult Scene(float x, float y);

TraceResult UnionResult(TraceResult lhs, TraceResult rhs);

TraceResult IntersecResult(TraceResult lhs, TraceResult rhs);

TraceResult SubtractResult(TraceResult lhs, TraceResult rhs);

float Lighting(float x, float y);

float CircleSDF(float x, float y, float cx, float cy, float radius);

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

	FILE* fp = fopen("..//..//png//A_Intersec_B.png", "wb");
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
	TraceResult r1 = { CircleSDF(x, y, 0.3f, 0.5f, 0.20f),  1.0f };
	TraceResult r2 = { CircleSDF(x, y, 0.4f, 0.5f, 0.20f),  0.8f };

	//return UnionResult(r1, r2);//A_Union_B.png
	return IntersecResult(r1, r2);//A_Intersec_B.png
	//return SubtractResult(r1, r2);//A_Sub_B.png
	//return SubtractResult(r2, r1);//B_Sub_A.png

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
//��SDF�Ľ����������Ľ���
//���� SDF ����ʾ��״�������Լ򵥵�ʵ�ֹ���ʵ�弸�Σ�constructive solid geometry������������״�㼯�Ĳ�����������ʾģ��
//CSG : ����ʵ�弸��
//���Զ�SDF�Ľ�����ʵ���Ͼ��ǶԵ㼯���н�����

//��������
//1.�����൱�ڰѶ����״�����ֳ���������ABC������״����������ֳ�����ͼ��ΪD, ��ô���һ������D�ڣ�������������A������B������C����ô����㵽
//ABC��3��SDF,ȡ3����SDF��Сֵ��,�����Сֵ��<=0��˵���õ���A��B��C��,Ҳ������������D��


//��������
//2.��������˼�����������״�Ĺ������֣�����AB������ֳ�����ͼ��ΪC����ô���һ������C�ڣ�����������ͬʱ��A�ں�B�ڣ�����㵽
//AB��SDF����������SDF�����ֵ��������ֵ<=0˵���õ����A��Ҳ��B��,Ҳ������������C��


//��������?