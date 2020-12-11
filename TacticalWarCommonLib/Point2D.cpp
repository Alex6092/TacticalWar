#include "pch.h"
#include "Point2D.h"

using namespace tw;

Point2D::Point2D(int x, int y)
{
	this->x = x;
	this->y = y;
}

/*
Point2D::Point2D(Point2D &p)
{
	this->x = p.x;
	this->y = p.y;
}
*/

Point2D::Point2D()
{
	x = 0;
	y = 0;
}

Point2D::~Point2D()
{
}
