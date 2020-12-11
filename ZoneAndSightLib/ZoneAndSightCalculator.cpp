#include "pch.h"
#include "ZoneAndSightCalculator.h"
#include <iostream>

using namespace tw;

ZoneAndSightCalculator * ZoneAndSightCalculator::instance = NULL;

ZoneAndSightCalculator::ZoneAndSightCalculator()
{
}


ZoneAndSightCalculator::~ZoneAndSightCalculator()
{
}

ZoneAndSightCalculator * ZoneAndSightCalculator::getInstance()
{
	if (instance == NULL)
		instance = new ZoneAndSightCalculator();

	return instance;
}


std::vector<Point2D> ZoneAndSightCalculator::generateZone(int x, int y, int minPO, int maxPO, TypeZoneLaunch type)
{
	std::vector<Point2D> targettableCells;
	int i;

	if (type == TypeZoneLaunch::LINE){

		for (i = x+minPO+1; i <= x + minPO + maxPO; i++) {
			targettableCells.push_back(Point2D(i, y));
		}
		for (i = x-minPO-1; i >= x - minPO - maxPO; i--) {
			targettableCells.push_back(Point2D(i, y));
		}
		for (i = y+minPO+1; i <= y + minPO + maxPO; i++) {
			targettableCells.push_back(Point2D(x, i));
		}
		for (i = y-minPO-1; i >= y - minPO -maxPO; i--) {
			targettableCells.push_back(Point2D(x, i));
		}
	}

	if (type == TypeZoneLaunch::DIAGONAL) {
		int x1 = x;
		int y1 = y;
		for (i = 0; i <= maxPO; i++) {
			targettableCells.push_back(Point2D(x1, y1));
			x1 = x1 + 1;
			y1 = y1 + 1;
		}
		x1 = x;
		y1 = y;
		for (i = 0; i <= maxPO; i++) {
			targettableCells.push_back(Point2D(x1, y1));
			x1 = x1 - 1;
			y1 = y1 - 1;

		}
		x1 = x;
		y1 = y;
		for (i = 0; i <= maxPO; i++) {
			targettableCells.push_back(Point2D(x1, y1));
			x1 = x1 + 1;
			y1 = y1 - 1;

		}
		x1 = x;
		y1 = y;
		for (i = 0; i <= maxPO; i++) {
			targettableCells.push_back(Point2D(x1, y1));
			x1 = x1 - 1;
			y1 = y1 + 1;

		}
	}

	if (type == TypeZoneLaunch::STAR) {
		int x1, y1;

		for (i = x; i <= x + 2; i++) {
			targettableCells.push_back(Point2D(i, y));
		}
		for (i = x; i >= x - 2; i--) {
			targettableCells.push_back(Point2D(i, y));
		}
		for (i = y; i <= y + 2; i++) {
			targettableCells.push_back(Point2D(x, i));
		}
		for (i = y; i >= y - 2; i--) {
			targettableCells.push_back(Point2D(x, i));
		}

		for (i = 0; i <= 1; i++) {
			targettableCells.push_back(Point2D(x, y));

			x1 = x + 1;
			y1 = y + 1;
			targettableCells.push_back(Point2D(x1, y1));

			x1 = x - 1;
			y1 = y - 1;
			targettableCells.push_back(Point2D(x1, y1));

			x1 = x + 1;
			y1 = y - 1;
			targettableCells.push_back(Point2D(x1, y1));

			x1 = x - 1;
			y1 = y + 1;
			targettableCells.push_back(Point2D(x1, y1));
		}
	}

	return targettableCells;
}

bool ZoneAndSightCalculator::isObstacle(int x, int y, std::vector<Obstacle*> obstacles)
{
	bool bObstacle = false;
	for (int i = 0; i < obstacles.size(); i++)
	{
		if (obstacles[i]->getX() == x && obstacles[i]->getY() == y)
		{
			bObstacle = true;
			break;
		}
	}

	return bObstacle;
}

std::vector<Point2D> ZoneAndSightCalculator::processLineOfSight(int launcherX, int launcherY, std::vector<Point2D> cellsToTest, std::vector<Obstacle*> obstacles)
{
	std::vector<Point2D> targettableCells;

	/*int targetX, targetY = 0;
	int screenSizeX, screenSizeY, a, b, y;
	for (targetX = 0; targetX < screenSizeX; targetX++) {
		a = (targetY - launcherY) / (targetX - launcherX);
		b = launcherY - (launcherX * a);
		if (targetX > launcherX) {
			for (int i = launcherX; i < targetX; i++) {
				y = a * i + b;
				if (this->isObstacle(i,y,obstacles) == true) {
					break;
				}
				else {
					targettableCells.push_back(Point2D(i, y));
				}
			}
		}
		if (targetX < launcherX) {
			for (int i = launcherX; i > targetX; i--) {
				y = a * i + b;
				if (this->isObstacle(i, y, obstacles) == true) {
					break;
				}
				else {
					targettableCells.push_back(Point2D(i, y));
				}
			}
		}
	}
	targetY = screenSizeY;
	for (targetX = 0; targetX < screenSizeX; targetX++) {
		a = (targetY - launcherY) / (targetX - launcherX);
		b = launcherY - (launcherX * a);
		if (targetX > launcherX) {
			for (int i = launcherX; i < targetX; i++) {
				y = a * i + b;
				if (this->isObstacle(i, y, obstacles) == true) {
					break;
				}
				else {
					targettableCells.push_back(Point2D(i, y));
				}
			}
		}
		if (targetX < launcherX) {
			for (int i = launcherX; i > targetX; i--) {
				y = a * i + b;
				if (this->isObstacle(i, y, obstacles) == true) {
					break;
				}
				else {
					targettableCells.push_back(Point2D(i, y));
				}
			}
		}
	}
	targetX = 0;
	for (targetY = 0; targetY < screenSizeY; targetY++) {
		a = (targetY - launcherY) / (targetX - launcherX);
		b = launcherY - (launcherX * a);
		if (targetY > launcherY) {
			for (int i = launcherY; i < targetY; i++) {
				y = a * i + b;
				if (this->isObstacle(i, y, obstacles) == true) {
					break;
				}
				else {
					targettableCells.push_back(Point2D(i, y));
				}
			}
		}
		if (targetY < launcherY) {
			for (int i = launcherY; i > targetY; i--) {
				y = a * i + b;
				if (this->isObstacle(i, y, obstacles) == true) {
					break;
				}
				else {
					targettableCells.push_back(Point2D(i, y));
				}
			}
		}
	}
	targetX = screenSizeX;
	for (targetY = 0; targetY < screenSizeY; targetY++) {
		a = (targetY - launcherY) / (targetX - launcherX);
		b = launcherY - (launcherX * a);
		if (targetY > launcherY) {
			for (int i = launcherY; i < targetY; i++) {
				y = a * i + b;
				if (this->isObstacle(i, y, obstacles) == true) {
					break;
				}
				else {
					targettableCells.push_back(Point2D(i, y));
				}
			}
		}
		if (targetY < launcherY) {
			for (int i = launcherY; i > targetY; i--) {
				y = a * i + b;
				if (this->isObstacle(i, y, obstacles) == true) {
					break;
				}
				else {
					targettableCells.push_back(Point2D(i, y));
				}
			}
		}
	}*/

	for (int i = 0; i < cellsToTest.size(); i++) {
		int targetX = cellsToTest[i].getX();
		int targetY = cellsToTest[i].getY();
		float a = (targetY - launcherY) / (targetX - launcherX);
		float b = launcherY - (launcherX * a);
		int y;

		if (targetX > launcherX) {
			for (int x = launcherX; x < targetX; x++) {
				y = ceil(a * x + b);
				if (this->isObstacle(x, y, obstacles) == true) {
					break;
				}
				else {
					targettableCells.push_back(Point2D(x, y));
				}
			}
		}
		else if (targetX < launcherX) {
			for (int x = launcherX; x > targetX; x--) {
				y = ceil(a * x + b);
				if (this->isObstacle(x, y, obstacles) == true) {
					break;
				}
				else {
					targettableCells.push_back(Point2D(x, y));
				}
			}
		}
		 else if (targetX == launcherX) {
			if (targetY > launcherY) {
				for (int y = launcherY; y < targetY; y++) {
					if (this->isObstacle(targetX, y, obstacles) == true) {
						break;
					}
					else {
						targettableCells.push_back(Point2D(targetX, y));
					}
				}
			}
			else if (targetY < launcherY) {
				for (int y = launcherY; y > targetY; y--) {
					if (this->isObstacle(targetX, y, obstacles) == true) {
						break;
					}
					else {
						targettableCells.push_back(Point2D(targetX, y));
					}
				}
			}
		}
	}

	return targettableCells;
}