#pragma once

#include <string>
#include <GameData.h>

// Données de jeu (classes, sorts, règles) utilisées par le client. Elles sont envoyées par
// le serveur à la connexion (message GD) ; le fichier local sert de secours.
class ClientGameData
{
public:
	static ClientGameData & get();

	const tw::battle::GameData & data();
	bool loadFromServer(const std::string & json);

	const tw::battle::ClassDef * findClass(int classId) { return data().findClass(classId); }

private:
	ClientGameData() : loaded(false) {}

	tw::battle::GameData gameData;
	bool loaded;
};
