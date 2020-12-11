#pragma once
#include "Point2D.h"
#include "CellData.h"
#include "BaseCharacterModel.h"

namespace tw
{
	class Obstacle : public Point2D
	{
		bool isTargettable;

	public:
		Obstacle(CellData * cell);
		Obstacle(BaseCharacterModel * character);
		~Obstacle();

		inline bool getIsTargettable() {
			return isTargettable;
		}
	};
}