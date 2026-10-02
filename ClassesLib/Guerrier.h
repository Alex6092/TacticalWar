#pragma once

#include <BaseCharacterModel.h>

// Guerrier : personnage affiché par le client. Les caractéristiques et les sorts de la classe sont
// définis dans assets/data/gamedata.json et appliqués par le serveur.
class Guerrier : public tw::BaseCharacterModel
{
public:
	Guerrier(tw::Environment * environment, int teamId, int currentX, int currentY)
		: BaseCharacterModel(environment, teamId, currentX, currentY)
	{
	}

	virtual int getClassId()
	{
		return 4;
	}

	virtual std::string getGraphicsPath()
	{
		return "./assets/Warrior/";
	}
};
