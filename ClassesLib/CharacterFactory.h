#pragma once

#include <BaseCharacterModel.h>
#include <Environment.h>

class CharacterFactory
{
private:
	static CharacterFactory * instance;
	CharacterFactory() {}

public:
	static CharacterFactory * getInstance();

	tw::BaseCharacterModel * constructCharacter(tw::Environment * environment, int classId, int teamId, int posX, int posY);

	std::vector<int> getClassesIds()
	{
		std::vector<int> classesIds;

		classesIds.push_back(1);
		classesIds.push_back(2);
		classesIds.push_back(3);
		classesIds.push_back(4);

		return classesIds;
	}
};

