#pragma once
namespace tw
{
	class Point2D
	{
		int x;
		int y;

	public:
		Point2D(int x, int y);
		//Point2D(Point2D &p);
		Point2D();
		~Point2D();

		inline int getX() const {
			return x;
		}

		inline int getY() const {
			return y;
		}

		inline void setX(int x) {
			this->x = x;
		}

		inline void setY(int y)
		{
			this->y = y;
		}

		bool operator==(Point2D &p) const
		{
			return (x == p.getX() && y == p.getY());
		}

		bool operator!=(Point2D &p) const
		{
			return !((*this) == p);
		}
	};
}