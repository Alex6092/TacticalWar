#pragma once


// TODO : Classe à compléter par la team effets de sorts ...

class Effect
{
private:
	int sourceClassId;			// Pour bien différencier les effets des différents sorts de chaque classe
	int sourceSpellNumber;		// (gestion du non cumul des effets).
								// Cela permet de vérifier de quel sort de quelle classe est issu l'effet.

public:
	Effect(int srcClassId, int srcSpellNumber)
	{
		this->sourceClassId = srcClassId;
		this->sourceSpellNumber = srcSpellNumber;
	}

	int getSourceClassId()
	{
		return sourceClassId;
	}

	int getSourceSpellNumber()
	{
		return sourceSpellNumber;
	}
};

