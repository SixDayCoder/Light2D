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


//�ⶨ����Ϊ(x,y)�ĵ㱻���������ɫֵ
float Lighting(float x, float y);


//�����(x,y)���뷢��Բ��Circle(cx,cy,radius)�ķ��ž��볡
//1.SDF < 0 ���ڳ�����״��,�����������״�߽����ΪSDF
//2.SDF = 0 ���ڳ�����״��
//3.SDF > 0 ���ڳ�����״��
float CircleSDF(float x, float y, float cx, float cy, float radius); 


//�򵥹���׷��
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

//�����(ox,oy)��(dx,dy)�����Ͻ��ܵĹ��ն�
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
//�ڼ������ʱ
//����ͼ�񳤿��Ƕ���,ͨ�����Կ�Ⱥ͸߶ȣ���x,y=>ӳ�䵽(0,1) X (0, 1)��Ȼ��ѵõ��Ľ����255.0fӳ�䵽{0,1,...,255}
//�൱������һ������任,�任�����տռ�
//��¼����Բ����(0.5,0.5),����뾶Ϊ0.1,����ʵ���ù��տռ�������
//�ô�����,�ڹ��տռ����Traceʱ,RAY_MARCHING_MAX_DISTANCE��������һ���������(����1�ͺ�)�����㲽��������ͬʱ,���ܵõ��ȽϿ�������ٶ�
//��Ȼ,��Ҳ���԰�����������PNG������ϵ��,��ʱ�����ķ���Բ�̵�λ��Ӧ�þ�����PNG��������