#ifndef E1M1_H_H_
#define E1M1_H_H_

#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))

const int e1m1Vertices[8] __attribute__((aligned(4)))=
{
	//x,y
	256, 224,
	-192, 224,
	-192, -128,
	256, -128
};
const u16 e1m1VerticesLength = NELEMS(e1m1Vertices);

const u16 e1m1Segs[8] __attribute__((aligned(4)))=
{
	//vertexIndex0,vertexIndex1
	2,1,
	3,2,
	0,3,
	1,0
};
const u16 e1m1SegsLength = NELEMS(e1m1Segs);

const int e1m1Things[10] __attribute__((aligned(4)))=
{
	// 0x,1y,2angle,3type,4flags
	32, 160, 270, 1, 7,
	-96, -64, 270, 3004, 7
};

const u16 e1m1ThingsLength = NELEMS(e1m1Things);

#endif