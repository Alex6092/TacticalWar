#pragma once

#include <BaseCharacterModel.h>

// Mage : personnage affiché par le client. Les caractéristiques et les sorts de la classe sont
// définis dans assets/data/gamedata.json et appliqués par le serveur.
class Mage : public tw::BaseCharacterModel
{
public:
	Mage(tw::Environment * environment, int teamId, int currentX, int currentY)
		: BaseCharacterModel(environment, teamId, currentX, currentY)
	{
	}

	virtual int getClassId()
	{
		return 1;
	}

	virtual std::string getGraphicsPath()
	{
		return "./assets/Mage/";
	}
};
