#include "pch.h"
#include "ZoneAndSightCalculator.h"
#include <iostream>
#include <TypeZoneLaunch.h>

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

		for (i = x+minPO; i <= x + minPO + ( maxPO - minPO); i++) {
			targettableCells.push_back(Point2D(i, y));
		}
		for (i = x-minPO; i >= x - minPO - ( maxPO - minPO); i--) {
			targettableCells.push_back(Point2D(i, y));
		}
		for (i = y+minPO; i <= y + minPO + (maxPO - minPO); i++) {
			targettableCells.push_back(Point2D(x, i));
		}
		for (i = y-minPO; i >= y - minPO - (maxPO - minPO); i--) {
			targettableCells.push_back(Point2D(x, i));
		}
	}

	if (type == TypeZoneLaunch::DIAGONAL) {
		int x1 = x + minPO;
		int y1 = y + minPO;
		for (i = 0; i <= (maxPO - minPO); i++) {
			targettableCells.push_back(Point2D(x1, y1));
			x1 = x1 + 1;
			y1 = y1 + 1;
		}
		x1 = x - minPO;
		y1 = y - minPO;
		for (i = 0; i <= (maxPO - minPO); i++) {
			targettableCells.push_back(Point2D(x1, y1));
			x1 = x1 - 1;
			y1 = y1 - 1;

		}
		x1 = x + minPO;
		y1 = y - minPO;
		for (i = 0; i <= (maxPO - minPO); i++) {
			targettableCells.push_back(Point2D(x1, y1));
			x1 = x1 + 1;
			y1 = y1 - 1;

		}
		x1 = x - minPO;
		y1 = y + minPO;
		for (i = 0; i <= (maxPO - minPO); i++) {
			targettableCells.push_back(Point2D(x1, y1));
			x1 = x1 - 1;
			y1 = y1 + 1;

		}
	}

	if (type == TypeZoneLaunch::STAR) {
		for (i = x + minPO; i <= x + minPO + (maxPO - minPO); i++) {
			targettableCells.push_back(Point2D(i, y));
		}
		for (i = x - minPO; i >= x - minPO - (maxPO - minPO); i--) {
			targettableCells.push_back(Point2D(i, y));
		}
		for (i = y + minPO; i <= y + minPO + (maxPO - minPO); i++) {
			targettableCells.push_back(Point2D(x, i));
		}
		for (i = y - minPO; i >= y - minPO - (maxPO - minPO); i--) {
			targettableCells.push_back(Point2D(x, i));
		}

		int x1 = x + minPO;
		int y1 = y + minPO;
		for (i = 0; i <= (maxPO - minPO); i++) {
			targettableCells.push_back(Point2D(x1, y1));
			x1 = x1 + 1;
			y1 = y1 + 1;
		}
		x1 = x - minPO;
		y1 = y - minPO;
		for (i = 0; i <= (maxPO - minPO); i++) {
			targettableCells.push_back(Point2D(x1, y1));
			x1 = x1 - 1;
			y1 = y1 - 1;

		}
		x1 = x + minPO;
		y1 = y - minPO;
		for (i = 0; i <= (maxPO - minPO); i++) {
			targettableCells.push_back(Point2D(x1, y1));
			x1 = x1 + 1;
			y1 = y1 - 1;

		}
		x1 = x - minPO;
		y1 = y + minPO;
		for (i = 0; i <= (maxPO - minPO); i++) {
			targettableCells.push_back(Point2D(x1, y1));
			x1 = x1 - 1;
			y1 = y1 + 1;

		}
	}
	if (type == TypeZoneLaunch::NORMAL) {
		/*for (i = y + minPO + 1; i < y + minPO + maxPO + 1; i++) {
			for (int j = x - minPO - maxPO; j < x + minPO + maxPO + 1; j++) {
				targettableCells.push_back(Point2D(j, i));
			}
		}
		for (i = y - minPO - 1; i > y - minPO - maxPO - 1; i--) {
			for (int j = x - minPO - maxPO; j < x + minPO + maxPO + 1; j++) {
				targettableCells.push_back(Point2D(j, i));
			}
		}
		for (i = x + minPO + 1; i < x + minPO + maxPO + 1; i++) {
			for (int j = y - minPO - maxPO; j < y + minPO + maxPO; j++) {
				targettableCells.push_back(Point2D(i, j));
			}
		}
		for (i = x - minPO - 1; i > x - minPO - maxPO - 1; i--) {
			for (int j = y - minPO - maxPO; j < y + minPO + maxPO + 1; j++) {
				targettableCells.push_back(Point2D(i, j));
			}
		}




		int j = x, k = x;
		for (i = y + minPO; i > y - minPO; i--) {
			if (i >= y) {
				if (j < x + minPO) {
					targettableCells.push_back(Point2D(j, i));
					j++;
				}
				if (k > x - minPO) {
					targettableCells.push_back(Point2D(k, i));
					k--;
				}
			}
			if (i < y) {
				if (j > x) {
					targettableCells.push_back(Point2D(j, i));
					j--;
				}
				if (k < x) {
					targettableCells.push_back(Point2D(k, i));
					k++;
				}
			}
		}*/
		/*int j = y;
		int k = y;
		for (i = x - minPO; i < x; i++) {
			targettableCells.push_back(Point2D(i, j));
			targettableCells.push_back(Point2D(i, k));
			j++;
			k--;
		}
		for (i = x; i <= x + minPO; i++) {
			targettableCells.push_back(Point2D(i, j));
			targettableCells.push_back(Point2D(i, k));
			j--;
			k++;
		}*/
		for (int l = 0; l <= maxPO - minPO; l++) {
			int j = y;
			int k = y;
			for (i = x - minPO - l; i < x; i++) {
				targettableCells.push_back(Point2D(i, j));
				targettableCells.push_back(Point2D(i, k));
				j++;
				k--;
			}
			for (i = x; i <= x + minPO + l; i++) {
				targettableCells.push_back(Point2D(i, j));
				targettableCells.push_back(Point2D(i, k));
				j--;
				k++;
			}
		}
		
	}

	return targettableCells;
}

int ZoneAndSightCalculator::isObstacle(int x, int y, std::vector<Obstacle> obstacles)
{
	int bObstacle = 0;
	for (int i = 0; i < obstacles.size(); i++)
	{
		if (obstacles[i].getX() == x && obstacles[i].getY() == y)
		{
			if (obstacles[i].getIsTargettable() == true) {
				bObstacle = 2;
			}
			else {
				bObstacle = 1;
			}
			break;
		}
	}

	return bObstacle;
}

std::vector<Point2D> ZoneAndSightCalculator::processLineOfSight(int launcherX, int launcherY, std::vector<Point2D> cellsToTest, std::vector<Obstacle> obstacles)
{
	std::vector<Point2D> targettableCells;

	//for (int i = 0; i < cellsToTest.size(); i++) {
	//	int targetX = cellsToTest[i].getX();
	//	int targetY = cellsToTest[i].getY();
	//	double a, b;
	//	if (targetX != launcherX) {
	//		a = (double)(targetY - launcherY) / (double)(targetX - launcherX);
	//		b = launcherY - (launcherX * a);
	//	}
	//	else {
	//		a = 0;
	//		b = 0;
	//	}
	//	int y, y1, x;
	//	if (targetX > launcherX) {
	//		for (int x = launcherX; x <= targetX; x++) {
	//			/*if (a > -0.5 && a < 0.5) {
	//				y = floor(a * x + b);
	//				if (this->isObstacle(x, y, obstacles) == 1) {
	//					break;
	//				}
	//				else if (this->isObstacle(x, y, obstacles) == 2) {
	//					if(x == targetX && y == targetY)
	//						targettableCells.push_back(Point2D(x, y));

	//					break;
	//				}
	//				else if (x == targetX && y == targetY) {
	//					targettableCells.push_back(Point2D(x, y));
	//				}
	//			}
	//			else {*/
	//				y = ceil(a * (x+1) + b);
	//				y1 = floor(a * x + b);

	//				int nbCases = y > y1 ? y - y1 : y1 - y;
	//				int o = 0;
	//				do
	//				{
	//					y = y1 + o;
	//					if (this->isObstacle(x, y, obstacles) == 1) {
	//						break;
	//					}
	//					else if (this->isObstacle(x, y, obstacles) == 2) {
	//						if (x == targetX && y == targetY)
	//							targettableCells.push_back(Point2D(x, y));
	//						break;
	//					}
	//					else if (x == targetX && y == targetY) {
	//						targettableCells.push_back(Point2D(x, y));
	//					}
	//					o++;
	//				} while (o < nbCases);

	//				/*
	//				if (this->isObstacle(x, y1, obstacles) == 1) {
	//					break;
	//				}
	//				else if (this->isObstacle(x, y1, obstacles) == 2) {
	//					if (x == targetX && y1 == targetY)
	//						targettableCells.push_back(Point2D(x, y1));
	//					break;
	//				}
	//				else if (x == targetX && y1 == targetY) {
	//					targettableCells.push_back(Point2D(x, y1));
	//				}
	//				
	//			}*/
	//		}
	//	}
	//	else if (targetX < launcherX) {
	//		for (int x = launcherX; x >= targetX; x--) {
	//			/*if (a > -0.5 && a < 0.5) {
	//				y = floor(a * x + b);
	//				if (this->isObstacle(x, y, obstacles) == 1) {
	//					break;
	//				}
	//				else if (this->isObstacle(x, y, obstacles) == 2) {
	//					if (x == targetX && y == targetY)
	//						targettableCells.push_back(Point2D(x, y));
	//					break;
	//				}
	//				else if(x == targetX && y == targetY){
	//					targettableCells.push_back(Point2D(x, y));
	//				}
	//			}
	//			else {*/
	//				y = ceil(a * (x-1) + b);
	//				y1 = floor(a * x + b);
	//				int nbCases = y > y1 ? y - y1 : y1 - y;
	//				int o = 0;
	//				do
	//				{
	//					y = y1 + o;
	//					if (this->isObstacle(x, y, obstacles) == 1) {
	//						break;
	//					}
	//					else if (this->isObstacle(x, y, obstacles) == 2) {
	//						if (x == targetX && y == targetY)
	//							targettableCells.push_back(Point2D(x, y));
	//						break;
	//					}
	//					else if (x == targetX && y == targetY) {
	//						targettableCells.push_back(Point2D(x, y));
	//					}
	//					o++;
	//				} while (o < nbCases);
	//				/*if (this->isObstacle(x, y1, obstacles) == 1) {
	//					break;
	//				}
	//				else if (this->isObstacle(x, y1, obstacles) == 2) {
	//					if (x == targetX && y1 == targetY)
	//						targettableCells.push_back(Point2D(x, y1));
	//					break;
	//				}
	//				else if (x == targetX && y1 == targetY) {
	//					targettableCells.push_back(Point2D(x, y1));
	//				}
	//			}*/
	//		}
	//	}
	//	else if (targetX == launcherX) {
	//		if (targetY > launcherY) {
	//			for (int y = launcherY; y <= targetY; y++) {
	//				if (this->isObstacle(targetX, y, obstacles) == 1) {
	//					break;
	//				}
	//				else if (this->isObstacle(targetX, y, obstacles) == 2) {
	//					if (y == targetY)
	//						targettableCells.push_back(Point2D(targetX, y));
	//					break;
	//				}
	//				else if (y == targetY) {
	//					targettableCells.push_back(Point2D(targetX, y));
	//				}
	//			} 
	//		}
	//		else if (targetY < launcherY) {
	//			for (int y = launcherY; y >= targetY; y--) {
	//				if (this->isObstacle(targetX, y, obstacles) == 1) {
	//					break;
	//				}
	//				else if (this->isObstacle(targetX, y, obstacles) == 2) {
	//					if (y == targetY)
	//						targettableCells.push_back(Point2D(targetX, y));
	//					break;
	//				}
	//				else if (y == targetY) {
	//					targettableCells.push_back(Point2D(targetX, y));
	//				}
	//			}
	//		}
	//	}
	//}

	for (int i = 0; i < cellsToTest.size(); i++)
	{
		Point2D cell = cellsToTest[i];
		bool bCellIsTargettable = true;

		// On peut se cibler soit même :
		if (cell.getX() == launcherX && cell.getY() == launcherY)
		{

		}
		// Cas simplifié : ciblage en ligne
		else if (cell.getX() == launcherX || cell.getY() == launcherY)
		{
			// Cible dans la ligne
			if (cell.getX() == launcherX)
			{
				for (int i = 0; i < obstacles.size(); i++)
				{
					Obstacle o = obstacles[i];

					// Si l'obstacle n'est pas nous même :
					if (o.getX() != launcherX || o.getY() != launcherY)
					{
						// Si c'est un obstacle sur la ligne :
						if (o.getX() == launcherX)
						{
							// Si l'obstacle est la cible :
							if (o.getY() == cell.getY())
							{
								// Si l'obstacle n'est pas ciblable :
								if (!o.getIsTargettable())
								{
									bCellIsTargettable = false;
								}
							}
							else
							{
								// L'obstacle n'est pas la cible.
								// L'obstacle se trouve t'il entre le lanceur et la cible ?
								if (launcherY < cell.getY())
								{
									// L'obstacle se trouve entre le lanceur et la cible
									// launcherY < pos.y < cell.y
									if (o.getY() > launcherY && o.getY() < cell.getY())
									{
										bCellIsTargettable = false;
									}
								}
								else
								{
									// L'obstacle se trouve entre le lanceur et la cible
									// cell.y < pos.y < launcherY
									if (o.getY() > cell.getY() && o.getY() < launcherY)
									{
										bCellIsTargettable = false;
									}
								}
							}
						}
					}

					// On s'arrête au premier obstacle bloquant rencontré :
					if (!bCellIsTargettable)
						break;
				}
			}
			else // (cell.y == launcherY) est vrai => ciblage en colonne
			{
				for (int i = 0; i < obstacles.size(); i++)
				{
					Obstacle o = obstacles[i];

					// Si l'obstacle n'est pas nous même :
					if (o.getX() != launcherX || o.getY() != launcherY)
					{
						// Si c'est un obstacle sur la colonne :
						if (o.getY() == launcherY)
						{
							// Si l'obstacle est la cible :
							if (o.getX() == cell.getX())
							{
								// Si l'obstacle n'est pas ciblable :
								if (!o.getIsTargettable())
								{
									bCellIsTargettable = false;
								}
							}
							else
							{
								// L'obstacle n'est pas la cible.
								// L'obstacle se trouve t'il entre le lanceur et la cible ?
								if (launcherX < cell.getX())
								{
									// L'obstacle se trouve entre le lanceur et la cible
									// launcherX < pos.x < cell.x
									if (o.getX() > launcherX && o.getX() < cell.getX())
									{
										bCellIsTargettable = false;
									}
								}
								else
								{
									// L'obstacle se trouve entre le lanceur et la cible
									// cell.x < pos.x < launcherX
									if (o.getX() > cell.getX() && o.getX() < launcherX)
									{
										bCellIsTargettable = false;
									}
								}
							}
						}
					}

					// On s'arrête au premier obstacle bloquant rencontré :
					if (!bCellIsTargettable)
						break;
				}
			}
		}
		// Cas général
		else
		{
			// Calcul du vecteur Lanceur/Cible :
			Point2D LTVector(cell.getX() - launcherX, cell.getY() - launcherY);
			double coeffDir = (double)LTVector.getY() / (double)LTVector.getX();
			double p = (double)(launcherY + 0.5) - (double)(coeffDir * (double)(launcherX + 0.5));

			for (int i = 0; i < obstacles.size(); i++)
			{
				Obstacle o = obstacles[i];

				if (o.getX() == launcherX && o.getY() == launcherY)
				{

				}
				else if (o.getX() == cell.getX() && o.getY() == cell.getY())
				{
					if (!o.getIsTargettable())
					{
						bCellIsTargettable = false;
						break;
					}
				}
				else
				{
					double yMin = (coeffDir * (double)o.getX()) + p;
					double yMax = (coeffDir * (double)(o.getX() + 1)) + p;

					if (yMin > yMax)
					{
						double tmp = yMin;
						yMin = yMax;
						yMax = tmp;
					}

					if ((yMin >= o.getY() && yMin <= (o.getY() + 1))
						||
						(yMax >= o.getY() && yMax <= (o.getY() + 1))
						||
						(yMin <= o.getY() && yMax >= (o.getY() + 1)))
					{
						// Les vecteurs se croisent, on vérifie que l'obstacle
						// est bien entre le lanceur et la cible :
						// Test sur x :
						if (
							(cell.getX() >= launcherX && (o.getX() >= launcherX && o.getX() <= cell.getX()))
							||
							(cell.getX() <= launcherX && (o.getX() >= cell.getX() && o.getX() <= launcherX))
							)
						{
							// Test sur y :
							if (
								(cell.getY() >= launcherY && (o.getY() >= launcherY && o.getY() <= cell.getY()))
								||
								(cell.getY() <= launcherY && (o.getY() >= cell.getY() && o.getY() <= launcherY))
								)
							{
								// Test des diagos :
								if ((yMax == o.getY() && yMin < o.getY())
									||
									(yMin == (o.getY() + 1) && yMax > (o.getY() + 1)))
								{
									// Ciblable
								}
								else
								{
									// L'obstacle est entre le lanceur et la cible, la case n'est pas ciblable.
									bCellIsTargettable = false;
									break;
								}
							}
						}
					}
				}
			}
		}

		if (bCellIsTargettable)
		{
			targettableCells.push_back(cell);
		}
	}
	return targettableCells;
}