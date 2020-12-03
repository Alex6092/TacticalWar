#pragma once


// TODO : Classe � compl�ter par la team effets de sorts ...

class Effect
{
private:
	int sourceClassId;			// Pour bien diff�rencier les effets des diff�rents sorts de chaque classe
	int sourceSpellNumber;		// (gestion du non cumul des effets).
								// Cela permet de v�rifier de quel sort de quelle classe est issu l'effet.

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

